#pragma once
#include <string>

#define debug

struct Cfg{
  std::string fileName;
  bool strictWarn = false;
  bool deckAssigned = false;
  bool headerMode = false;
};

Cfg generateCfg(const int argc, char* argv[]);

