#include "manifest.h"
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace {
void writeFile(const fs::path& path, const std::string& contents) {
  std::ofstream ofile(path);
  ofile << contents;
}
}  // namespace

TEST_CASE("manifestPath sits under a directory input", "[manifest]") {
  fs::path dir = fs::temp_directory_path() / "md2anki_manifestpath_dir";
  fs::create_directories(dir);

  CHECK(manifestPath(dir) == dir / ".md2anki-ids");
  fs::remove_all(dir);
}

TEST_CASE("manifestPath sits next to a file input", "[manifest]") {
  fs::path path = fs::temp_directory_path() / "md2anki_manifestpath.md";
  writeFile(path, "");

  CHECK(manifestPath(path) == path.parent_path() / ".md2anki-ids");
  fs::remove(path);
}

TEST_CASE("readManifest returns an empty manifest for a missing file",
          "[manifest]") {
  Manifest manifest =
      readManifest(fs::temp_directory_path() / "md2anki_no_such_manifest");
  CHECK(manifest.source.empty());
  CHECK(manifest.ids.empty());
}

TEST_CASE("writeManifest then readManifest round-trips source and ids",
          "[manifest]") {
  fs::path path = fs::temp_directory_path() / "md2anki_manifest_roundtrip";
  Manifest written{"vault/", {"aaaaaaaa", "bbbbbbbb"}};

  writeManifest(path, written);
  Manifest read = readManifest(path);
  fs::remove(path);

  CHECK(read.source == "vault/");
  CHECK(read.ids == written.ids);
}

TEST_CASE("staleIds returns ids missing from the current set", "[manifest]") {
  Manifest previous{"vault/", {"aaaaaaaa", "bbbbbbbb", "cccccccc"}};
  std::set<std::string> current{"bbbbbbbb"};

  CHECK(staleIds(previous, current) ==
        std::vector<std::string>{"aaaaaaaa", "cccccccc"});
}

TEST_CASE("staleIds is empty when nothing was removed", "[manifest]") {
  Manifest previous{"vault/", {"aaaaaaaa"}};
  std::set<std::string> current{"aaaaaaaa", "bbbbbbbb"};

  CHECK(staleIds(previous, current).empty());
}
