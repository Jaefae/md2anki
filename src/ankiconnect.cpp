#include "ankiconnect.h"

#include <csignal>
#include <iostream>
#include <mutex>
#include <set>
#include <thread>
#include <unordered_map>

#include "httplib.h"

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace {
volatile std::sig_atomic_t g_interrupted = 0;

void handleSigint(int) { g_interrupted = 1; }

void installHandlerOnce() {
  static std::once_flag once;
  std::call_once(once, [] { std::signal(SIGINT, handleSigint); });
}
}  // namespace

void resetAnkiConnectInterruptForTests() { g_interrupted = 0; }
void raiseAnkiConnectInterruptForTests() { g_interrupted = 1; }

bool defaultPost(const std::string& url, const std::string& body,
                  std::string& outResponse) {
  httplib::Client client(url);
  client.set_connection_timeout(2, 0);
  // Generous: a single call can be a batched "multi" covering an entire
  // vault's worth of note updates, which takes AnkiConnect noticeably longer
  // than a single-note action to process.
  client.set_read_timeout(60, 0);
  auto res = client.Post("/", body, "application/json");
  if (!res || res->status != 200) return false;
  outResponse = res->body;
  return true;
}

bool postWithRetry(const std::string& url, const std::string& body,
                    std::string& outResponse, PostFn postFn,
                    std::chrono::milliseconds interval) {
  installHandlerOnce();
  constexpr auto kPollStep = std::chrono::milliseconds(100);

  while (true) {
    if (g_interrupted) {
      std::cout << "[INFO] AnkiConnect sync cancelled by user." << std::endl;
      return false;
    }
    if (postFn(url, body, outResponse)) return true;

    std::cout << "[INFO] Could not reach AnkiConnect at " << url
               << " -- is Anki running with the AnkiConnect add-on? Retrying in "
               << std::chrono::duration_cast<std::chrono::seconds>(interval).count()
               << "s (Ctrl+C to cancel)..." << std::endl;

    for (auto slept = std::chrono::milliseconds(0); slept < interval;
         slept += kPollStep) {
      if (g_interrupted) {
        std::cout << "[INFO] AnkiConnect sync cancelled by user." << std::endl;
        return false;
      }
      std::this_thread::sleep_for(kPollStep);
    }
  }
}

AnkiConnectReply ankiConnectInvoke(const std::string& action, const json& params,
                                    const std::string& url, PostFn postFn) {
  json request;
  request["action"]  = action;
  request["version"] = 6;
  request["params"]  = params;

  AnkiConnectReply reply;
  std::string      responseBody;
  if (!postWithRetry(url, request.dump(), responseBody, postFn)) {
    reply.error = "cancelled";
    return reply;
  }
  reply.transportOk = true;

  json response = json::parse(responseBody, nullptr, false);
  if (response.is_discarded() || !response.is_object()) {
    reply.error = "invalid response from AnkiConnect";
    return reply;
  }

  json errorField = response.value("error", json(nullptr));
  if (!errorField.is_null()) {
    reply.error = errorField.is_string() ? errorField.get<std::string>() : errorField.dump();
    return reply;
  }

  reply.ok     = true;
  reply.result = response.value("result", json(nullptr));
  return reply;
}

std::string_view modelNameFor(CardType type) {
  switch (type) {
    case CardType::Cloze: return "Cloze";
    case CardType::QAR: return "Basic (and reversed card)";
    case CardType::QA: return "Basic";
  }
  return "Basic";
}

json buildNoteFields(const Card& card) {
  if (card.type == CardType::Cloze) {
    return json{{"Text", card.front}};
  }
  return json{{"Front", card.front}, {"Back", card.back}};
}

std::string idTag(const std::string& id) {
  return std::string(kAnkiConnectIdTagPrefix) + id;
}

std::vector<std::string> tagsWithId(const Card& card) {
  std::vector<std::string> tags = card.tags;
  tags.push_back(idTag(card.id));
  return tags;
}

std::string joinTags(const std::vector<std::string>& tags) {
  std::string out;
  for (size_t i = 0; i < tags.size(); ++i) {
    out += tags[i];
    if (i + 1 != tags.size()) out += ' ';
  }
  return out;
}

json buildAddNotePayload(const Card& card) {
  json note;
  note["deckName"]  = card.deck;
  note["modelName"] = std::string(modelNameFor(card.type));
  note["fields"]    = buildNoteFields(card);
  note["tags"]      = tagsWithId(card);
  return note;
}

