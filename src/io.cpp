#include "io.h"

#include <algorithm>
#include <fstream>
#include <iterator>

#include "card.h"
#include "util.h"

namespace fs = std::filesystem;

std::vector<fs::path> collectMarkdownFiles(const fs::path& path) {
  std::vector<fs::path> files;
  if (!fs::is_directory(path)) {
    files.emplace_back(path);
    return files;
  }
  for (const auto& entry : fs::recursive_directory_iterator(path)) {
    if (entry.is_regular_file() &&
        toLower(entry.path().extension().string()) == ".md") {
      files.emplace_back(entry.path());
    }
  }
  // Directory iteration order is unspecified; sort so output is reproducible.
  std::sort(files.begin(), files.end());
  return files;
}

bool readTextFile(const fs::path& path, std::string& out) {
  std::ifstream ifile(path);
  if (!ifile.is_open()) return false;
  out.assign(std::istreambuf_iterator<char>(ifile),
             std::istreambuf_iterator<char>());
  return true;
}

ParseResult parseFiles(const Cfg& cfg) {
  ParseResult res;
  for (const auto& file : collectMarkdownFiles(cfg.inputPath)) {
    std::string text;
    if (!readTextFile(file, text)) {
      res.errors.push_back({file, 0, "Could not open file " + file.string()});
      continue;
    }
    ParseResult fileRes = parseMarkdown(text, file);
    res.cards.insert(res.cards.end(),
                     std::make_move_iterator(fileRes.cards.begin()),
                     std::make_move_iterator(fileRes.cards.end()));
    res.errors.insert(res.errors.end(),
                      std::make_move_iterator(fileRes.errors.begin()),
                      std::make_move_iterator(fileRes.errors.end()));
  }
  return res;
}

bool saveFile(const Cfg& cfg, ParseResult& res) {
  if (cfg.strictWarn && !res.errors.empty()) {
    return false;
  }
  std::ofstream ofile(cfg.outputPath);
  if (!ofile.is_open()) return false;
  ofile << toCsvDocument(res.cards);
  return true;
}
