#pragma once

#include <string>
#include <set>
#include <unordered_map>

using Dependencies = std::set<std::string>;

struct SharedLibrary
{
    std::string name;
    std::string path;
    Dependencies dependencies;
    bool scanned = false;
};

using SharedLibraries = std::unordered_map<std::string, SharedLibrary>;
