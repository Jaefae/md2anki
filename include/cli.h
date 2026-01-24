#pragma once
#include <string>

#define debug

struct Cfg{
  std::string inputPath;
  std::string outputPath;
  bool strictWarn = false;
  bool deckAssigned = false;
  bool headerMode = false;
  bool fromArgs(const int argc, char* argv[]);
};
