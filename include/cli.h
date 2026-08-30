#pragma once
#include <filesystem>
#include <string>

bool validPath(const std::filesystem::path& file, const std::string_view& extension);
struct Cfg{
  std::filesystem::path inputPath;
  std::filesystem::path outputPath;
  bool strictWarn = false;
  bool writeIds = false;
  bool ankiConnect = false;
  std::string ankiConnectUrl = "http://127.0.0.1:8765";
  bool fromArgs(const int argc, char* argv[]);
};
