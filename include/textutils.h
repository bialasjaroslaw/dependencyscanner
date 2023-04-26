#pragma once

#include <string>

#include "dependency_extractor_export.h"

namespace Text {
DEPENDENCY_EXTRACTOR_EXPORT std::string ltrim(const std::string& s);
DEPENDENCY_EXTRACTOR_EXPORT std::string rtrim(const std::string& s);
DEPENDENCY_EXTRACTOR_EXPORT std::string trim(const std::string& s);
} // namespace Text