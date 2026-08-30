#include <catch2/catch_test_macros.hpp>
#include <string>
#include <vector>

#include "parser.h"

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

TEST_CASE("collectCSV also splits on whitespace, with or without commas",
          "[parser]") {
  CHECK(collectCSV("a b c") == std::vector<std::string>{"a", "b", "c"});
  CHECK(collectCSV("a,b c") == std::vector<std::string>{"a", "b", "c"});
  CHECK(collectCSV("a\tb\nc") == std::vector<std::string>{"a", "b", "c"});
  CHECK(collectCSV("  a   b  ") == std::vector<std::string>{"a", "b"});
}

TEST_CASE("toCloze converts a numbered cloze marker", "[parser]") {
  std::string input = "The 1{answer} is here";
  REQUIRE(toCloze(input) == ClozeStatus::Ok);
  CHECK(input == "The {{c1::answer}} is here");
}

TEST_CASE("toCloze handles multiple cloze markers", "[parser]") {
  std::string input = "1{first} and 2{second}";
  REQUIRE(toCloze(input) == ClozeStatus::Ok);
  CHECK(input == "{{c1::first}} and {{c2::second}}");
}

TEST_CASE("toCloze converts two-digit cloze numbers", "[parser]") {
  std::string input = "10{ten} and 42{forty two}";
  REQUIRE(toCloze(input) == ClozeStatus::Ok);
  CHECK(input == "{{c10::ten}} and {{c42::forty two}}");
}

TEST_CASE("toCloze accepts the whole c1-c99 range", "[parser]") {
  std::string lowest = "1{x}";
  REQUIRE(toCloze(lowest) == ClozeStatus::Ok);
  CHECK(lowest == "{{c1::x}}");

  std::string highest = "99{x}";
  REQUIRE(toCloze(highest) == ClozeStatus::Ok);
  CHECK(highest == "{{c99::x}}");
}

TEST_CASE("toCloze normalizes leading zeros in the cloze number", "[parser]") {
  std::string input = "007{x}";
  REQUIRE(toCloze(input) == ClozeStatus::Ok);
  CHECK(input == "{{c7::x}}");
}

TEST_CASE("toCloze rejects cloze numbers outside c1-c99", "[parser]") {
  std::string zero = "0{x}";
  CHECK(toCloze(zero) == ClozeStatus::NumberOutOfRange);
  CHECK(zero == "0{x}");  // Left unchanged so the caller can report it.

  std::string hundred = "100{x}";
  CHECK(toCloze(hundred) == ClozeStatus::NumberOutOfRange);

  std::string huge = "999999999999999999999{x}";
  CHECK(toCloze(huge) == ClozeStatus::NumberOutOfRange);
}

TEST_CASE("toCloze leaves digits that don't open a marker alone", "[parser]") {
  std::string input = "Apollo 11 landed in 1969, 1{Neil Armstrong} first";
  REQUIRE(toCloze(input) == ClozeStatus::Ok);
  CHECK(input == "Apollo 11 landed in 1969, {{c1::Neil Armstrong}} first");
}

TEST_CASE("toCloze leaves text without markers unchanged", "[parser]") {
  std::string input = "no clozes here";
  REQUIRE(toCloze(input) == ClozeStatus::Ok);
  CHECK(input == "no clozes here");
}

TEST_CASE("toCloze leaves a brace not preceded by digits alone", "[parser]") {
  std::string input = "a literal {brace} and 1{a cloze}";
  REQUIRE(toCloze(input) == ClozeStatus::Ok);
  CHECK(input == "a literal {brace} and {{c1::a cloze}}");
}

TEST_CASE("toCloze fails on unclosed cloze", "[parser]") {
  std::string input = "The 1{answer is here";
  CHECK(toCloze(input) == ClozeStatus::UnclosedBracket);
  CHECK(input == "The 1{answer is here");  // Left unchanged.
}

TEST_CASE("toCloze fails when a new cloze opens before the previous closes",
          "[parser]") {
  std::string input = "1{first 2{second}";
  CHECK(toCloze(input) == ClozeStatus::UnclosedBracket);
}

TEST_CASE("LineCursor walks lines and numbers them from one", "[parser]") {
  LineCursor lines{"first\nsecond\nthird"};

  CHECK(lines.lineNumber() == 0);
  CHECK(lines.peek() == "first");
  CHECK(lines.next() == "first");
  CHECK(lines.lineNumber() == 1);
  CHECK(lines.next() == "second");
  CHECK(lines.next() == "third");
  CHECK(lines.lineNumber() == 3);
  CHECK(lines.atEnd());
}

