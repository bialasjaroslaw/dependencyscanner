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
    std::string deployDir;
    std::string appName;
    std::string ndkPath;
    std::string jsonFile;
    std::string toolchainPrefix = "llvm";
    std::string ndkHost = "linux-x86_64";
    std::set<std::string> extraDirs;
    std::set<std::string> libDirs;
    int platform = 0;
    bool fixLibs = false;
    std::string qt;
    app.add_option("-d,--directory", appDirectory, "Build directory with subdirs (armeabi/arm64...)")
        ->check(CLI::ExistingDirectory);
    app.add_option("-a,--app", appName, "Application name");
    app.add_option("-n,--ndk", ndkPath, "Android NDK")->check(CLI::ExistingDirectory);
    app.add_option("-p,--platform", platform, "Android target platform")->check(CLI::PositiveNumber);
    app.add_option("-q,--qt", qt, "Qt install directory")->check(CLI::ExistingDirectory);
    app.add_option("-j,--json", jsonFile, "JSON with configuration")->check(CLI::ExistingFile);
    app.add_option("-e,--extradirs", extraDirs, "Extra dirs (, separated)use during app libs load")
        ->check(CLI::ExistingDirectory);
    app.add_option("-l,--libdirs", libDirs, "Directories where missing libs might be found (, separated)")
        ->check(CLI::ExistingDirectory);
    app.add_flag("-f,--fix", fixLibs, "Try to fix missing libs");
    app.add_option("-c,--deploy", deployDir, "Where to deploy missing libs")->check(CLI::ExistingDirectory);
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
        auto platformPath =
            AndroidDependencyExtractor::platformPath(ndkPath, toolchainPrefix, ndkHost, arch.second, platform);
        ExtractorOptions options{.libraryDirs = {appDir}, .scanDirs = {qtLibDir}, .systemDirs = {platformPath}};
        options.preloadInfo(".so");

        if (!std::filesystem::exists(platformPath))
        {
            spdlog::error("Directory {} does not exist", platformPath);
            return 1;
        }

        spdlog::info("Checking dependencies for architecture {}", arch.first);
        auto result = extractor.resolveDependencies(appMainLib, options);
        if (!result.unmet.empty())
        {
            checkStatus = false;
            break;
        }

        if (fixLibs && !result.availableForCopy.empty())
        {
            auto deployTo = deployDir.empty() ? appDir : deployDir;
            for (const auto& lib : result.availableForCopy)
            {
                spdlog::debug("Copy {} -> {}", lib.second.path, deployTo);
                std::filesystem::copy(lib.second.path, deployTo);
            }
        }
    }

    if (!checkStatus)
        spdlog::error("Check failed, missing at least one library!");

    return 0;
}
