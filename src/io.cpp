#include "io.h"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <map>
#include <set>

#include "card.h"
#include "idgen.h"
#include "util.h"

namespace fs = std::filesystem;

std::vector<fs::path> collectMarkdownFiles(const fs::path& path) {
  std::vector<fs::path> files;
  if (!fs::is_directory(path)) {
    files.emplace_back(path);
    return files;
  }
  for (const auto& entry : fs::recursive_directory_iterator(path)) {
    if (entry.is_regular_file() &&
        toLower(entry.path().extension().string()) == ".md") {
      files.emplace_back(entry.path());
    }
  }
  // Directory iteration order is unspecified; sort so output is reproducible.
  std::sort(files.begin(), files.end());
  return files;
}

bool readTextFile(const fs::path& path, std::string& out) {
  std::ifstream ifile(path);
  if (!ifile.is_open()) return false;
  out.assign(std::istreambuf_iterator<char>(ifile),
             std::istreambuf_iterator<char>());
  return true;
}

ParseResult parseFiles(const Cfg& cfg) {
  ParseResult res;
  for (const auto& file : collectMarkdownFiles(cfg.inputPath)) {
    std::string text;
    if (!readTextFile(file, text)) {
      res.errors.push_back({file, 0, "Could not open file " + file.string()});
      continue;
    }
    ParseResult fileRes = parseMarkdown(text, file);
    for (auto& card : fileRes.cards) {
      card.file = file;
    }
    res.cards.insert(res.cards.end(),
                     std::make_move_iterator(fileRes.cards.begin()),
                     std::make_move_iterator(fileRes.cards.end()));
    res.errors.insert(res.errors.end(),
                      std::make_move_iterator(fileRes.errors.begin()),
                      std::make_move_iterator(fileRes.errors.end()));
  }
  return res;
}

bool saveFile(const Cfg& cfg, ParseResult& res) {
  if (cfg.strictWarn && !res.errors.empty()) {
    return false;
  }
  std::ofstream ofile(cfg.outputPath);
  if (!ofile.is_open()) return false;
  ofile << toCsvDocument(res.cards);
  return true;
}

namespace {
std::string_view tagFor(CardType type) {
  switch (type) {
    case CardType::QA: return "q";
    case CardType::QAR: return "qr";
    case CardType::Cloze: return "c";
  }
  return "";
}

/// Byte offset where `lineNumber` (1-indexed) starts in `text`.
size_t lineStartOffset(const std::string& text, size_t lineNumber) {
  size_t offset = 0;
  for (size_t n = 1; n < lineNumber; ++n) {
    offset = text.find('\n', offset);
    if (offset == std::string::npos) return std::string::npos;
    ++offset;
  }
  return offset;
}

/// Splices `(id)` between the tag and colon on `card`'s source line.
bool insertIdMarker(std::string& text, const Card& card) {
  size_t lineStart = lineStartOffset(text, card.lineNumber);
  if (lineStart == std::string::npos) return false;
  size_t lineEnd = text.find('\n', lineStart);
  if (lineEnd == std::string::npos) lineEnd = text.size();

  std::string_view rawLine{text.data() + lineStart, lineEnd - lineStart};
  std::string_view trimmed = ltrim(rawLine);
  size_t           insertAt =
      lineStart + (rawLine.size() - trimmed.size()) + tagFor(card.type).size();

  text.insert(insertAt, "(" + card.id + ")");
  return true;
}
}  // namespace

bool applyWriteIds(ParseResult& res, Manifest& manifest,
                    const fs::path& manifestFile) {
  std::set<std::string> taken = manifest.ids;
  for (const auto& card : res.cards) {
    if (!card.id.empty()) taken.insert(card.id);
  }

  std::map<fs::path, std::vector<Card*>> pending;
  for (auto& card : res.cards) {
    if (card.id.empty()) {
      card.id = generateId(taken);
      taken.insert(card.id);
      pending[card.file].push_back(&card);
    }
  }

  for (auto& [file, cards] : pending) {
    // Patch furthest-down lines first so earlier insertions don't shift the
    // offsets still-to-be-patched cards were computed against.
    std::sort(cards.begin(), cards.end(), [](const Card* a, const Card* b) {
      return a->lineNumber > b->lineNumber;
    });

    std::string text;
    if (!readTextFile(file, text)) return false;
    for (const Card* card : cards) {
      if (!insertIdMarker(text, *card)) return false;
    }

    std::ofstream ofile(file);
    if (!ofile.is_open()) return false;
    ofile << text;
  }

  manifest.ids = std::move(taken);
  writeManifest(manifestFile, manifest);
  return true;
}
