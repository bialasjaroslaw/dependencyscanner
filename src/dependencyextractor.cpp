#include "dependencyextractor.h"

#include <filesystem>
#include <fmt/format.h>

void ExtractorOptions::preloadInfo(const std::string& libraryExtension)
{
    libraryDirsFiles = scanDirectories(libraryDirs, libraryExtension);
    scanDirsFiles = scanDirectories(scanDirs, libraryExtension);
    systemDirsFiles = scanDirectories(systemDirs, libraryExtension);
}

std::vector<std::string> ExtractorOptions::scanDirectory(const std::string& path, const std::string& libraryExtension)
{
    std::vector<std::string> ret;
    for (const auto& item : std::filesystem::directory_iterator(path))
        if (item.path().extension() == libraryExtension)
            ret.push_back(item.path().filename());
    return ret;
}

std::unordered_map<std::string, std::string> ExtractorOptions::scanDirectories(const std::set<std::string>& paths,
                                                                               const std::string& libraryExtension)
{
    std::unordered_map<std::string, std::string> ret;
    for (const auto& path : paths)
        for (const auto& name : scanDirectory(path, libraryExtension))
            ret.insert({name, fmt::format("{}/{}", path, name)});
    return ret;
}