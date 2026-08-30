#include "cli.h"

#include <filesystem>
#include <iomanip>
#include <iostream>
#include <string>
#include <system_error>

#include "util.h"
#define debug

namespace fs = std::filesystem;

/// Checks basic file path validity and extension equality.
bool validPath(const fs::path& file, const std::string_view& extension) {
  if (file.has_filename() && file.has_extension() &&
      toLower(file.extension().string()) == extension) {
    return true;
  }
  return false;
}
namespace {
void printUsage(const std::string& usage) {
  std::cout << "[INFO] Optional arguments: " << std::endl
            << "\t-o outputPath" << std::setw(15)
            << ": Destination .csv file. Optional if --anki-connect is given."
            << std::endl
            << "\t--anki-connect" << std::setw(14)
            << ": Push cards directly to a running Anki instance via the "
               "AnkiConnect add-on (default http://127.0.0.1:8765), "
               "instead of or alongside -o. Implies --write-ids."
            << std::endl
            << "\t--anki-connect-url url" << std::setw(6)
            << ": Override the AnkiConnect URL used by --anki-connect."
            << std::endl
            << "\t-s, --strict" << std::setw(16)
            << ": Stop compilation on error instead of skipping."
            << std::endl
            << "\t--write-ids" << std::setw(17)
            << ": Assign ids to cards missing one and write them back "
               "to source (implies --strict)."
            << std::endl;
}
}  // namespace

/// Returns true upon building config, false on error.
bool Cfg::fromArgs(const int argc, char* argv[]) {
  const std::string usage =
      "Usage: md2anki [inputPath] [-o outputPath] [--anki-connect] "
      "[additionalFlags]";

  if (argc < 2) {
    std::cout << "[ERROR] Not enough arguments. " << usage << std::endl;
    return false;
  }

  // Validate input path
  if (!fs::exists(argv[1])) {
    std::cout << "[ERROR] Input path is invalid or doesn't exist (" << argv[1]
              << ")" << std::endl;
    return false;
  }
  this->inputPath = argv[1];

  // Collect remaining flags (-o, --anki-connect, -s/--strict, --write-ids, ...)
  for (int i = 2; i < argc; i++) {
    const std::string_view arg = argv[i];
    if (arg == "-o") {
      if (i + 1 >= argc || !validPath(argv[i + 1], ".csv")) {
        std::cout << "[ERROR] Output path invalid, expected .csv" << std::endl;
        return false;
      }
      this->outputPath = argv[++i];
    } else if (arg == "--anki-connect") {
      this->ankiConnect = true;
    } else if (arg == "--anki-connect-url") {
      if (i + 1 >= argc) {
        std::cout << "[ERROR] --anki-connect-url requires a value." << std::endl;
        return false;
      }
      this->ankiConnectUrl = argv[++i];
    } else if (arg == "-s" || arg == "--strict") {
      this->strictWarn = true;
    } else if (arg == "--write-ids") {
      this->writeIds   = true;
      this->strictWarn = true;
    } else {
      std::cout << "[ERROR] Unknown argument. " << usage << std::endl;
      printUsage(usage);
      return false;
    }
  }

  if (this->outputPath.empty() && !this->ankiConnect) {
    std::cout << "[ERROR] Must provide -o outputPath and/or --anki-connect. "
              << usage << std::endl;
    return false;
  }
  if (this->ankiConnect) {
    this->writeIds   = true;
    this->strictWarn = true;
  }

  // Return true for successful cfg on no error.
  return true;
}
