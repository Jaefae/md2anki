#include "cli.h"
#include <filesystem>
#include <iostream>
#include <string>
#include <system_error>
#define debug

namespace fs = std::filesystem;

/// Returns true upon building config, false on error.
bool Cfg::fromArgs(const int argc, char *argv[]) {
  // Validate input path
  if (argc < 4) {
    std::cout << "[ERROR] You must supply input/output paths." << std::endl 
              <<"\t[EXAMPLE] md2anki [inputPath] -o [outputPath]"<< std::endl;
    return false;
  } else {
    fs::path inPath = argv[1];
    std::error_code ec;
    bool malformed = true;

    // Check path/file existence
    if (!fs::exists(inPath, ec)) {
      std::cout << "[ERROR] File path does not exist. (" << inPath << ")"
                << std::endl;
      return false;
    }
    // Check path extensions/dir
    if (inPath.has_extension() && inPath.extension() == ".md" ||
        fs::is_directory(inPath, ec))
      malformed = false;
    if (malformed) {
      std::cout << "[ERROR] Expected .md or directory, got " << inPath
                << std::endl;
      return false;
    } else {
      this->inputPath = inPath.string();
    }
  }

  // Collect Flags
  for (int i = 2; i < argc; i++) {
    std::string_view arg = argv[i];
    if (arg == "-s" || arg == "--strict")
      this->strictWarn = true;
    else if (arg == "-h" || arg == "--header")
      this->headerMode = true;

    // Validate output path.
    else if (arg == "-o") {
      if (i == argc)
        std::cout << "[ERROR] You must supply an output path!" << std::endl;
      else if (fs::path outPath = argv[i + 1];
               !outPath.has_filename() || !(outPath.extension() == ".csv")) {
        std::cout << "[ERROR] Expected an output path ending with .csv, got "
                  << outPath << " instead." << std::endl;
        return false;
      } else {
        this->outputPath = outPath;
      }
    // Failure states (Uknown args, no outpath, no inpath)
    } else {
      bool isPath = (arg.ends_with(".csv") || arg.ends_with(".md"));
      if(!isPath){
        std::cout << "[ERROR] Unknown argument: " << arg << std::endl;
        return false;
      } else if(this->outputPath == ""){
        std::cout << "[ERROR] No output path provided." 
          << "(md2anki [inputPath] -o [outputPath])" << std::endl;
        return false;
      } else if(this->inputPath == ""){
        std::cout << "[ERROR] No input path provided." 
          << "(md2anki [inputPath] -o [outputPath])" << std::endl;
      }
    }
  }
#ifdef debug
  if (strictWarn)
    std::cout << "[INFO] Strict warning mode enabled! (Compilation will stop "
                 "on error)"
              << std::endl;
  if (headerMode)
    std::cout << "[INFO] Header mode enabled. (See GitHub README.md)"
              << std::endl;
#endif
  return true;
}
