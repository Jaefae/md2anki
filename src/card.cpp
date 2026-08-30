#include "card.h"

#include <algorithm>
#include <sstream>
#include <string>
#include <vector>

std::string escapeCSV(const std::string& text) {
  std::ostringstream escaped{};
  escaped << '\"';
  for (const char& chr : text) {
    if (chr == '\"') {
      escaped << "\"\"";
    } else
      escaped << chr;
  }
  escaped << '\"';
  return escaped.str();
}
std::string Card::toCsv(bool includeId) const {
  std::ostringstream output;
  if (includeId) {
    output << escapeCSV(id) << ',';
  }
  // Deck
  output << escapeCSV(deck) << ',';
  // Tags
  std::string tagStr;
  for (size_t i{0}; i < tags.size(); i++) {
    tagStr += tags[i];
    if (i != tags.size() - 1) tagStr += ' ';
  }
  output << escapeCSV(tagStr) << ',';
  // Type & Content
  switch (type) {
    case (CardType::Cloze):
      output << "Cloze" << ',';
      output << escapeCSV(front);
      break;
    case (CardType::QA):
      output << "Basic" << ',';
      output << escapeCSV(front) << ',';
      output << escapeCSV(back);
      break;
    case (CardType::QAR):
      output << "Basic (and reversed card)" << ',';
      output << escapeCSV(front) << ',';
      output << escapeCSV(back);
      break;
  }
  return output.str();
};

std::string toCsvDocument(const std::vector<Card>& cards) {
  bool hasIds = std::any_of(cards.begin(), cards.end(),
                            [](const Card& card) { return !card.id.empty(); });

  std::ostringstream output;
  output << "#separator:Comma\n";
  output << "#html:false\n";
  int column = 1;
  if (hasIds) {
    output << "#guid column:" << column++ << "\n";
  }
  output << "#deck column:" << column++ << "\n";
  output << "#tags column:" << column++ << "\n";
  output << "#notetype column:" << column++ << "\n";
  for (const auto& card : cards) {
    output << card.toCsv(hasIds) << "\n";
  }
  return output.str();
}