TEST_CASE("LineCursor keeps interior blank lines but stops after a trailing "
          "newline",
          "[parser]") {
  LineCursor lines{"a\n\nb\n"};

  CHECK(lines.next() == "a");
  CHECK(lines.next() == "");
  CHECK(lines.next() == "b");
  CHECK(lines.atEnd());
}

TEST_CASE("LineCursor is empty for empty text", "[parser]") {
  LineCursor lines{""};
  CHECK(lines.atEnd());
  CHECK(lines.peek() == "");
}

TEST_CASE("LineCursor strips carriage returns from CRLF text", "[parser]") {
  LineCursor lines{"a\r\nb\r\n"};
  CHECK(lines.next() == "a");
  CHECK(lines.next() == "b");
  CHECK(lines.atEnd());
}

TEST_CASE("parsePair reads a matching answer line", "[parser]") {
  LineCursor  lines{"A: the answer\n"};
  std::string back;

  REQUIRE(parsePair(lines, back, "a:"));
  CHECK(back == "the answer");
  CHECK(lines.lineNumber() == 1);
  CHECK(lines.atEnd());
}

TEST_CASE("parsePair fails when the expected token is missing", "[parser]") {
  LineCursor  lines{"not an answer\n"};
  std::string back;

  CHECK_FALSE(parsePair(lines, back, "a:"));
  CHECK(lines.lineNumber() == 0);
  // The unmatched line should be left for the caller to reprocess.
  CHECK(lines.next() == "not an answer");
}

TEST_CASE("parsePair rewinds on a blank next line instead of consuming it",
          "[parser]") {
  LineCursor  lines{"\nA: real answer\n"};
  std::string back;

  CHECK_FALSE(parsePair(lines, back, "a:"));
  CHECK(lines.lineNumber() == 0);
  // The blank line should be left in place, not silently swallowed.
  CHECK(lines.next() == "");
}

TEST_CASE("parsePair fails at end of input", "[parser]") {
  LineCursor  lines{""};
  std::string back = "untouched";

  CHECK_FALSE(parsePair(lines, back, "a:"));
  CHECK(back == "untouched");
}

TEST_CASE("parseMarkdown parses QA, QAR, and Cloze cards with deck/tags "
          "headers",
          "[parser]") {
  ParseResult res = parseMarkdown(
      "#deck: MyDeck\n"
      "#tags: t1, t2\n"
      "Q: What is 2+2?\n"
      "A: 4\n"
      "QR: capital of France\n"
      "AR: Paris\n"
      "C: 1{Anki} is spaced repetition software\n");

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
  CHECK(res.cards[2].front == "{{c1::Anki}} is spaced repetition software");
}

TEST_CASE("parseMarkdown accepts markers in any case", "[parser]") {
  ParseResult res = parseMarkdown(
      "#DECK: MyDeck\n"
      "#Tags: t1\n"
      "q: lower question\n"
      "a: lower answer\n");

  REQUIRE(res.errors.empty());
  REQUIRE(res.cards.size() == 1);
  CHECK(res.cards[0].deck == "MyDeck");
  CHECK(res.cards[0].tags == std::vector<std::string>{"t1"});
  CHECK(res.cards[0].front == "lower question");
  CHECK(res.cards[0].back == "lower answer");
}

TEST_CASE("parseMarkdown applies headers to the cards that follow them",
          "[parser]") {
  ParseResult res = parseMarkdown(
      "#deck: First\n"
      "Q: one\n"
      "A: 1\n"
      "#deck: Second\n"
      "#tags: later\n"
      "Q: two\n"
      "A: 2\n");

  REQUIRE(res.errors.empty());
  REQUIRE(res.cards.size() == 2);
  CHECK(res.cards[0].deck == "First");
  CHECK(res.cards[0].tags.empty());
  CHECK(res.cards[1].deck == "Second");
  CHECK(res.cards[1].tags == std::vector<std::string>{"later"});
}

TEST_CASE("parseMarkdown ignores prose and empty headers", "[parser]") {
  ParseResult res = parseMarkdown(
      "# Just a title\n"
      "###\n"
      "Some ordinary paragraph text.\n"
      "\n"
      "Q: still parsed\n"
      "A: yes\n");

  REQUIRE(res.errors.empty());
  REQUIRE(res.cards.size() == 1);
  CHECK(res.cards[0].front == "still parsed");
  CHECK(res.cards[0].deck.empty());
}

