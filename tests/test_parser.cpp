#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <filesystem>
#include <fstream>

#include "cli.h"
#include "parser.h"

namespace fs = std::filesystem;

TEST_CASE("findNot returns index of first differing char", "[parser]") {
  CHECK(findNot("###abc", '#') == 3);
  CHECK(findNot("abc", '#') == 0);
  CHECK(findNot("###", '#') == 3);
  CHECK(findNot("", '#') == 0);
}

TEST_CASE("collectCSV splits, trims, and drops empty fields", "[parser]") {
  CHECK(collectCSV("a, b , c") == std::vector<std::string>{"a", "b", "c"});
  CHECK(collectCSV("a,,b") == std::vector<std::string>{"a", "b"});
  CHECK(collectCSV("") == std::vector<std::string>{});
  CHECK(collectCSV("single") == std::vector<std::string>{"single"});
}

TEST_CASE("toCloze converts a numbered cloze marker", "[parser]") {
  std::string input = "The 0{answer} is here";
  REQUIRE(toCloze(input));
  CHECK(input == "The {{c0::answer}} is here");
}

TEST_CASE("toCloze handles multiple cloze markers", "[parser]") {
  std::string input = "0{first} and 1{second}";
  REQUIRE(toCloze(input));
  CHECK(input == "{{c0::first}} and {{c1::second}}");
}

TEST_CASE("toCloze leaves text without markers unchanged", "[parser]") {
  std::string input = "no clozes here";
  REQUIRE(toCloze(input));
  CHECK(input == "no clozes here");
}

TEST_CASE("toCloze fails on unclosed cloze", "[parser]") {
  std::string input = "The 0{answer is here";
  CHECK_FALSE(toCloze(input));
}

TEST_CASE("toCloze fails when a new cloze opens before the previous closes",
          "[parser]") {
  std::string input = "0{first 1{second}";
  CHECK_FALSE(toCloze(input));
}

TEST_CASE("parsePair reads a matching answer line", "[parser]") {
  fs::path path = fs::temp_directory_path() / "md2anki_parsepair_ok.txt";
  {
    std::ofstream ofile(path);
    ofile << "A: the answer\n";
  }
  std::string back;
  size_t      lineNumber = 0;
  bool        ok;
  {
    std::ifstream ifile(path);
    ok = parsePair(ifile, back, lineNumber, "a:");
  }
  fs::remove(path);

  REQUIRE(ok);
  CHECK(back == "the answer");
  CHECK(lineNumber == 1);
}

TEST_CASE("parsePair fails when the expected token is missing", "[parser]") {
  fs::path path = fs::temp_directory_path() / "md2anki_parsepair_bad.txt";
  {
    std::ofstream ofile(path);
    ofile << "not an answer\n";
  }
  std::string back;
  size_t      lineNumber = 0;
  bool        ok;
  std::string replayedLine;
  {
    std::ifstream ifile(path);
    ok = parsePair(ifile, back, lineNumber, "a:");
    // The unmatched line should be put back for the caller to reprocess.
    std::getline(ifile, replayedLine);
  }
  fs::remove(path);

  CHECK_FALSE(ok);
  CHECK(lineNumber == 0);
  CHECK(replayedLine == "not an answer");
}

namespace {
fs::path writeTempMd(const std::string& contents, const std::string& name) {
  fs::path      path = fs::temp_directory_path() / name;
  std::ofstream ofile(path);
  ofile << contents;
  return path;
}
}  // namespace