std::string findNotesQueryForId(const std::string& id) {
  return "tag:" + idTag(id);
}

namespace {
std::string idFromTagPrefix(const std::string& tag) {
  if (tag.rfind(kAnkiConnectIdTagPrefix, 0) == 0) {
    return tag.substr(kAnkiConnectIdTagPrefix.size());
  }
  return "";
}

/// Anki search syntax: "tag:a or tag:b or tag:c".
std::string combinedFindQuery(const std::vector<std::string>& ids) {
  std::string query;
  for (size_t i = 0; i < ids.size(); ++i) {
    if (i) query += " or ";
    query += findNotesQueryForId(ids[i]);
  }
  return query;
}

/// Looks up every id in `ids` in a single findNotes + notesInfo round trip
/// (instead of one findNotes per id), returning a map from md2anki id to the
/// first matching Anki note id. An id absent from the map has no existing
/// note. Returns false (and logs a [WARN]) only on transport/AnkiConnect
/// failure -- ids simply not found are not an error.
bool lookupExistingNotes(const std::vector<std::string>& ids, const std::string& url,
                          PostFn postFn,
                          std::unordered_map<std::string, int64_t>& outIdToNoteId) {
  if (ids.empty()) return true;

  AnkiConnectReply findReply =
      ankiConnectInvoke("findNotes", json{{"query", combinedFindQuery(ids)}}, url, postFn);
  if (!findReply.ok) {
    std::cout << "[WARN] Could not look up existing Anki notes: " << findReply.error
               << std::endl;
    return false;
  }
  if (!findReply.result.is_array() || findReply.result.empty()) return true;

  AnkiConnectReply infoReply =
      ankiConnectInvoke("notesInfo", json{{"notes", findReply.result}}, url, postFn);
  if (!infoReply.ok) {
    std::cout << "[WARN] Could not read Anki note info: " << infoReply.error << std::endl;
    return false;
  }
  if (!infoReply.result.is_array()) return true;

  for (const auto& note : infoReply.result) {
    if (!note.is_object() || !note.contains("tags") || !note["tags"].is_array()) continue;
    int64_t noteId = note.value("noteId", int64_t{0});
    for (const auto& tagValue : note["tags"]) {
      if (!tagValue.is_string()) continue;
      std::string id = idFromTagPrefix(tagValue.get<std::string>());
      if (id.empty()) continue;
      if (outIdToNoteId.contains(id)) {
        std::cout << "[WARN] Multiple Anki notes tagged " << idTag(id)
                   << "; updating only the first." << std::endl;
      } else {
        outIdToNoteId[id] = noteId;
      }
    }
  }
  return true;
}

/// AnkiConnect's addNotes rejects any note whose deck doesn't already exist
/// (unlike Anki's CSV importer, which creates missing decks implicitly), so
/// every deck referenced by a pending add must be created first. createDeck
/// is idempotent -- it's a no-op, not an error, when the deck already exists.
bool ensureDecksExist(const std::vector<json>& pendingAdds, const std::string& url,
                       PostFn postFn) {
  std::set<std::string> deckNames;
  for (const auto& note : pendingAdds) {
    deckNames.insert(note.at("deckName").get<std::string>());
  }

  bool ok = true;
  for (const auto& deck : deckNames) {
    AnkiConnectReply createReply =
        ankiConnectInvoke("createDeck", json{{"deck", deck}}, url, postFn);
    if (!createReply.ok) {
      std::cout << "[WARN] Could not create Anki deck '" << deck
                 << "': " << createReply.error << std::endl;
      ok = false;
    }
  }
  return ok;
}
}  // namespace

