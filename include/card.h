#pragma once
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
  Card(std::string deck, std::vector<std::string> tags, CardType type, std::string front, std::string back){
    this->deck = deck;
    this->tags = tags;
    this->type = type;
    this->front = front;
    this->back = back;
  }
  std::string toCsv();
};
