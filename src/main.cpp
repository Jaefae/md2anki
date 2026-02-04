#include "cli.h"
#include <iostream>
#define debug

int main(int argc, char *argv[]) {
  Cfg cfg{};
  bool success = cfg.fromArgs(argc, argv);
  if (!success) {
    std::cout << "[EXIT] Could not build config." << std::endl;
    return 1;
  }
  std::cout << "[INFO] Configuration built." << std::endl;
  return 0;
}
