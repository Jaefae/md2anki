#include "parser.h"
#include "card.h"
#include <fstream>
#include <iostream>
#include <vector>

std::string_view ltrim(std::string_view s) {
  size_t start = 0;
  while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) start++;
  return s.substr(start);
}

std::string toLower(const std::string_view input) {
  std::string lower;
  lower.reserve(input.size());
  for (const unsigned char c : input) {
    lower += std::tolower(c);
  }
  return lower;
}

size_t findNot(const std::string_view input, char token) {
  for (size_t i{0}; i < input.size(); ++i) {
    if (input[i] != token) {return i;}
  }
  return input.size();
}

std::vector<std::string> collectCSV(const std::string_view& csv) {
  std::vector<std::string> result;
  std::string field;
  std::string_view csvStream = csv;
  size_t delimiter{0};
  while (delimiter != std::string_view::npos) {
    delimiter = csvStream.find(',', delimiter+1);
    result.push_back(static_cast<std::string>(csvStream.substr(0, delimiter)));
    csvStream = ltrim(csvStream.substr(delimiter+1));
  }
  return result;
}

bool parsePair(std::ifstream& ifile, std::string& back, size_t& lineNumber, std::string expectedToken) {
  std::string line;
  std::getline(ifile, line); // Next line
  lineNumber++;
  if (!line.starts_with(expectedToken)) {
    return false;
  }
  back = ltrim(line.substr(expectedToken.size()));
  return true;
}

bool toCloze(std::string& input) {
  std::string result;
  result.reserve(input.size());
  for (size_t i{0}; i < input.size(); ++i) {
    if (std::isdigit(input[i]) && i + 1 < input.size() && input[i+1] == '{') {
      int clozeNumber = input[i] - '0';
      size_t clozeStart = i + 2;
      size_t nextStart = input.find('{', clozeStart+1);
      size_t clozeEnd = input.find('}', clozeStart);
      if (clozeEnd == std::string::npos || ( nextStart != std::string::npos && clozeEnd > nextStart)) {
        return false;
      }
      std::string clozeContent = input.substr(clozeStart, clozeEnd - clozeStart);
      result += "{{c" + std::to_string(clozeNumber) + "::" + clozeContent + "}}";
      i = clozeEnd;
    } else {
      result += input[i];
    }
  }
  input = std::move(result);
  return true;
}

ParseResult parseFile(const Cfg &cfg)
{
  ParseResult res;
  std::ifstream ifile(cfg.inputPath);
  std::vector<std::string> currentTags;
  std::string currentDeck;
  if (!ifile.is_open())
  {
    res.errors.push_back({0, "Could not open file " + cfg.inputPath.string()});
    return res;
  }
  std::string line;
  size_t lineNumber{0};
  while (std::getline(ifile, line)) {
    line = ltrim(line);
    lineNumber++;
    std::string lowerLine = toLower(line);

    if (line.size() == 0) continue; // Skip empty lines
    if (line.starts_with('#')) { // If line is a header
      std::string header = line.substr(findNot(line, '#'));
      std::string lowerHeader = toLower(header);

      if (header.empty()) continue; // Skip empty header


      if (lowerHeader.find("tags:") != std::string::npos) { // Collect tags
        std::string tags = header.substr(lowerHeader.find("tags:") + 5);
        currentTags = collectCSV(ltrim(tags));
      } else if (lowerHeader.find("deck:") != std::string::npos) { // Get deck name
        currentDeck = ltrim(header.substr(lowerHeader.find("deck:") + 5));
      }

    } else if (lowerLine.starts_with("q:")) { // Line is a question
      std::string front = line.substr(2);
      std::string back;
      bool paired = parsePair(ifile, back, lineNumber, "a:");
      if (paired) res.cards.emplace_back(currentDeck, currentTags, CardType::QA, front, back);
      else {
        res.errors.push_back({lineNumber, "Missing answer pair to previous question." });
      }
    } else if (lowerLine.starts_with("qr:")) {
      std::string front = line.substr(3);
      std::string back;
      bool paired = parsePair(ifile, back, lineNumber, "ar:");
      if (paired) res.cards.emplace_back(currentDeck, currentTags, CardType::QAR, front, back);
      else {
        res.errors.push_back({lineNumber, "Missing answer pair to previous question."});
      }
    } else if (lowerLine.starts_with("c:")) {
      std::string front = line.substr(2);
      if (toCloze(front)) {
        res.cards.emplace_back(currentDeck, currentTags, CardType::Cloze, front, "");
      } else {
        res.errors.push_back({lineNumber, "Missing cloze deletion closing bracket."});
      }
    }
  }
  return res;
}

bool saveFile(const Cfg& cfg, ParseResult& res) {
  if (cfg.strictWarn && !res.errors.empty()) {return false;}
  std::ofstream ofile(cfg.outputPath);
  if (!ofile.is_open()) return false;
  ofile << "#separator:Comma\n";
  ofile << "#html:false\n";
  ofile << "#deck column:1\n";
  ofile << "#tags column:2\n";
  ofile << "#notetype column:3\n";
  for (auto& card: res.cards) {
    ofile << card.toCsv() << "\n";
  }
  return true;
}



