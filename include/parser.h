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

/// Anki numbers cloze deletions c1 through c99.
inline constexpr int kMinClozeNumber = 1;
inline constexpr int kMaxClozeNumber = 99;

enum class ClozeStatus {
  Ok,
  UnclosedBracket,   ///< A marker was never closed, or another opened first.
  NumberOutOfRange,  ///< A marker's number falls outside c1-c99.
};

/// Rewrites every `N{text}` marker into Anki's `{{cN::text}}` form, in place.
/// `N` is a run of digits directly followed by '{'. On failure `input` is left
/// unchanged so the caller can report the original text.
ClozeStatus toCloze(std::string& input);

/// Consumes the next line when it starts with `expectedToken`, writing the
/// remainder into `back`. Leaves the cursor and `back` untouched otherwise, so
/// the caller can reprocess the line.
bool parsePair(LineCursor& lines, std::string& back,
               std::string_view expectedToken);

/// Parses markdown source held in memory. `origin` only labels the errors it
/// produces; nothing is read from disk.
ParseResult parseMarkdown(std::string_view text,
                          const std::filesystem::path& origin = {});
