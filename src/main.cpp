#include <iostream>
#include <set>

#include "ankiconnect.h"
#include "cli.h"
#include "io.h"
#include "manifest.h"
#define debug

int main(int argc, char* argv[]) {
  Cfg  cfg{};
  bool success = cfg.fromArgs(argc, argv);
  if (!success) {
    std::cout << "[EXIT] Could not build config." << std::endl;
    return 1;
  }
  std::cout << "[INFO] Configuration built." << std::endl;
  ParseResult res = parseFiles(cfg);
  for (auto& error : res.errors) {
    std::cout << "[WARN] " << error.file.string() << ":" << error.lineNumber
              << " : " << error.message << std::endl;
  }

  std::filesystem::path manifestFile = manifestPath(cfg.inputPath);
  Manifest              manifest     = readManifest(manifestFile);
  Manifest              previous     = manifest;

  if (cfg.writeIds && res.errors.empty()) {
    manifest.source = cfg.inputPath.string();
    if (!applyWriteIds(res, manifest, manifestFile)) {
      std::cout << "[ERROR] Could not write ids back to source files."
                << std::endl;
      return 1;
    }
  }

  std::set<std::string> currentIds;
  for (const auto& card : res.cards) {
    if (!card.id.empty()) currentIds.insert(card.id);
  }

  bool sourceMismatch = !sameSource(previous.source, cfg.inputPath);
  if (sourceMismatch) {
    std::cout << "[WARN] " << manifestFile.string() << " was built from '"
              << previous.source << "', not '" << cfg.inputPath.string()
              << "' -- skipping stale id check." << std::endl;
  }

  bool overallOk = true;

  if (cfg.ankiConnect) {
    if (cfg.strictWarn && !res.errors.empty()) {
      std::cout
          << "[ERROR] Cards not synced to Anki due to listed errors. (strict "
             "warn mode)"
          << std::endl;
      overallOk = false;
    } else {
      Manifest ghostBaseline = sourceMismatch ? Manifest{} : previous;
      overallOk &= syncToAnkiConnect(res, ghostBaseline, manifest, manifestFile,
                                      cfg.ankiConnectUrl);
    }
  } else if (!sourceMismatch) {
    for (const auto& id : staleIds(previous, currentIds)) {
      std::cout << "[WARN] Card " << id
                << " no longer found in source -- remove it from Anki "
                   "manually."
                << std::endl;
    }
  }

  if (!cfg.outputPath.empty()) {
    if (saveFile(cfg, res)) {
      std::cout << "[INFO] Cards output to "
                << absolute(cfg.outputPath).string() << ".\n";
    } else {
      overallOk = false;
      if (cfg.strictWarn) {
        std::cout << "[ERROR] Cards not saved due to listed errors. (strict "
                     "warn mode)"
                  << std::endl;
      }
      std::cout << "[ERROR] Cards could not be saved to "
                << absolute(cfg.outputPath).string() << ".\n";
    }
  }

  return overallOk ? 0 : 1;
}
