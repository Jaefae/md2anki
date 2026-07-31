#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>

#include "cli.h"
#include "io.h"

namespace fs = std::filesystem;

namespace {
fs::path examplesDir() { return fs::path(MD2ANKI_EXAMPLES_DIR); }

std::string readFile(const fs::path& path) {
  std::ifstream ifile(path);
  return {std::istreambuf_iterator<char>(ifile),
          std::istreambuf_iterator<char>()};
}
}  // namespace

TEST_CASE("example.md parses with no errors", "[examples]") {
  Cfg cfg{};
  cfg.inputPath = examplesDir() / "example.md";

  ParseResult res = parseFiles(cfg);

  CHECK(res.errors.empty());
  REQUIRE(res.cards.size() == 3);

  CHECK(res.cards[0].type == CardType::QA);
  CHECK(res.cards[0].deck == "Example Deck");
  CHECK(res.cards[0].tags == std::vector<std::string>{"Tips"});

  CHECK(res.cards[1].type == CardType::Cloze);
  CHECK(res.cards[2].type == CardType::Cloze);
  CHECK(res.cards[1].tags == std::vector<std::string>{"Tips", "Closures"});
}

TEST_CASE("headerexample.md parses with no errors", "[examples]") {
  Cfg cfg{};
  cfg.inputPath = examplesDir() / "headerexample.md";

  ParseResult res = parseFiles(cfg);

  CHECK(res.errors.empty());
  REQUIRE(res.cards.size() == 4);
  CHECK(res.cards[0].type == CardType::QA);
  CHECK(res.cards[1].type == CardType::Cloze);
  CHECK(res.cards[2].type == CardType::Cloze);
  CHECK(res.cards[3].type == CardType::Cloze);

  for (const auto& card : res.cards) {
    CHECK(card.deck == "Example Deck");
  }
}

TEST_CASE("in.MD parses every deck with no errors", "[examples]") {
  Cfg cfg{};
  cfg.inputPath = examplesDir() / "in.md";

  ParseResult res = parseFiles(cfg);
  CHECK(res.errors.empty());
  REQUIRE(res.cards.size() == 13);

  std::vector<std::string> decks;
  for (const auto& card : res.cards) {
    if (decks.empty() || decks.back() != card.deck) {
      decks.push_back(card.deck);
    }
  }
  CHECK(decks == std::vector<std::string>{"Biology", "Computer Science",
                                          "Databases", "Physics"});
}

TEST_CASE("in.MD converts to the checked-in out.csv", "[examples]") {
  Cfg cfg{};
  cfg.inputPath = examplesDir() / "in.md";

  ParseResult res = parseFiles(cfg);
  REQUIRE(res.errors.empty());

  CHECK(toCsvDocument(res.cards) == readFile(examplesDir() / "out.csv"));
}
