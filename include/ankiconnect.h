#pragma once
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "card.h"
#include "manifest.h"
#include "parser.h"
#include "json.hpp"

inline constexpr std::string_view kDefaultAnkiConnectUrl = "http://127.0.0.1:8765";
inline constexpr std::string_view kAnkiConnectIdTagPrefix = "md2anki-id:";

/// Low-level transport: does exactly one HTTP POST of `body` to `url`,
/// writing the raw response body into `outResponse`. Returns false on any
/// connection-level failure (refused, timeout, DNS, etc). The default
/// implementation wraps httplib::Client; tests inject a fake instead.
using PostFn = std::function<bool(const std::string& url, const std::string& body,
                                   std::string& outResponse)>;

bool defaultPost(const std::string& url, const std::string& body,
                  std::string& outResponse);

/// Resets the SIGINT-triggered interrupt flag. Exposed for tests only.
void resetAnkiConnectInterruptForTests();
/// Marks the interrupt flag as raised without a real SIGINT. Exposed for
/// tests only.
void raiseAnkiConnectInterruptForTests();

/// Posts `body` to `url`, retrying indefinitely at `interval` until either
/// the post succeeds (returns true, response in `outResponse`) or the user
/// interrupts with Ctrl+C (returns false). Prints an [INFO] status line
/// before each retry.
bool postWithRetry(const std::string& url, const std::string& body,
                    std::string& outResponse, PostFn postFn = defaultPost,
                    std::chrono::milliseconds interval = std::chrono::seconds(3));

/// Result of one AnkiConnect action call.
struct AnkiConnectReply {
  bool transportOk = false;  ///< false only if interrupted before a response was ever obtained
  bool ok = false;           ///< true if transportOk && response "error" is null
  nlohmann::json result;     ///< response["result"], if ok
  std::string error;         ///< response["error"] (or a transport/parse message), if !ok
};

/// Builds {"action":action, "version":6, "params":params}, POSTs it via
/// postWithRetry, and parses the JSON response.
AnkiConnectReply ankiConnectInvoke(const std::string& action, const nlohmann::json& params,
                                    const std::string& url = std::string(kDefaultAnkiConnectUrl),
                                    PostFn postFn = defaultPost);

// --- Payload builders (pure, unit-testable without any transport) ---
std::string_view         modelNameFor(CardType type);
nlohmann::json           buildNoteFields(const Card& card);
std::string              idTag(const std::string& id);
std::vector<std::string> tagsWithId(const Card& card);
std::string              joinTags(const std::vector<std::string>& tags);
nlohmann::json           buildAddNotePayload(const Card& card);
std::string              findNotesQueryForId(const std::string& id);

/// Adds/updates every card in `res.cards` in Anki (matching existing notes by
/// their md2anki-id tag) and deletes ghost notes for ids present in
/// `previous` but absent from the cards currently parsed. Mutates
/// `manifest.ids` (removing successfully-deleted ghost ids) and rewrites
/// `manifestFile`. Every card in `res.cards` is expected to already have an
/// id (caller should run applyWriteIds first); cards without one are skipped
/// with a [WARN]. Returns false if any card/ghost failed to sync (including
/// user cancellation via Ctrl+C).
bool syncToAnkiConnect(ParseResult& res, const Manifest& previous, Manifest& manifest,
                        const std::filesystem::path& manifestFile,
                        const std::string& url = std::string(kDefaultAnkiConnectUrl),
                        PostFn postFn = defaultPost);