TEST_CASE("parseFiles recursively collects .md files from a directory tree",
          "[parser]") {
  fs::path root = fs::temp_directory_path() / "md2anki_dirmode_root";
  fs::remove_all(root);
  fs::create_directories(root / "nested");

  {
    std::ofstream top(root / "top.md");
    top << "#deck: TopDeck\n"
        << "Q: top question\n"
        << "A: top answer\n";
  }
  {
    std::ofstream nested(root / "nested" / "nested.md");
    nested << "#deck: NestedDeck\n"
           << "Q: nested question\n"
           << "A: nested answer\n";
  }
  {
    // Non-.md files should be ignored.
    std::ofstream ignored(root / "notes.txt");
    ignored << "Q: should not be parsed\n"
            << "A: ignored\n";
  }

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

TEST_CASE("parseFiles keeps deck/tag context independent per file",
          "[parser]") {
  fs::path root = fs::temp_directory_path() / "md2anki_dirmode_isolated";
  fs::remove_all(root);
  fs::create_directories(root);

  {
    std::ofstream withDeck(root / "a.md");
    withDeck << "#deck: DeckA\n"
             << "Q: a question\n"
             << "A: a answer\n";
  }
  {
    // No deck header: should NOT inherit DeckA from the previous file.
    std::ofstream noDeck(root / "b.md");
    noDeck << "Q: b question\n"
           << "A: b answer\n";
  }

  Cfg cfg{};
  cfg.inputPath = root;

  ParseResult res = parseFiles(cfg);
  fs::remove_all(root);

  REQUIRE(res.errors.empty());
  REQUIRE(res.cards.size() == 2);

  auto bCard = std::find_if(res.cards.begin(), res.cards.end(),
                             [](const Card& c) { return c.front == "b question"; });
  REQUIRE(bCard != res.cards.end());
  CHECK(bCard->deck == "");
}

TEST_CASE("parseFile reports an error when the input file cannot be opened",
          "[parser]") {
  Cfg cfg{};
  cfg.inputPath = fs::temp_directory_path() / "md2anki_does_not_exist.md";

  ParseResult res = parseFiles(cfg);

  REQUIRE(res.cards.empty());
  REQUIRE(res.errors.size() == 1);
  CHECK(res.errors[0].lineNumber == 0);
}

TEST_CASE("parseFile parses QA, QAR, and Cloze cards with deck/tags headers",
          "[parser]") {
  const std::string md =
      "#deck: MyDeck\n"
      "#tags: t1, t2\n"
      "Q: What is 2+2?\n"
      "A: 4\n"
      "QR: capital of France\n"
      "AR: Paris\n"
      "C: 0{Anki} is spaced repetition software\n";
  fs::path path = writeTempMd(md, "md2anki_parsefile_ok.md");
  Cfg      cfg{};
  cfg.inputPath = path;

  ParseResult res = parseFiles(cfg);
  fs::remove(path);

  REQUIRE(res.errors.empty());
  REQUIRE(res.cards.size() == 3);

  CHECK(res.cards[0].type == CardType::QA);
  CHECK(res.cards[0].front == "What is 2+2?");
  CHECK(res.cards[0].back == "4");
  CHECK(res.cards[0].deck == "MyDeck");
  CHECK(res.cards[0].tags == std::vector<std::string>{"t1", "t2"});

  CHECK(res.cards[1].type == CardType::QAR);
  CHECK(res.cards[1].front == "capital of France");
  CHECK(res.cards[1].back == "Paris");

  CHECK(res.cards[2].type == CardType::Cloze);
  CHECK(res.cards[2].front == "{{c0::Anki}} is spaced repetition software");
}

TEST_CASE("parseFile records an error for a question missing its answer",
          "[parser]") {
  const std::string md = "Q: unanswered question\n";
  fs::path path        = writeTempMd(md, "md2anki_parsefile_missing_answer.md");
  Cfg      cfg{};
  cfg.inputPath = path;

  ParseResult res = parseFiles(cfg);
  fs::remove(path);

  CHECK(res.cards.empty());
  REQUIRE(res.errors.size() == 1);
  // parsePair shouldn't advance lineNumber on empty read
  CHECK(res.errors[0].lineNumber == 1);
}

TEST_CASE("parseFile reprocesses a line consumed by a failed answer pairing",
          "[parser]") {
  const std::string md =
      "Q: first question with no answer\n"
      "Q: second question\n"
      "A: second answer\n";
  fs::path path = writeTempMd(md, "md2anki_parsefile_reprocess.md");
  Cfg      cfg{};
  cfg.inputPath = path;

  ParseResult res = parseFiles(cfg);
  fs::remove(path);

  REQUIRE(res.errors.size() == 1);
  CHECK(res.errors[0].lineNumber == 1);

  // The second question must not be lost when the first pairing fails.
  REQUIRE(res.cards.size() == 1);
  CHECK(res.cards[0].front == "second question");
  CHECK(res.cards[0].back == "second answer");
}

TEST_CASE("parseFile records an error for an unclosed cloze", "[parser]") {
  const std::string md   = "C: 0{unclosed\n";
  fs::path          path = writeTempMd(md, "md2anki_parsefile_bad_cloze.md");
  Cfg               cfg{};
  cfg.inputPath = path;

  ParseResult res = parseFiles(cfg);
  fs::remove(path);

  CHECK(res.cards.empty());
  REQUIRE(res.errors.size() == 1);
  CHECK(res.errors[0].lineNumber == 1);
}

TEST_CASE("saveFile writes the header and card rows", "[parser]") {
  fs::path outPath = fs::temp_directory_path() / "md2anki_savefile_out.csv";
  Cfg      cfg{};
  cfg.outputPath = outPath;

  ParseResult              res;
  std::vector<std::string> tags{"tag"};
  res.cards.emplace_back("Deck", tags, CardType::QA, "front", "back");

  bool ok = saveFile(cfg, res);
  REQUIRE(ok);

  std::string contents;
  {
    std::ifstream ifile(outPath);
    contents.assign(std::istreambuf_iterator<char>(ifile),
                    std::istreambuf_iterator<char>());
  }
  fs::remove(outPath);

  CHECK(contents.find("#separator:Comma") != std::string::npos);
  CHECK(contents.find("\"Deck\",\"tag\",Basic,\"front\",\"back\"") !=
        std::string::npos);
}

TEST_CASE("saveFile refuses to write when strictWarn is set and errors exist",
          "[parser]") {
  fs::path outPath = fs::temp_directory_path() / "md2anki_savefile_strict.csv";
  Cfg      cfg{};
  cfg.outputPath = outPath;
  cfg.strictWarn = true;

  ParseResult res;
  res.errors.push_back({1, "some error"});

  bool ok = saveFile(cfg, res);
  CHECK_FALSE(ok);
  CHECK_FALSE(fs::exists(outPath));
}