TEST_CASE("parseMarkdown records an error for a question missing its answer",
          "[parser]") {
  ParseResult res = parseMarkdown("Q: unanswered question\n");

  CHECK(res.cards.empty());
  REQUIRE(res.errors.size() == 1);
  CHECK(res.errors[0].lineNumber == 1);
  CHECK(res.errors[0].message.find("unanswered question") !=
        std::string::npos);
}

TEST_CASE("parseMarkdown labels errors with the origin it was given",
          "[parser]") {
  ParseResult res = parseMarkdown("Q: unanswered question\n", "notes/deck.md");

  REQUIRE(res.errors.size() == 1);
  CHECK(res.errors[0].file == std::filesystem::path("notes/deck.md"));
}

TEST_CASE("parseMarkdown leaves the origin empty by default", "[parser]") {
  ParseResult res = parseMarkdown("Q: unanswered question\n");

  REQUIRE(res.errors.size() == 1);
  CHECK(res.errors[0].file.empty());
}

TEST_CASE("parseMarkdown reprocesses a line consumed by a failed answer "
          "pairing",
          "[parser]") {
  ParseResult res = parseMarkdown(
      "Q: first question with no answer\n"
      "Q: second question\n"
      "A: second answer\n");

  REQUIRE(res.errors.size() == 1);
  CHECK(res.errors[0].lineNumber == 1);
  CHECK(res.errors[0].message.find("first question with no answer") !=
        std::string::npos);

  // The second question must not be lost when the first pairing fails.
  REQUIRE(res.cards.size() == 1);
  CHECK(res.cards[0].front == "second question");
  CHECK(res.cards[0].back == "second answer");
}

TEST_CASE(
    "parseMarkdown reports accurate line numbers for consecutive questions "
    "separated by blank lines",
    "[parser]") {
  // A blank line is not skipped when looking for an answer, so each of these
  // questions fails to pair (a separate limitation from the bug below).
  // What this test guards is that the blank line is rewound rather than
  // permanently consumed, so line numbers stay accurate instead of drifting
  // and errors aren't misattributed to the wrong question.
  ParseResult res = parseMarkdown(
      "Q: first question with no answer\n"
      "\n"
      "Q: second question with no answer\n"
      "\n"
      "A: orphaned answer, unreachable since the blank line breaks pairing\n");

  CHECK(res.cards.empty());
  REQUIRE(res.errors.size() == 2);
  CHECK(res.errors[0].lineNumber == 1);
  CHECK(res.errors[0].message.find("first question with no answer") !=
        std::string::npos);
  CHECK(res.errors[1].lineNumber == 3);
  CHECK(res.errors[1].message.find("second question with no answer") !=
        std::string::npos);
}

TEST_CASE("parseMarkdown records an error for an unclosed cloze", "[parser]") {
  ParseResult res = parseMarkdown("C: 1{unclosed\n");

  CHECK(res.cards.empty());
  REQUIRE(res.errors.size() == 1);
  CHECK(res.errors[0].lineNumber == 1);
  CHECK(res.errors[0].message.find("closing bracket") != std::string::npos);
  CHECK(res.errors[0].message.find("1{unclosed") != std::string::npos);
}

TEST_CASE("parseMarkdown records an error for an out-of-range cloze number",
          "[parser]") {
  ParseResult res = parseMarkdown(
      "C: 0{too low}\n"
      "C: 100{too high}\n");

  CHECK(res.cards.empty());
  REQUIRE(res.errors.size() == 2);
  CHECK(res.errors[0].lineNumber == 1);
  CHECK(res.errors[0].message.find("between 1 and 99") != std::string::npos);
  CHECK(res.errors[0].message.find("0{too low}") != std::string::npos);
  CHECK(res.errors[1].lineNumber == 2);
  CHECK(res.errors[1].message.find("100{too high}") != std::string::npos);
}

TEST_CASE("parseMarkdown converts multi-digit cloze cards", "[parser]") {
  ParseResult res = parseMarkdown("C: 9{nine} then 10{ten} then 99{ninety}\n");

  REQUIRE(res.errors.empty());
  REQUIRE(res.cards.size() == 1);
  CHECK(res.cards[0].front ==
        "{{c9::nine}} then {{c10::ten}} then {{c99::ninety}}");
}

