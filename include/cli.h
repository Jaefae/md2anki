#include <string>

#define debug

struct Cfg{
  bool strictWarn = false;
  bool deckAssigned = false;
  bool headerMode = false;
  std::string fileName;
};

Cfg generateCfg(const int argc, char* argv[]);

