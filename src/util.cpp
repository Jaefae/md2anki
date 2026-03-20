#include "util.h"
#include <cctype>

std::string_view ltrim(std::string_view input) {
  size_t start = 0;
  while (start < input.size() && std::isspace(static_cast<unsigned char>(input[start]))) {
    ++start;
  }
  return input.substr(start);
}

std::string_view rtrim(std::string_view input) {
  if (input.empty()) {
    return input;
  }
  size_t end = input.size();
  while (end > 0 && std::isspace(static_cast<unsigned char>(input[end - 1]))) {
    --end;
  }
  return input.substr(0, end);
}
// Performs a ltrim, then rtrim on the text object
std::string_view trim(std::string_view input) {
  return rtrim(ltrim(input));
}

std::string toLower(std::string_view input) {
  std::string lower;
  lower.reserve(input.size());
  for (const unsigned char c : input) {
    lower += static_cast<char>(std::tolower(c));
  }
  return lower;
}
