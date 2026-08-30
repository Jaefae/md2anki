#pragma once
#include <functional>
#include <set>
#include <string>

using IdRng = std::function<unsigned()>;

/// 8-hex-character id not already present in `taken`. Retries on collision.
/// Pass `rng` to force deterministic output in tests; defaults to a
/// process-wide random source.
std::string generateId(const std::set<std::string>& taken, IdRng rng = {});
