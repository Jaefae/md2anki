#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>

#include "cli.h"
#include "io.h"

namespace fs = std::filesystem;

namespace {
void writeFile(const fs::path& path, const std::string& contents) {
  std::ofstream ofile(path);
  ofile << contents;
}

std::string readBack(const fs::path& path) {
  std::ifstream ifile(path);
  return {std::istreambuf_iterator<char>(ifile),
          std::istreambuf_iterator<char>()};
}
}  // namespace

TEST_CASE("collectMarkdownFiles walks a directory tree for .md files",
          "[io]") {
  fs::path root = fs::temp_directory_path() / "md2anki_collect_root";
  fs::remove_all(root);
  fs::create_directories(root / "nested");
  writeFile(root / "top.md", "");
  writeFile(root / "nested" / "nested.md", "");
  writeFile(root / "notes.txt", "");

  std::vector<fs::path> files = collectMarkdownFiles(root);
  fs::remove_all(root);

  REQUIRE(files.size() == 2);
  CHECK(std::is_sorted(files.begin(), files.end()));
  CHECK(std::find(files.begin(), files.end(), root / "nested" / "nested.md") !=
        files.end());
  CHECK(std::find(files.begin(), files.end(), root / "top.md") != files.end());
}

TEST_CASE("collectMarkdownFiles returns a single file unchanged", "[io]") {
  fs::path path = fs::temp_directory_path() / "md2anki_collect_single.md";

  std::vector<fs::path> files = collectMarkdownFiles(path);

  REQUIRE(files.size() == 1);
  CHECK(files[0] == path);
}

TEST_CASE("readTextFile reads contents and reports missing files", "[io]") {
  fs::path path = fs::temp_directory_path() / "md2anki_readtextfile.md";
  writeFile(path, "Q: question\nA: answer\n");

  std::string contents;
  REQUIRE(readTextFile(path, contents));
  CHECK(contents == "Q: question\nA: answer\n");
  fs::remove(path);

  std::string untouched = "untouched";
  CHECK_FALSE(readTextFile(path, untouched));
  CHECK(untouched == "untouched");
}

TEST_CASE("parseFiles recursively collects .md files from a directory tree",
          "[io]") {
  fs::path root = fs::temp_directory_path() / "md2anki_dirmode_root";
  fs::remove_all(root);
  fs::create_directories(root / "nested");

  writeFile(root / "top.md",
            "#deck: TopDeck\n"
            "Q: top question\n"
            "A: top answer\n");
  writeFile(root / "nested" / "nested.md",
            "#deck: NestedDeck\n"
            "Q: nested question\n"
            "A: nested answer\n");
  // Non-.md files should be ignored.
  writeFile(root / "notes.txt",
            "Q: should not be parsed\n"
            "A: ignored\n");

  Cfg cfg{};
  cfg.inputPath = root;

  ParseResult res = parseFiles(cfg);
  fs::remove_all(root);

  REQUIRE(res.errors.empty());
  REQUIRE(res.cards.size() == 2);

  std::vector<std::string> decks{res.cards[0].deck, res.cards[1].deck};
  CHECK(std::find(decks.begin(), decks.end(), "TopDeck") != decks.end());
  CHECK(std::find(decks.begin(), decks.end(), "NestedDeck") != decks.end());
}

TEST_CASE("parseFiles keeps deck/tag context independent per file", "[io]") {
  fs::path root = fs::temp_directory_path() / "md2anki_dirmode_isolated";
  fs::remove_all(root);
  fs::create_directories(root);

  writeFile(root / "a.md",
            "#deck: DeckA\n"
            "Q: a question\n"
            "A: a answer\n");
  // No deck header: should NOT inherit DeckA from the previous file.
  writeFile(root / "b.md",
            "Q: b question\n"
            "A: b answer\n");

  Cfg cfg{};
  cfg.inputPath = root;

  ParseResult res = parseFiles(cfg);
  fs::remove_all(root);

  REQUIRE(res.errors.empty());
  REQUIRE(res.cards.size() == 2);

  auto bCard =
      std::find_if(res.cards.begin(), res.cards.end(),
                   [](const Card& c) { return c.front == "b question"; });
  REQUIRE(bCard != res.cards.end());
  CHECK(bCard->deck == "");
}

TEST_CASE("parseFiles labels errors with the file they came from", "[io]") {
  fs::path path = fs::temp_directory_path() / "md2anki_error_origin.md";
  writeFile(path, "Q: unanswered question\n");

  Cfg cfg{};
  cfg.inputPath = path;

  ParseResult res = parseFiles(cfg);
  fs::remove(path);

  REQUIRE(res.errors.size() == 1);
  CHECK(res.errors[0].file == path);
  CHECK(res.errors[0].lineNumber == 1);
}

