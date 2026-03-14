#include "card.h"
#include <string>
#include <sstream>
#include <string_view>

std::string escapeCSV(const std::string& text){
  std::ostringstream escaped{};
  escaped << '\"';
  for(const char& chr: text){
    if(chr == '\"') {
      escaped << "\"\"";
    } else escaped << chr;
  }
  escaped << '\"';
  return escaped.str();
}
std::string Card::toCsv(){
  std::ostringstream output;
  // Deck
  output << deck << ',';
  // Tags
  for(size_t i{0}; i < tags.size(); i++){
    const std::string_view tag{tags[i]};
    output << tag;
    if(i != tags.size()-1) output << ' ';
  }
  output << ',';
  // Type & Content
  switch(type){
    case(CardType::Cloze):
      output << "Cloze" << ',';
      output << escapeCSV(front);
      break;
    case(CardType::QA):
      output << "Basic" << ',';
      output << escapeCSV(front) << ',';
      output << escapeCSV(back);
      break;
    case(CardType::QAR):
      output << "Basic (and reversed card)" << ',';
      output << escapeCSV(front) << ',';
      output << escapeCSV(back);
      break;
  }
  return output.str();
};