bool syncToAnkiConnect(ParseResult& res, const Manifest& previous, Manifest& manifest,
                        const fs::path& manifestFile, const std::string& url,
                        PostFn postFn) {
  bool                   overallOk = true;
  std::set<std::string>  currentIds;
  std::vector<Card*>     cardsToSync;

  for (auto& card : res.cards) {
    if (card.id.empty()) {
      std::cout << "[WARN] Skipping card with no id (run with --write-ids)." << std::endl;
      overallOk = false;
      continue;
    }
    currentIds.insert(card.id);
    cardsToSync.push_back(&card);
  }

  // One findNotes + one notesInfo round trip covers every card, instead of a
  // separate lookup per card.
  std::unordered_map<std::string, int64_t> idToNoteId;
  {
    std::vector<std::string> ids;
    ids.reserve(cardsToSync.size());
    for (const Card* card : cardsToSync) ids.push_back(card->id);
    if (!lookupExistingNotes(ids, url, postFn, idToNoteId)) overallOk = false;
  }

  std::vector<json> pendingAdds;
  std::vector<json> updateActions;
  for (Card* card : cardsToSync) {
    auto it = idToNoteId.find(card->id);
    if (it == idToNoteId.end()) {
      pendingAdds.push_back(buildAddNotePayload(*card));
      continue;
    }
    int64_t noteId = it->second;
    updateActions.push_back(json{
        {"action", "updateNoteFields"},
        {"params", json{{"note", json{{"id", noteId}, {"fields", buildNoteFields(*card)}}}}}});
    updateActions.push_back(
        json{{"action", "updateNoteTags"},
             {"params", json{{"note", noteId}, {"tags", joinTags(tagsWithId(*card))}}}});
  }

  // All updates ride in a single "multi" call instead of two calls per card.
  if (!updateActions.empty()) {
    AnkiConnectReply multiReply =
        ankiConnectInvoke("multi", json{{"actions", updateActions}}, url, postFn);
    if (!multiReply.ok) {
      std::cout << "[WARN] Could not update Anki notes: " << multiReply.error << std::endl;
      overallOk = false;
    } else if (multiReply.result.is_array()) {
      // On success, a sub-action's entry is its own raw result value (often
      // null, or a scalar/array -- whatever that action normally returns).
      // Only a failed sub-action comes back wrapped as {"result":null,
      // "error":"..."}, so only an object with a non-null "error" key means
      // that specific update failed.
      for (const auto& sub : multiReply.result) {
        if (!sub.is_object()) continue;
        json subError = sub.value("error", json(nullptr));
        if (!subError.is_null()) {
          std::cout << "[WARN] Could not update an Anki note: "
                     << (subError.is_string() ? subError.get<std::string>() : subError.dump())
                     << std::endl;
          overallOk = false;
        }
      }
    }
  }

  if (!pendingAdds.empty()) {
    if (!ensureDecksExist(pendingAdds, url, postFn)) overallOk = false;

    AnkiConnectReply addReply =
        ankiConnectInvoke("addNotes", json{{"notes", pendingAdds}}, url, postFn);
    if (!addReply.ok) {
      std::cout << "[WARN] Could not add new cards to Anki: " << addReply.error
                 << std::endl;
      overallOk = false;
    } else if (addReply.result.is_array()) {
      for (const auto& entry : addReply.result) {
        if (entry.is_null()) {
          std::cout << "[WARN] Anki rejected a new note (possible duplicate)."
                     << std::endl;
        }
      }
    }
  }

  // Ghost deletion also collapses to one findNotes + notesInfo + deleteNotes,
  // rather than a lookup-then-delete pair per removed id.
  std::vector<std::string> ghosts = staleIds(previous, currentIds);
  if (!ghosts.empty()) {
    std::unordered_map<std::string, int64_t> ghostNoteIds;
    if (!lookupExistingNotes(ghosts, url, postFn, ghostNoteIds)) {
      std::cout << "[WARN] Could not look up ghost Anki notes -- will retry next run."
                 << std::endl;
      overallOk = false;
    } else {
      std::vector<int64_t> toDelete;
      for (const auto& id : ghosts) {
        auto it = ghostNoteIds.find(id);
        if (it == ghostNoteIds.end()) {
          manifest.ids.erase(id);  // already gone from Anki
        } else {
          toDelete.push_back(it->second);
        }
      }

      if (!toDelete.empty()) {
        AnkiConnectReply deleteReply =
            ankiConnectInvoke("deleteNotes", json{{"notes", toDelete}}, url, postFn);
        if (deleteReply.ok) {
          for (const auto& id : ghosts) {
            if (ghostNoteIds.contains(id)) {
              manifest.ids.erase(id);
              std::cout << "[INFO] Deleted Anki note(s) for removed card " << id
                         << std::endl;
            }
          }
        } else {
          std::cout << "[WARN] Could not delete Anki note(s) for removed cards"
                        " -- will retry next run: "
                     << deleteReply.error << std::endl;
          overallOk = false;
        }
      }
    }
  }

  writeManifest(manifestFile, manifest);
  return overallOk;
}
