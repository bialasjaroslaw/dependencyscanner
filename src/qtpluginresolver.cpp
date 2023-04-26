#include <fstream>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include <filesystem>
#include <fmt/format.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "androiddependencyextractor.h"

#include <CLI/CLI.hpp>

std::unordered_map<std::string, std::string> ARCH_MAPPING{{"armeabi-v7a", "arm-linux-androideabi"},
                                                          {"arm64-v8a", "aarch64-linux-android"},
                                                          {"x86", "i686-linux-android"},
                                                          {"x86_64", "x86_64-linux-android"}};

int main(int argc, char* argv[])
{
    CLI::App app{"Android app dependency check"};
    std::string appDirectory;
    std::string appName;
    std::string ndkPath;
    std::string jsonFile;
    std::string toolchainPrefix = "llvm";
    std::string ndkHost = "linux-x86_64";
    bool deployToAppDirectory = false;
    std::string qt;
    app.add_option("-d,--directory", appDirectory, "Build directory with subdirs (armeabi/arm64...)")
        ->check(CLI::ExistingDirectory);
    app.add_option("-a,--app", appName, "Application name");
    app.add_option("-n,--ndk", ndkPath, "Android NDK")->check(CLI::ExistingDirectory);
    app.add_option("-q,--qt", qt, "Qt install directory")->check(CLI::ExistingDirectory);
    app.add_option("-j,--json", jsonFile, "JSON with configuration")->check(CLI::ExistingFile);
    app.add_flag("-c,--deploy", deployToAppDirectory, "Wheather to deploy missing libs");
    CLI11_PARSE(app, argc, argv);

    if (std::filesystem::exists(jsonFile))
    {
        using json = nlohmann::json;
        std::ifstream file(jsonFile);
        json data = json::parse(file);
        if (!data.is_object())
        {
            spdlog::error("JSON config {} is not an object", jsonFile);
            return 1;
        }

        if (data.contains("ndk"))
            ndkPath = data["ndk"].get<std::string>();
        if (data.contains("application-binary"))
            appName = data["application-binary"].get<std::string>();
        if (data.contains("toolchain-prefix"))
            toolchainPrefix = data["toolchain-prefix"].get<std::string>();
        if (data.contains("ndk-host"))
            ndkHost = data["ndk-host"].get<std::string>();
        if (data.contains("qt"))
            qt = data["qt"].get<std::string>();
    }

    spdlog::set_level(spdlog::level::debug);
    bool checkStatus = true;
    
    std::map<std::string, std::string> PLUGINS_INFO;

    for (const auto& arch : ARCH_MAPPING)
    {
        auto appDir = fmt::format("{}/{}", appDirectory, arch.first);
        auto appPath = fmt::format("{}/lib{}_{}.so", appDir, appName, arch.first);
        auto qtLibDir = fmt::format("{}/lib", qt);
        if (!std::filesystem::exists(appPath))
        {
            spdlog::warn("Skipping arch {}, file {} does not exists", arch.first, appPath);
            continue;
        }

        SharedLibrary appMainLib{.name = appName, .path = appPath};
        AndroidDependencyExtractor extractor(
            AndroidDependencyExtractor::getToolPath(ndkPath, toolchainPrefix, ndkHost));
        ExtractorOptions options{.libraryDirs = {appDir}, .scanDirs = {qtLibDir}, .systemDirs = {}};
        options.preloadInfo(".so");

        spdlog::info("Checking dependencies for architecture {}", arch.first);
        auto result = extractor.resolveDependencies(appMainLib, options);

        auto qtLibBaseName = [](const std::string& fullName) {
            auto it = std::find_if_not(fullName.cbegin(), fullName.cend(),
                                       [](auto character) { return std::isalnum(character); });
            constexpr auto prefixLength = 3; // lib
            return it == fullName.cend()
                       ? fullName
                       : fullName.substr(prefixLength, std::distance(fullName.cbegin(), it) - prefixLength);
        };

        auto readPluginInfo = [](const std::string& path) -> std::string {
            std::string pluginSubPath;
            std::ifstream file(path);
            if (!file.is_open())
                return pluginSubPath;
            std::string line;
            while (std::getline(file, line))
            {
                if (!line.starts_with("_populate_"))
                    continue;

                auto idx = line.find("RELEASE \"");
                if (idx == std::string::npos)
                    break;

                auto startIdx = idx + 9;
                auto endIdx = line.find("\"", startIdx);

                if (endIdx == std::string::npos || endIdx <= startIdx)
                    break;

                pluginSubPath = line.substr(startIdx, endIdx - startIdx);
                break;
            }
            file.close();
            return pluginSubPath;
        };

        auto qtPlugins = [qt](const std::string& libraryBaseName) -> std::set<std::string> {
            std::set<std::string> pluginNamesInfo;
            auto directory = fmt::format("{}/lib/cmake/{}", qt, libraryBaseName);
            if (!std::filesystem::exists(directory))
                return pluginNamesInfo;

            for (const auto& item : std::filesystem::directory_iterator(directory))
            {
                const auto& ext = item.path().extension().string();
                const auto& name = item.path().stem().string();
                if (ext == ".cmake" && name.starts_with(libraryBaseName) && name.ends_with("Plugin"))
                {
                    auto prefixSize = libraryBaseName.length() + 1;
                    auto length = name.length() - 6 - prefixSize;
                    if (length <= 0)
                        continue;
                    pluginNamesInfo.insert(item.path());
                }
            }
            return pluginNamesInfo;
        };

        auto replaceArch = [](const std::string& subpath, const std::string& arch) -> std::string {
            auto idx = subpath.find("${ANDROID_ABI}");
            if (idx == std::string::npos)
                return {};
            auto result = subpath;
            result.replace(idx, 14, arch);
            return result;
        };
        
        auto fullPluginPath = [qt](const std::string& subpath){
            return fmt::format("{}/plugins/{}", qt, subpath);
        };

        for (const auto& val : result.resolved)
        {
            if (!val.first.starts_with("libQt5"))
                continue;
            auto baseName = qtLibBaseName(val.second.name);
            auto allPlugins = qtPlugins(baseName);
            if(allPlugins.empty())
            {
                spdlog::debug("No plugins for {} => {}", baseName, val.second.path);
                continue;
            }
            
            spdlog::debug("Plugins for {} => {}", baseName, val.second.path);
            for (const auto& plugin : allPlugins)
            {
                std::string info;
            
                if(!PLUGINS_INFO.contains(plugin))
                {
                    info = readPluginInfo(plugin);
                    PLUGINS_INFO[plugin] = info;
                }
                else
                    info = PLUGINS_INFO[plugin];
                auto subpath = replaceArch(info, arch.first);
                auto fullPath = fullPluginPath(subpath);
                spdlog::debug("===> {}", fullPath);
                if(deployToAppDirectory)
                {
                    spdlog::debug("Copy {} => {}", fullPath, appDir);
                    // SKIP for now
//                    std::filesystem::copy_file(fullPath, appDir);
                }
            }
        }
    }

    if (!checkStatus)
        spdlog::error("Check failed, missing at least one library!");

    return 0;
}
