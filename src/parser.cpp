#include "parser.h"

#include <cctype>
#include <string>
#include <string_view>
#include <vector>

#include "card.h"
#include "util.h"

namespace fs = std::filesystem;

namespace {
/// Strips the trailing '\r' of a CRLF-terminated line.
std::string_view dropCarriageReturn(std::string_view line) {
  if (!line.empty() && line.back() == '\r') {
    line.remove_suffix(1);
  }
  return line;
}
}  // namespace

std::string_view LineCursor::peek() const {
  if (atEnd()) return {};
  size_t end = text_.find('\n', pos_);
  if (end == std::string_view::npos) {
    end = text_.size();
  }
  return dropCarriageReturn(text_.substr(pos_, end - pos_));
}

std::string_view LineCursor::next() {
  std::string_view line = peek();
  if (atEnd()) return line;
  size_t end = text_.find('\n', pos_);
  pos_       = (end == std::string_view::npos) ? text_.size() : end + 1;
  ++lineNumber_;
  return line;
}

size_t findNot(const std::string_view input, char token) {
  for (size_t i{0}; i < input.size(); ++i) {
    if (input[i] != token) {
      return i;
    }
  }
  return input.size();
}

std::vector<std::string> collectCSV(std::string_view csv) {
  std::vector<std::string> result;
  std::string_view         csvStream = csv;
  size_t                   delimiter{0};
  while (delimiter != std::string_view::npos) {
    delimiter = csvStream.find_first_of(", \t\r\n");
    std::string_view token = csvStream.substr(0, delimiter);
    token                  = trim(token);
    if (!token.empty()) {
      result.emplace_back(token);
    }
    if (delimiter == std::string_view::npos) {
      break;
    }
    csvStream = csvStream.substr(delimiter + 1);
  }
  return result;
}

bool parsePair(LineCursor& lines, std::string& back,
               std::string_view expectedToken) {
  std::string_view line = ltrim(lines.peek());
  if (lines.atEnd() || line.empty() ||
      !toLower(line).starts_with(expectedToken)) {
    return false;  // Leave the line for the caller's loop to reprocess.
  }
  lines.next();
  back = ltrim(line.substr(expectedToken.size()));
  return true;
}

bool toCloze(std::string& input) {
  std::string result;
  result.reserve(input.size());
  for (size_t i{0}; i < input.size(); ++i) {
    if (std::isdigit(static_cast<unsigned char>(input[i])) &&
        i + 1 < input.size() && input[i + 1] == '{') {
      int    clozeNumber = input[i] - '0';
      size_t clozeStart  = i + 2;
      size_t nextStart   = input.find('{', clozeStart + 1);
      size_t clozeEnd    = input.find('}', clozeStart);
      if (clozeEnd == std::string::npos ||
          (nextStart != std::string::npos && clozeEnd > nextStart)) {
        return false;
      }
      std::string clozeContent =
          input.substr(clozeStart, clozeEnd - clozeStart);
      result +=
          "{{c" + std::to_string(clozeNumber) + "::" + clozeContent + "}}";
      i = clozeEnd;
    } else {
      result += input[i];
    }
  }
  input = std::move(result);
  return true;
}

ParseResult parseMarkdown(std::string_view text, const fs::path& origin) {
  ParseResult              res;
  LineCursor               lines{text};
  std::vector<std::string> currentTags;
  std::string              currentDeck;

  while (!lines.atEnd()) {
    std::string_view line       = ltrim(lines.next());
    size_t           lineNumber = lines.lineNumber();
    std::string      lowerLine  = toLower(line);

    if (line.empty()) continue;    // Skip empty lines
    if (line.starts_with('#')) {   // If line is a header
      std::string_view header      = line.substr(findNot(line, '#'));
      std::string      lowerHeader = toLower(header);

      if (header.empty()) continue;  // Skip empty header

      if (size_t tagsAt = lowerHeader.find("tags:");
          tagsAt != std::string::npos) {  // Collect tags
        currentTags = collectCSV(ltrim(header.substr(tagsAt + 5)));
      } else if (size_t deckAt = lowerHeader.find("deck:");
                 deckAt != std::string::npos) {  // Get deck name
        currentDeck = ltrim(header.substr(deckAt + 5));
      }

    } else if (lowerLine.starts_with("q:")) {  // Line is a question
      std::string front{ltrim(line.substr(2))};
      std::string back;
      if (parsePair(lines, back, "a:")) {
        res.cards.emplace_back(currentDeck, currentTags, CardType::QA, front,
                               back);
      } else {
        res.errors.push_back({origin, lineNumber,
                              "Missing answer pair to question: '" + front +
                                  "'"});
      }
    } else if (lowerLine.starts_with("qr:")) {
      std::string front{ltrim(line.substr(3))};
      std::string back;
      if (parsePair(lines, back, "ar:")) {
        res.cards.emplace_back(currentDeck, currentTags, CardType::QAR, front,
                               back);
      } else {
        res.errors.push_back({origin, lineNumber,
                              "Missing answer pair to question: '" + front +
                                  "'"});
      }
    } else if (lowerLine.starts_with("c:")) {
      std::string front{ltrim(line.substr(2))};
      if (toCloze(front)) {
        res.cards.emplace_back(currentDeck, currentTags, CardType::Cloze, front,
                               "");
      } else {
        res.errors.push_back({origin, lineNumber,
                              "Missing cloze deletion closing bracket: '" +
                                  front + "'"});
      }
    }
  }
  return res;
}
