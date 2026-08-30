#pragma once
#include <filesystem>
#include <vector>
#include <string>
#include <string_view>


enum class CardType {
  QA,
  Cloze,
  QAR,
};

CardType getType(const std::string_view&);
std::string escapeCSV(const std::string& text);

struct Card{
  CardType type;
  std::string front;
  std::string back;
  std::string deck;
  std::vector<std::string> tags;
  std::string id;                    // empty until --write-ids assigns one
  size_t lineNumber{0};              // source line the Q:/Qr:/C: tag starts on
  std::filesystem::path file;        // source file this card was parsed from
  Card(std::string deck, std::vector<std::string>& tags, CardType type, std::string front, std::string back){
    this->deck = deck;
    this->tags = tags;
    this->type = type;
    this->front = front;
    this->back = back;
  }
  std::string toCsv(bool includeId = false) const;
};

/// Renders a full Anki CSV document (import headers + one row per card).
std::string toCsvDocument(const std::vector<Card>& cards);
