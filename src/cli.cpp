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
/// Returns true upon building config, false on error.
bool Cfg::fromArgs(const int argc, char* argv[]) {
  const std::string usage =
      "Usage: md2anki [inputPath] -o [outputPath] [additionalFlags]";
  Cfg cfg;

  // Atleast 4 arguments: [exe] [inputPath] -o [outputPath]
  if (argc < 4) {
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

  // Validate output path
  if (std::string(argv[2]) != "-o" || !validPath(argv[3], ".csv")) {
    std::cout << "[ERROR] Output path invalid, expected .csv (" << argv[3]
              << ")" << std::endl;
    return false;
  }
  this->outputPath = argv[3];

  // Collect remaining flags (-s --strict -h --header)
  for (int i = 4; i < argc; i++) {
    const std::string_view arg      = argv[i];
    bool                   knownArg = false;
    if (arg == "-s" || arg == "--strict")
      this->strictWarn = true;
    else if (arg == "--write-ids") {
      this->writeIds   = true;
      this->strictWarn = true;
    } else {
      std::cout << "[ERROR] Unknown argument. " << usage << std::endl;
      std::cout << "[INFO] Optional arguments: " << std::endl
                << "\t-s, --strict" << std::setw(20)
                << ": Stop compilation on error instead of skipping."
                << std::endl
                << "\t--write-ids" << std::setw(21)
                << ": Assign ids to cards missing one and write them back "
                   "to source (implies --strict)."
                << std::endl;
      return false;
    }
  }
  // Return true for successful cfg on no error.
  return true;
}
