#include "cli.h"
#include <filesystem>
#include <iostream>
#include <iomanip>
#include <string>
#include <system_error>
#define debug

namespace fs = std::filesystem;

/// Checks basic file path validity and extension equality.
bool validPath(const fs::path& file, const std::string_view& extension){
  if(file.has_filename() && file.has_extension() && file.extension() == extension){
    return true;
  }
  return false;
}
/// Returns true upon building config, false on error.
bool Cfg::fromArgs(const int argc, char *argv[]) {
  const std::string usage = "Usage: md2anki [inputPath] -o [outputPath] [additionalFlags]";
  Cfg cfg;

  // Atleast 4 arguments: [exe] [inputPath] -o [outputPath]
  if (argc < 4) {
    std::cout << "[ERROR] Not enough arguments. " << usage << std::endl;
    return false;
  }

  // Validate input path
  if(!validPath(argv[1], ".md") || !fs::exists(argv[1])){
    std::cout << "[ERROR] Input file is invalid or doesn't exist, expected .md (" << argv[1] << ")" << std::endl;
    return false;
  }
  this->inputPath = argv[1];

  // Validate output path
  if(std::string(argv[2]) != "-o" || !validPath(argv[3], ".csv")){
    std::cout << "[ERROR] Output path invalid, expected .csv (" << argv[3] << ")" << std::endl;
    return false;
  }
  this->outputPath = argv[3];

  // Collect remaining flags (-s --strict -h --header)
  for(int i = 4; i < argc; i++){
    const std::string_view arg = argv[i];
    bool knownArg = false;
    if(arg == "-s" || arg == "--strict") this->strictWarn = true;
    else {
      std::cout << "[ERROR] Unknown argument. " << usage << std::endl;
      std::cout << "[INFO] Optional arguments: " << std::endl
                << "\t-s, --strict" << std::setw(20) << ": Stop compilation on error instead of skipping." << std::endl;
      return false;
    }
  }
  // Return true for successful cfg on no error.
  return true;
}
