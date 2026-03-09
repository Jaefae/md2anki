#pragma once
#include <filesystem>

bool validPath(const std::filesystem::path& file, const std::string_view& extension);
struct Cfg{
  std::filesystem::path inputPath;
  std::filesystem::path outputPath;
  bool strictWarn = false;
  bool fromArgs(const int argc, char* argv[]);
};