TEST_CASE("parseMarkdown handles CRLF input identically to LF", "[parser]") {
  ParseResult res = parseMarkdown(
      "#deck: MyDeck\r\n"
      "Q: question\r\n"
      "A: answer\r\n");

  REQUIRE(res.errors.empty());
  REQUIRE(res.cards.size() == 1);
  CHECK(res.cards[0].deck == "MyDeck");
  CHECK(res.cards[0].front == "question");
  CHECK(res.cards[0].back == "answer");
}

TEST_CASE("parseMarkdown accepts a final line without a trailing newline",
          "[parser]") {
  ParseResult res = parseMarkdown(
      "Q: question\n"
      "A: answer");

  REQUIRE(res.errors.empty());
  REQUIRE(res.cards.size() == 1);
  CHECK(res.cards[0].back == "answer");
}

TEST_CASE("parseMarkdown returns nothing for empty input", "[parser]") {
  ParseResult res = parseMarkdown("");
  CHECK(res.cards.empty());
  CHECK(res.errors.empty());
}

TEST_CASE("parseMarkdown reads an inline id on a question", "[parser]") {
  ParseResult res = parseMarkdown("Q(4be3): How will ids look?\nA: like this!\n");

  REQUIRE(res.cards.size() == 1);
  CHECK(res.cards[0].id == "4be3");
  CHECK(res.cards[0].front == "How will ids look?");
  CHECK(res.cards[0].lineNumber == 1);
}

TEST_CASE("parseMarkdown reads an inline id on a reversible question",
          "[parser]") {
  ParseResult res = parseMarkdown("Qr(ab12): front\nAr: back\n");

  REQUIRE(res.cards.size() == 1);
  CHECK(res.cards[0].id == "ab12");
  CHECK(res.cards[0].front == "front");
}

TEST_CASE("parseMarkdown reads an inline id on a cloze", "[parser]") {
  ParseResult res = parseMarkdown("C(cd34): 1{answer} is here\n");

  REQUIRE(res.cards.size() == 1);
  CHECK(res.cards[0].id == "cd34");
}

TEST_CASE("parseMarkdown leaves id empty when the tag carries none",
          "[parser]") {
  ParseResult res = parseMarkdown("Q: no id\nA: none\n");

  REQUIRE(res.cards.size() == 1);
  CHECK(res.cards[0].id.empty());
}

TEST_CASE("parseMarkdown reads an inline id case-insensitively", "[parser]") {
  ParseResult res = parseMarkdown("q(4be3): lower tag\na: lower answer\n");

  REQUIRE(res.cards.size() == 1);
  CHECK(res.cards[0].id == "4be3");
}

TEST_CASE("isDirectiveLine recognizes headers and card markers", "[parser]") {
  CHECK(isDirectiveLine("#deck: X"));
  CHECK(isDirectiveLine("Q: x"));
  CHECK(isDirectiveLine("qr: x"));
  CHECK(isDirectiveLine("A: x"));
  CHECK(isDirectiveLine("ar: x"));
  CHECK(isDirectiveLine("C: x"));
  CHECK(isDirectiveLine("Q(4be3): x"));
  CHECK(isDirectiveLine("qr(ab12): x"));
  CHECK_FALSE(isDirectiveLine("just prose"));
  CHECK_FALSE(isDirectiveLine(""));
}

TEST_CASE("appendContinuation is a no-op when the next line isn't indented",
          "[parser]") {
  LineCursor  lines{"not indented\n"};
  std::string field = "first line";

  appendContinuation(lines, field);
  CHECK(field == "first line");
  // The unconsumed line is left for the caller to reprocess.
  CHECK(lines.next() == "not indented");
}

TEST_CASE("appendContinuation stops at a directive even if indented",
          "[parser]") {
  LineCursor  lines{"  Q: next card\n"};
  std::string field = "first line";

  appendContinuation(lines, field);
  CHECK(field == "first line");
  CHECK(lines.lineNumber() == 0);
}

TEST_CASE("appendContinuation absorbs indented lines, stripping one indent "
          "unit",
          "[parser]") {
  LineCursor  lines{"\tsecond line\n  third line\n"};
  std::string field = "first line";

  appendContinuation(lines, field);
  CHECK(field == "first line\nsecond line\nthird line");
  CHECK(lines.atEnd());
}

TEST_CASE("appendContinuation preserves a code line's own nested indentation",
          "[parser]") {
  LineCursor  lines{"\tif (n <= 1) {\n\t\treturn 1;\n\t}\n"};
  std::string field = "base case:";

  appendContinuation(lines, field);
  CHECK(field == "base case:\nif (n <= 1) {\n\treturn 1;\n}");
}

