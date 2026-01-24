#include "cli.h"
#include <iostream>
#define debug

int main(int argc, char *argv[]) {
  Cfg cfg{};
  bool res = cfg.fromArgs(argc, argv);
#ifdef debug
  if (res) {
    std::cout << "Configuration built." << std::endl;
#endif
  } else {
    std::cout << "[EXIT] Could not build config." << std::endl;
  }
  return 0;
}
