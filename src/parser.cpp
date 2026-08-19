#include "parser.h"

#include <cctype>
#include <charconv>
#include <string>
#include <string_view>
#include <system_error>
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

bool isDigit(char chr) {
  return std::isdigit(static_cast<unsigned char>(chr)) != 0;
}

/// True if a raw (non-trimmed) line has a leading space or tab in the source.
bool isIndented(std::string_view rawLine) {
  return !rawLine.empty() && (rawLine.front() == ' ' || rawLine.front() == '\t');
}

/// Strips exactly one indent unit (a tab, or up to two leading spaces) so the
/// rest of a code snippet's own indentation survives.
std::string_view stripIndentMarker(std::string_view rawLine) {
  if (rawLine.empty()) return rawLine;
  if (rawLine.front() == '\t') return rawLine.substr(1);
  size_t spaces = 0;
  while (spaces < 2 && spaces < rawLine.size() && rawLine[spaces] == ' ') {
    ++spaces;
  }
  return rawLine.substr(spaces);
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
    delimiter              = csvStream.find_first_of(", \t\r\n");
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

bool isDirectiveLine(std::string_view line) {
  if (line.empty()) return false;
  if (line.starts_with('#')) return true;
  std::string lower = toLower(line);
  return lower.starts_with("qr:") || lower.starts_with("q:") ||
         lower.starts_with("ar:") || lower.starts_with("a:") ||
         lower.starts_with("c:");
}

void appendContinuation(LineCursor& lines, std::string& field) {
  while (!lines.atEnd()) {
    std::string_view rawNext = lines.peek();

    if (rawNext.empty()) {
      // A blank line only continues the field if the block hasn't dedented
      // by the time it resumes; otherwise leave the blank line(s) unconsumed.
      LineCursor::State checkpoint  = lines.checkpoint();
      size_t            blankCount = 0;
      while (!lines.atEnd() && lines.peek().empty()) {
        lines.next();
        ++blankCount;
      }
      std::string_view afterBlanks = lines.peek();
      if (!lines.atEnd() && isIndented(afterBlanks) &&
          !isDirectiveLine(ltrim(afterBlanks))) {
        field.append(blankCount, '\n');
        continue;
      }
      lines.restore(checkpoint);
      break;
    }

    if (!isIndented(rawNext) || isDirectiveLine(ltrim(rawNext))) break;

    field += '\n';
    field += stripIndentMarker(rawNext);
    lines.next();
  }
}

ClozeStatus toCloze(std::string& input) {
  std::string result;
  result.reserve(input.size());
  for (size_t i{0}; i < input.size(); ++i) {
    // A marker is a run of digits followed directly by '{'.
    size_t digitsEnd = i;
    while (digitsEnd < input.size() && isDigit(input[digitsEnd])) {
      ++digitsEnd;
    }
    if (digitsEnd == i || digitsEnd == input.size() ||
        input[digitsEnd] != '{') {
      result += input[i];
      continue;
    }

    int  clozeNumber{};
    auto parsed = std::from_chars(input.data() + i, input.data() + digitsEnd,
                                  clozeNumber);
    if (parsed.ec != std::errc{} || clozeNumber < kMinClozeNumber ||
        clozeNumber > kMaxClozeNumber) {
      return ClozeStatus::NumberOutOfRange;
    }

    size_t clozeStart = digitsEnd + 1;
    size_t nextStart  = input.find('{', clozeStart);
    size_t clozeEnd   = input.find('}', clozeStart);
    if (clozeEnd == std::string::npos ||
        (nextStart != std::string::npos && clozeEnd > nextStart)) {
      return ClozeStatus::UnclosedBracket;
    }

    result += "{{c" + std::to_string(clozeNumber) +
              "::" + input.substr(clozeStart, clozeEnd - clozeStart) + "}}";
    i = clozeEnd;
  }
  input = std::move(result);
  return ClozeStatus::Ok;
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

    if (line.empty()) continue;   // Skip empty lines
    if (line.starts_with('#')) {  // If line is a header
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
      appendContinuation(lines, front);
      std::string back;
      if (parsePair(lines, back, "a:")) {
        appendContinuation(lines, back);
        res.cards.emplace_back(currentDeck, currentTags, CardType::QA, front,
                               back);
      } else {
        res.errors.push_back(
            {origin, lineNumber,
             "Missing answer pair to question: '" + front + "'"});
      }
    } else if (lowerLine.starts_with("qr:")) {
      std::string front{ltrim(line.substr(3))};
      appendContinuation(lines, front);
      std::string back;
      if (parsePair(lines, back, "ar:")) {
        appendContinuation(lines, back);
        res.cards.emplace_back(currentDeck, currentTags, CardType::QAR, front,
                               back);
      } else {
        res.errors.push_back(
            {origin, lineNumber,
             "Missing answer pair to question: '" + front + "'"});
      }
    } else if (lowerLine.starts_with("c:")) {
      std::string front{ltrim(line.substr(2))};
      appendContinuation(lines, front);
      switch (toCloze(front)) {
        case ClozeStatus::Ok:
          res.cards.emplace_back(currentDeck, currentTags, CardType::Cloze,
                                 front, "");
          break;
        case ClozeStatus::UnclosedBracket:
          res.errors.push_back(
              {origin, lineNumber,
               "Missing cloze deletion closing bracket: '" + front + "'"});
          break;
        case ClozeStatus::NumberOutOfRange:
          res.errors.push_back(
              {origin, lineNumber,
               "Cloze number must be between " +
                   std::to_string(kMinClozeNumber) + " and " +
                   std::to_string(kMaxClozeNumber) + ": '" + front + "'"});
          break;
      }
    }
  }
  return res;
}
