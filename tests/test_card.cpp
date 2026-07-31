#include "card.h"
#include <catch2/catch_test_macros.hpp>

TEST_CASE("escapeCSV wraps plain text in quotes", "[card]") {
  CHECK(escapeCSV("hello") == "\"hello\"");
  CHECK(escapeCSV("") == "\"\"");
}

TEST_CASE("escapeCSV doubles embedded quotes", "[card]") {
  CHECK(escapeCSV("say \"hi\"") == "\"say \"\"hi\"\"\"");
}

TEST_CASE("escapeCSV leaves commas and newlines untouched inside quotes", "[card]") {
  CHECK(escapeCSV("a,b\nc") == "\"a,b\nc\"");
}

TEST_CASE("Card::toCsv formats a QA card", "[card]") {
  std::vector<std::string> tags{"tag1", "tag2"};
  Card card("MyDeck", tags, CardType::QA, "front text", "back text");
  CHECK(card.toCsv() ==
        "\"MyDeck\",\"tag1 tag2\",Basic,\"front text\",\"back text\"");
}

TEST_CASE("Card::toCsv formats a QAR card", "[card]") {
  std::vector<std::string> tags{};
  Card card("Deck", tags, CardType::QAR, "q", "a");
  CHECK(card.toCsv() ==
        "\"Deck\",\"\",Basic (and reversed card),\"q\",\"a\"");
}

TEST_CASE("Card::toCsv formats a Cloze card without a back field", "[card]") {
  std::vector<std::string> tags{"x"};
  Card card("Deck", tags, CardType::Cloze, "{{c1::front}}", "");
  CHECK(card.toCsv() == "\"Deck\",\"x\",Cloze,\"{{c1::front}}\"");
}

TEST_CASE("Card::toCsv handles no tags", "[card]") {
  std::vector<std::string> tags{};
  Card card("Deck", tags, CardType::QA, "q", "a");
  CHECK(card.toCsv() == "\"Deck\",\"\",Basic,\"q\",\"a\"");
}

TEST_CASE("Card::toCsv escapes a deck name containing a comma", "[card]") {
  std::vector<std::string> tags{};
  Card card("My, Deck", tags, CardType::QA, "q", "a");
  CHECK(card.toCsv() == "\"My, Deck\",\"\",Basic,\"q\",\"a\"");
}

TEST_CASE("Card::toCsv escapes a tag containing a comma", "[card]") {
  std::vector<std::string> tags{"a,b", "plain"};
  Card card("Deck", tags, CardType::QA, "q", "a");
  CHECK(card.toCsv() == "\"Deck\",\"a,b plain\",Basic,\"q\",\"a\"");
}

TEST_CASE("Card::toCsv escapes embedded quotes in the deck name", "[card]") {
  std::vector<std::string> tags{};
  Card card("Deck \"Nickname\"", tags, CardType::QA, "q", "a");
  CHECK(card.toCsv() == "\"Deck \"\"Nickname\"\"\",\"\",Basic,\"q\",\"a\"");
}

TEST_CASE("toCsvDocument writes the Anki import headers", "[card]") {
  CHECK(toCsvDocument({}) ==
        "#separator:Comma\n"
        "#html:false\n"
        "#deck column:1\n"
        "#tags column:2\n"
        "#notetype column:3\n");
}

TEST_CASE("toCsvDocument appends one newline-terminated row per card",
          "[card]") {
  std::vector<std::string> tags{"tag"};
  std::vector<Card>        cards{
      Card("Deck", tags, CardType::QA, "q", "a"),
      Card("Deck", tags, CardType::Cloze, "{{c1::x}}", ""),
  };

  CHECK(toCsvDocument(cards) ==
        "#separator:Comma\n"
        "#html:false\n"
        "#deck column:1\n"
        "#tags column:2\n"
        "#notetype column:3\n"
        "\"Deck\",\"tag\",Basic,\"q\",\"a\"\n"
        "\"Deck\",\"tag\",Cloze,\"{{c1::x}}\"\n");
}
