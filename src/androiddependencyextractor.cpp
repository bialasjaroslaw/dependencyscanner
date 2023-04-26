#include "androiddependencyextractor.h"

#include <filesystem>
#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include "textutils.h"


static std::string findInPath(const std::set<std::string>& directories, const std::string& fileName)
{
    for (const auto& prefix : directories)
        if (auto path = fmt::format("{}/{}", prefix, fileName); std::filesystem::exists(path))
            return path;
    return std::string();
}

AndroidDependencyExtractor::AndroidDependencyExtractor(const std::string& tool)
{
    toolPath = tool;
    if (!std::filesystem::exists(toolPath))
    {
        spdlog::error("Command does not exist: {}", toolPath);
        toolPath.clear();
    }
}

void AndroidDependencyExtractor::scanDependencies(SharedLibrary& target)
{
    auto readCmd = fmt::format("{} --needed-libs {}", toolPath, target.path);

    FILE* readProcess = popen(readCmd.c_str(), "r");
    if (!readProcess)
    {
        spdlog::error("Cannot execute process {}", readCmd);
        return;
    }

    bool readLibs = false;
    char buffer[1024];
    while (fgets(buffer, sizeof(buffer), readProcess) != nullptr)
    {
        auto line = std::string(buffer, strlen(buffer));
        line = Text::trim(line);
        if (!readLibs)
        {
            readLibs = line.starts_with("NeededLibraries");
            continue;
        }
        if (!line.starts_with("lib"))
            continue;
        target.dependencies.insert(line);
    }
    target.scanned = true;
    pclose(readProcess);
}

ResolveResult AndroidDependencyExtractor::resolveDependencies(SharedLibrary& target,
                                                                                     const ExtractorOptions& options)
{
    SharedLibrary lib = target;
    SharedLibraries libsToCopy;
    SharedLibraries libsToCheck;
    SharedLibraries checkedLibs;
    SharedLibraries resolvedLibs;
    SharedLibraries unmetLibs;

    auto inProgress = [&](const auto& library) {
        return libsToCheck.contains(library) || resolvedLibs.contains(library) || checkedLibs.contains(library) ||
               unmetLibs.contains(library);
    };

    while (!lib.name.empty())
    {
        scanDependencies(lib);
        resolvedLibs.insert({lib.name, lib});
        for (const auto& library : lib.dependencies)
            if (!inProgress(library))
                libsToCheck.insert({library, SharedLibrary{.name = library}});
        lib = {};

        while (!libsToCheck.empty())
        {
            auto dependency = libsToCheck.begin();
            auto markAsChecked = [&](const std::string& path, bool skipAnalysis = false, bool shouldBeCopied = false) {
                auto modified = *dependency;
                modified.second.path = path;

                if (skipAnalysis)
                    resolvedLibs.insert(modified);
                else
                    checkedLibs.insert(modified);

                if (shouldBeCopied)
                    libsToCopy.insert(modified);

                libsToCheck.erase(libsToCheck.begin());
            };

            if (options.libraryDirsFiles.contains(dependency->first))
                markAsChecked(options.libraryDirsFiles.at(dependency->first), false, false);
            else if (options.scanDirsFiles.contains(dependency->first))
                markAsChecked(options.scanDirsFiles.at(dependency->first), false, true);
            else if (options.systemDirsFiles.contains(dependency->first))
                markAsChecked(options.systemDirsFiles.at(dependency->first), true, false);
            else
                unmetLibs.insert(libsToCheck.extract(libsToCheck.begin()));
        }

        if (!checkedLibs.empty())
        {
            lib = checkedLibs.begin()->second;
            checkedLibs.extract(checkedLibs.begin());
        }
    }
    return {.resolved = resolvedLibs, .availableForCopy = libsToCopy, .unmet = unmetLibs};
}

std::string AndroidDependencyExtractor::getToolPath(const std::string& ndkPath, const std::string& toolchainPrefix,
                                             const std::string& ndkHost)
{
    return fmt::format("{}/toolchains/{}/prebuilt/{}/bin/llvm-readobj", ndkPath, toolchainPrefix, ndkHost);
}

std::string AndroidDependencyExtractor::platformPath(const std::string& ndkPath, const std::string& toolchainPrefix,
                                              const std::string& ndkHost, const std::string& arch, int platform)
{
    return fmt::format("{}/toolchains/{}/prebuilt/{}/sysroot/usr/lib/{}/{}", ndkPath, toolchainPrefix, ndkHost, arch,
                       platform);
}
