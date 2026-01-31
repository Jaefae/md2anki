#pragma once
#include <filesystem>

struct Cfg{
  std::filesystem::path inputPath;
  std::filesystem::path outputPath;
  bool strictWarn = false;
  bool deckAssigned = false;
  bool headerMode = false;
  bool fromArgs(const int argc, char* argv[]);
};
