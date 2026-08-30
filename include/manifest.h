#pragma once
#include <filesystem>
#include <set>
#include <string>
#include <vector>

struct Manifest {
  std::string            source;  ///< literal inputPath this manifest was built from
  std::set<std::string>  ids;     ///< sorted: stable file order, no git-diff churn
};

/// Directory inputPath -> inputPath/.md2anki-ids. File inputPath ->
/// alongside it, in its parent directory.
std::filesystem::path manifestPath(const std::filesystem::path& inputPath);

/// Missing file reads as an empty, source-less manifest.
Manifest readManifest(const std::filesystem::path& path);
void     writeManifest(const std::filesystem::path& path, const Manifest& manifest);

/// Ids present in `previous` but not in `current`.
std::vector<std::string> staleIds(const Manifest&              previous,
                                   const std::set<std::string>& current);
