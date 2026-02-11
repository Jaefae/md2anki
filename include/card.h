#pragma once
#include <vector>
#include <string>
#include <string_view>


enum class CardType {
  QA,
  Cloze,
};

CardType getType(const std::string_view&);

struct Card{
  CardType type;
  std::string front;
  std::string back;
  std::string deck;
  std::vector<std::string> tags;
  Card(CardType type, std::string front, std::string back, std::string deck, std::vector<std::string> tags){
    this->type = type;
    this->front = front;
    this->back = back;
    this->deck = deck;
    this->tags = tags;
  }
  std::string toCsv();
};
