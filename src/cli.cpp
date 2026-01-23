#pragma once
#include "cli.h"
#include <iostream>
#define debug

Cfg generateCfg(const int argc, char* argv[]){
  Cfg cfg{};

  for(int i = 0; i < argc; i++){
    std::string_view arg = argv[i];
    if(arg == "-s" || arg == "--strict"){
      cfg.strictWarn = true;
      #ifdef debug
      std::cout << "[INFO] Strict warning mode enabled! (Compilation will stop on error)." << std::endl;
      #endif
    } else if (arg == "-h" || arg == "--header") {
      cfg.headerMode = true;
      #ifdef debug
      std::cout << "[INFO] Header mode enabled. (See README.md on github)." << std::endl;
      #endif
    }
  }
  return cfg;
}