TEST_CASE("appendContinuation tolerates a blank line inside an indented "
          "block",
          "[parser]") {
  LineCursor  lines{"\tsecond line\n\n\tthird line after one blank\n"};
  std::string field = "first line";

  appendContinuation(lines, field);
  CHECK(field == "first line\nsecond line\n\nthird line after one blank");
  CHECK(lines.atEnd());
}

TEST_CASE("appendContinuation tolerates multiple blank lines inside a block",
          "[parser]") {
  LineCursor  lines{"\ta\n\n\n\tb\n"};
  std::string field = "start";

  appendContinuation(lines, field);
  CHECK(field == "start\na\n\n\nb");
}

TEST_CASE("appendContinuation stops for good once the block dedents past "
          "blank lines",
          "[parser]") {
  LineCursor  lines{"\tsecond line\n\nnot indented anymore\n"};
  std::string field = "first line";

  appendContinuation(lines, field);
  CHECK(field == "first line\nsecond line");
  // The blank line and the dedented line are both left unconsumed.
  CHECK(lines.next() == "");
  CHECK(lines.next() == "not indented anymore");
}

TEST_CASE("appendContinuation leaves blank lines unconsumed when the block "
          "never resumes",
          "[parser]") {
  LineCursor  lines{"\tsecond line\n\n"};
  std::string field = "first line";

  appendContinuation(lines, field);
  CHECK(field == "first line\nsecond line");
  CHECK_FALSE(lines.atEnd());
  CHECK(lines.next() == "");
  CHECK(lines.atEnd());
}

TEST_CASE("parseMarkdown parses a multi-line question and answer", "[parser]") {
  ParseResult res = parseMarkdown(
      "Q: What are the five process states, and what triggers\n"
      "  Ready to Running?\n"
      "A: New, Ready, Running, Waiting, Terminated.\n"
      "  The transition happens when the scheduler dispatches the process.\n");

  REQUIRE(res.errors.empty());
  REQUIRE(res.cards.size() == 1);
  CHECK(res.cards[0].front ==
        "What are the five process states, and what triggers\n"
        "Ready to Running?");
  CHECK(res.cards[0].back ==
        "New, Ready, Running, Waiting, Terminated.\n"
        "The transition happens when the scheduler dispatches the process.");
}

TEST_CASE("parseMarkdown parses a multi-line answer with a blank-line-"
          "tolerant code block",
          "[parser]") {
  ParseResult res = parseMarkdown(
      "Q: What does the base case look like?\n"
      "A: It returns immediately without recursing:\n"
      "\tif (n <= 1) {\n"
      "\t\treturn 1;\n"
      "\t}\n"
      "\n"
      "\treturn n * factorial(n - 1);\n");

  REQUIRE(res.errors.empty());
  REQUIRE(res.cards.size() == 1);
  CHECK(res.cards[0].back ==
        "It returns immediately without recursing:\n"
        "if (n <= 1) {\n"
        "\treturn 1;\n"
        "}\n"
        "\n"
        "return n * factorial(n - 1);");
}

TEST_CASE("parseMarkdown does not swallow unindented prose after a card",
          "[parser]") {
  ParseResult res = parseMarkdown(
      "Q: What's the key memory difference between a thread and a process?\n"
      "A: Threads share the process's address space; each process has its "
      "own.\n"
      "Also need to re-read the mutex section before the exam.\n");

  REQUIRE(res.errors.empty());
  REQUIRE(res.cards.size() == 1);
  CHECK(res.cards[0].back ==
        "Threads share the process's address space; each process has its "
        "own.");
}

TEST_CASE("parseMarkdown still parses single-line cards unchanged",
          "[parser]") {
  ParseResult res = parseMarkdown(
      "Q: one\n"
      "A: 1\n"
      "\n"
      "Q: two\n"
      "A: 2\n");

  REQUIRE(res.errors.empty());
  REQUIRE(res.cards.size() == 2);
  CHECK(res.cards[0].back == "1");
  CHECK(res.cards[1].back == "2");
}

TEST_CASE("parseMarkdown parses a multi-line cloze card", "[parser]") {
  ParseResult res = parseMarkdown(
      "C: A thread has its own 1{stack}, 2{program counter}, and\n"
      "  3{register set}, but shares the process's heap.\n");

  REQUIRE(res.errors.empty());
  REQUIRE(res.cards.size() == 1);
  CHECK(res.cards[0].front ==
        "A thread has its own {{c1::stack}}, {{c2::program counter}}, and\n"
        "{{c3::register set}}, but shares the process's heap.");
}
