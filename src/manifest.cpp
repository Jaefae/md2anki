#include "manifest.h"

#include <fstream>

namespace fs = std::filesystem;

fs::path manifestPath(const fs::path& inputPath) {
  if (fs::is_directory(inputPath)) {
    return inputPath / ".md2anki-ids";
  }
  return inputPath.parent_path() / ".md2anki-ids";
}

Manifest readManifest(const fs::path& path) {
  Manifest      manifest;
  std::ifstream ifile(path);
  if (!ifile.is_open()) return manifest;

  std::getline(ifile, manifest.source);
  std::string line;
  while (std::getline(ifile, line)) {
    if (!line.empty()) manifest.ids.insert(line);
  }
  return manifest;
}

void writeManifest(const fs::path& path, const Manifest& manifest) {
  std::ofstream ofile(path);
  ofile << manifest.source << '\n';
  for (const auto& id : manifest.ids) {
    ofile << id << '\n';
  }
}

std::vector<std::string> staleIds(const Manifest&              previous,
                                   const std::set<std::string>& current) {
  std::vector<std::string> stale;
  for (const auto& id : previous.ids) {
    if (!current.contains(id)) stale.push_back(id);
  }
  return stale;
}
