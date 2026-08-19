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

  /// Opaque cursor position, for speculative lookahead that may be undone.
  struct State {
    size_t pos;
    size_t lineNumber;
  };
  State checkpoint() const { return {pos_, lineNumber_}; }
  void  restore(State state) {
    pos_        = state.pos;
    lineNumber_ = state.lineNumber;
  }

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

/// True for a line that starts a new header or card (`#`, `Q:`, `Qr:`, `C:`,
/// `A:`, `Ar:`), checked case-insensitively. `line` should already be
/// left-trimmed.
bool isDirectiveLine(std::string_view line);

/// Appends indented lines following the current position onto `field`,
/// letting a `Q:`/`A:`/`C:` field span multiple lines. A line only continues
/// the field if it's indented (leading space or tab) in the source; a
/// directive line or a dedented line always stops it. A run of blank lines is
/// tolerated (kept as blank lines in `field`) as long as the next non-blank
/// line is still indented and not a directive — otherwise the blank lines are
/// left unconsumed and continuation stops.
void appendContinuation(LineCursor& lines, std::string& field);

/// Parses markdown source held in memory. `origin` only labels the errors it
/// produces; nothing is read from disk.
ParseResult parseMarkdown(std::string_view text,
                          const std::filesystem::path& origin = {});
