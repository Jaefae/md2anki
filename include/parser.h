#pragma once
#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "card.h"

struct ParseError {
  std::filesystem::path file;
  size_t                lineNumber;
  std::string           message;
};

struct ParseResult {
  std::vector<Card>       cards;
  std::vector<ParseError> errors;
};

/// Read-only cursor over the lines of an in-memory document. Lines are
/// numbered from 1 and exclude their terminator; a trailing '\r' is dropped so
/// CRLF and LF documents parse identically.
class LineCursor {
 public:
  explicit LineCursor(std::string_view text) : text_{text} {}

  bool atEnd() const { return pos_ >= text_.size(); }
  /// The next unconsumed line, without advancing. Empty when atEnd().
  std::string_view peek() const;
  /// Consumes and returns the next line, bumping lineNumber().
  std::string_view next();
  /// Line number of the most recently consumed line (0 before the first).
  size_t lineNumber() const { return lineNumber_; }

 private:
  std::string_view text_;
  size_t           pos_{0};
  size_t           lineNumber_{0};
};

std::vector<std::string> collectCSV(std::string_view csv);
size_t                   findNot(std::string_view input, char token);
bool                     toCloze(std::string& input);

/// Consumes the next line when it starts with `expectedToken`, writing the
/// remainder into `back`. Leaves the cursor and `back` untouched otherwise, so
/// the caller can reprocess the line.
bool parsePair(LineCursor& lines, std::string& back,
               std::string_view expectedToken);

/// Parses markdown source held in memory. `origin` only labels the errors it
/// produces; nothing is read from disk.
ParseResult parseMarkdown(std::string_view text,
                          const std::filesystem::path& origin = {});