TEST_CASE("parseFiles reports an error when the input file cannot be opened",
          "[io]") {
  Cfg cfg{};
  cfg.inputPath = fs::temp_directory_path() / "md2anki_does_not_exist.md";

  ParseResult res = parseFiles(cfg);

  REQUIRE(res.cards.empty());
  REQUIRE(res.errors.size() == 1);
  CHECK(res.errors[0].lineNumber == 0);
  CHECK(res.errors[0].file == cfg.inputPath);
}

TEST_CASE("saveFile writes the header and card rows", "[io]") {
  fs::path outPath = fs::temp_directory_path() / "md2anki_savefile_out.csv";
  Cfg      cfg{};
  cfg.outputPath = outPath;

  ParseResult              res;
  std::vector<std::string> tags{"tag"};
  res.cards.emplace_back("Deck", tags, CardType::QA, "front", "back");

  REQUIRE(saveFile(cfg, res));

  std::string contents = readBack(outPath);
  fs::remove(outPath);

  CHECK(contents == toCsvDocument(res.cards));
  CHECK(contents.find("#separator:Comma") != std::string::npos);
  CHECK(contents.find("\"Deck\",\"tag\",Basic,\"front\",\"back\"") !=
        std::string::npos);
}

TEST_CASE("applyWriteIds assigns ids and patches only the Q:/C: lines",
          "[io]") {
  fs::path path = fs::temp_directory_path() / "md2anki_writeids.md";
  writeFile(path,
            "#deck: Deck\n"
            "Q: front one\n"
            "A: back one\n"
            "\n"
            "Q: front two\n"
            "A: back two\n");

  Cfg cfg{};
  cfg.inputPath = path;
  ParseResult res = parseFiles(cfg);
  REQUIRE(res.cards.size() == 2);

  Manifest manifest{};
  fs::path manifestFile =
      fs::temp_directory_path() / "md2anki_writeids_manifest";

  REQUIRE(applyWriteIds(res, manifest, manifestFile));
  CHECK_FALSE(res.cards[0].id.empty());
  CHECK_FALSE(res.cards[1].id.empty());
  CHECK(res.cards[0].id != res.cards[1].id);

  std::string patched = readBack(path);
  fs::remove(path);
  fs::remove(manifestFile);

  CHECK(patched == "#deck: Deck\n"
                    "Q(" + res.cards[0].id + "): front one\n"
                    "A: back one\n"
                    "\n"
                    "Q(" + res.cards[1].id + "): front two\n"
                    "A: back two\n");
  CHECK(manifest.ids == std::set<std::string>{res.cards[0].id, res.cards[1].id});
}

TEST_CASE("applyWriteIds is idempotent on cards that already have an id",
          "[io]") {
  fs::path path = fs::temp_directory_path() / "md2anki_writeids_idempotent.md";
  writeFile(path, "Q: front\nA: back\n");

  Cfg cfg{};
  cfg.inputPath = path;
  Manifest manifest{};
  fs::path manifestFile =
      fs::temp_directory_path() / "md2anki_writeids_idempotent_manifest";

  ParseResult first = parseFiles(cfg);
  REQUIRE(applyWriteIds(first, manifest, manifestFile));
  std::string firstId = first.cards[0].id;

  ParseResult second = parseFiles(cfg);
  Manifest    reread  = readManifest(manifestFile);
  REQUIRE(applyWriteIds(second, reread, manifestFile));
  fs::remove(path);
  fs::remove(manifestFile);

  CHECK(second.cards[0].id == firstId);
  CHECK(reread.ids == std::set<std::string>{firstId});
}

TEST_CASE("applyWriteIds fills the manifest's source from the caller",
          "[io]") {
  fs::path path = fs::temp_directory_path() / "md2anki_writeids_source.md";
  writeFile(path, "Q: front\nA: back\n");

  Cfg cfg{};
  cfg.inputPath = path;
  ParseResult res = parseFiles(cfg);

  Manifest manifest{};
  manifest.source = path.string();
  fs::path manifestFile =
      fs::temp_directory_path() / "md2anki_writeids_source_manifest";

  REQUIRE(applyWriteIds(res, manifest, manifestFile));
  Manifest written = readManifest(manifestFile);
  fs::remove(path);
  fs::remove(manifestFile);

  CHECK(written.source == path.string());
}

TEST_CASE("saveFile refuses to write when strictWarn is set and errors exist",
          "[io]") {
  fs::path outPath = fs::temp_directory_path() / "md2anki_savefile_strict.csv";
  Cfg      cfg{};
  cfg.outputPath = outPath;
  cfg.strictWarn = true;

  ParseResult res;
  res.errors.push_back({"dummy.md", 1, "some error"});

  CHECK_FALSE(saveFile(cfg, res));
  CHECK_FALSE(fs::exists(outPath));
}
