#include "textutils.h"

const std::string WHITESPACE = " \n\r\t\f\v";

std::string Text::ltrim(const std::string& s)
{
    size_t start = s.find_first_not_of(WHITESPACE);
    return (start == std::string::npos) ? std::string() : s.substr(start);
}

std::string Text::rtrim(const std::string& s)
{
    size_t end = s.find_last_not_of(WHITESPACE);
    return (end == std::string::npos) ? std::string() : s.substr(0, end + 1);
}

std::string Text::trim(const std::string& s)
{
    return rtrim(ltrim(s));
}
