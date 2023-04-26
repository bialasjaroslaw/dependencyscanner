#pragma once

#include "dependency.h"

#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "dependency_extractor_export.h"

struct ExtractorOptions
{
    std::set<std::string> libraryDirs;
    std::set<std::string> scanDirs;
    std::set<std::string> systemDirs;

    std::unordered_map<std::string, std::string> libraryDirsFiles;
    std::unordered_map<std::string, std::string> scanDirsFiles;
    std::unordered_map<std::string, std::string> systemDirsFiles;

    void preloadInfo(const std::string& libraryExtension);
    std::vector<std::string> scanDirectory(const std::string& path, const std::string& libraryExtension);
    std::unordered_map<std::string, std::string> scanDirectories(const std::set<std::string>& paths,
                                                                 const std::string& libraryExtension);
};

struct ResolveResult
{
    SharedLibraries resolved;
    SharedLibraries availableForCopy;
    SharedLibraries unmet;
};

class DEPENDENCY_EXTRACTOR_EXPORT DependencyExtractor
{
public:
    DependencyExtractor() = default;
    virtual void scanDependencies(SharedLibrary& target) = 0;
    virtual ResolveResult resolveDependencies(SharedLibrary& target, const ExtractorOptions& options) = 0;
};
