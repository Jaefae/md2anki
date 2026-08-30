#include "idgen.h"

#include <random>

namespace {
unsigned randomWord() {
  static thread_local std::mt19937                     engine{std::random_device{}()};
  static thread_local std::uniform_int_distribution<unsigned> dist;
  return dist(engine);
}

std::string toHex8(unsigned value) {
  static constexpr char kDigits[] = "0123456789abcdef";
  std::string           hex(8, '0');
  for (int i = 7; i >= 0; --i) {
    hex[i] = kDigits[value & 0xF];
    value >>= 4;
  }
  return hex;
}
}  // namespace

std::string generateId(const std::set<std::string>& taken, IdRng rng) {
  if (!rng) rng = randomWord;
  std::string id;
  do {
    id = toHex8(rng());
  } while (taken.contains(id));
  return id;
}
