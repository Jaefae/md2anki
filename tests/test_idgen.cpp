#include "idgen.h"
#include <catch2/catch_test_macros.hpp>
#include <set>

TEST_CASE("generateId returns 8 lowercase hex characters", "[idgen]") {
  std::string id = generateId({});
  CHECK(id.size() == 8);
  CHECK(id.find_first_not_of("0123456789abcdef") == std::string::npos);
}

TEST_CASE("generateId never returns an id already in taken", "[idgen]") {
  std::set<std::string> taken{"00000000"};
  int                   call = 0;
  IdRng                 rng  = [&call]() { return call++ == 0 ? 0u : 1u; };

  CHECK(generateId(taken, rng) == "00000001");
}

TEST_CASE("generateId retries past more than one collision", "[idgen]") {
  std::set<std::string> taken{"00000000", "00000001"};
  int                   call = 0;
  IdRng                 rng  = [&call]() { return call++; };

  CHECK(generateId(taken, rng) == "00000002");
}
