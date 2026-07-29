#pragma once
#include "cli.h"
#include "card.h"
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>


struct ParseError {
  std::filesystem::path file;
  size_t lineNumber;
  std::string message;
};

struct ParseResult {
  std::vector<Card> cards;
  std::vector<ParseError> errors;
};

std::vector<std::string> collectCSV(const std::string_view& csv);
size_t findNot(std::string_view input, char token);
bool parsePair(std::ifstream& ifile, std::string& back, size_t& lineNumber, std::string expectedToken);
bool toCloze(std::string& input);
ParseResult parseFiles(const Cfg& cfg);
bool saveFile(const Cfg& cfg, ParseResult& res);
