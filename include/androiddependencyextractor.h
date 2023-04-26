#pragma once

#include "dependencyextractor.h"

#include "dependency_extractor_export.h"

class DEPENDENCY_EXTRACTOR_EXPORT AndroidDependencyExtractor : public DependencyExtractor
{
public:
    AndroidDependencyExtractor(const std::string& tool);

    void scanDependencies(SharedLibrary& target) override;
    ResolveResult resolveDependencies(SharedLibrary& target, const ExtractorOptions& options) override;

    static std::string getToolPath(const std::string& ndkPath, const std::string& toolchainPrefix,
                                   const std::string& ndkHost);
    static std::string platformPath(const std::string& ndkPath, const std::string& toolchainPrefix,
                                    const std::string& ndkHost, const std::string& arch, int platform);

private:
    std::string toolPath;
};
