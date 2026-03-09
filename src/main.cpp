#include "cli.h"
#include <iostream>
#include "parser.h"
#define debug

int main(int argc, char *argv[]) {
  Cfg cfg{};
  bool success = cfg.fromArgs(argc, argv);
  if (!success) {
    std::cout << "[EXIT] Could not build config." << std::endl;
    return 1;
  }
  std::cout << "[INFO] Configuration built." << std::endl;
  ParseResult res = parseFile(cfg);
  for (auto& error: res.errors) {
    std::cout << "[Warn] " << error.message <<
      " (line " << error.lineNumber << ")" << std::endl;
  }
  if (saveFile(cfg, res)) {
    std::cout << "[INFO] Cards output to " << absolute(cfg.outputPath).string() << ".\n";
    return 0;
  } else {
    if (cfg.strictWarn) {
      std::cout << "[ERROR] Cards not saved due to listed errors. (strict warn mode)" << std::endl;
    }
    std::cout << "[ERROR] Cards could not be saved to " << absolute(cfg.outputPath).string() << ".\n";
    return 1;
  }
}
