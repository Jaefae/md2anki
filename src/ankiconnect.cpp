#include "ankiconnect.h"

#include <csignal>
#include <iostream>
#include <mutex>
#include <set>
#include <thread>

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
  client.set_read_timeout(5, 0);
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
bool addOrUpdateCard(Card& card, const std::string& url, PostFn postFn,
                      std::vector<json>& pendingAdds) {
  AnkiConnectReply findReply = ankiConnectInvoke(
      "findNotes", json{{"query", findNotesQueryForId(card.id)}}, url, postFn);
  if (!findReply.ok) {
    std::cout << "[WARN] Could not look up Anki note for card " << card.id << ": "
               << findReply.error << std::endl;
    return false;
  }

  const json& noteIds = findReply.result;
  if (!noteIds.is_array() || noteIds.empty()) {
    pendingAdds.push_back(buildAddNotePayload(card));
    return true;
  }

  if (noteIds.size() > 1) {
    std::cout << "[WARN] Multiple Anki notes tagged " << idTag(card.id)
               << "; updating only the first." << std::endl;
  }
  int64_t noteId = noteIds.front().get<int64_t>();

  AnkiConnectReply updateReply = ankiConnectInvoke(
      "updateNoteFields",
      json{{"note", json{{"id", noteId}, {"fields", buildNoteFields(card)}}}}, url,
      postFn);
  if (!updateReply.ok) {
    std::cout << "[WARN] Could not update Anki note for card " << card.id << ": "
               << updateReply.error << std::endl;
    return false;
  }

  AnkiConnectReply tagsReply = ankiConnectInvoke(
      "updateNoteTags",
      json{{"note", noteId}, {"tags", joinTags(tagsWithId(card))}},
      url, postFn);
  if (!tagsReply.ok) {
    std::cout << "[WARN] Could not update tags for card " << card.id << ": "
               << tagsReply.error << std::endl;
    return false;
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
  std::vector<json>      pendingAdds;
  std::set<std::string>  currentIds;

  for (auto& card : res.cards) {
    if (card.id.empty()) {
      std::cout << "[WARN] Skipping card with no id (run with --write-ids)." << std::endl;
      overallOk = false;
      continue;
    }
    currentIds.insert(card.id);
    if (!addOrUpdateCard(card, url, postFn, pendingAdds)) overallOk = false;
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

  for (const auto& id : staleIds(previous, currentIds)) {
    AnkiConnectReply findReply = ankiConnectInvoke(
        "findNotes", json{{"query", findNotesQueryForId(id)}}, url, postFn);
    if (!findReply.ok) {
      std::cout << "[WARN] Could not look up ghost Anki note " << id
                 << " -- will retry next run." << std::endl;
      overallOk = false;
      continue;
    }

    const json& noteIds = findReply.result;
    if (!noteIds.is_array() || noteIds.empty()) {
      manifest.ids.erase(id);
      continue;
    }

    AnkiConnectReply deleteReply =
        ankiConnectInvoke("deleteNotes", json{{"notes", noteIds}}, url, postFn);
    if (deleteReply.ok) {
      manifest.ids.erase(id);
      std::cout << "[INFO] Deleted Anki note(s) for removed card " << id << std::endl;
    } else {
      std::cout << "[WARN] Could not delete Anki note(s) for removed card " << id
                 << " -- will retry next run." << std::endl;
      overallOk = false;
    }
  }

  writeManifest(manifestFile, manifest);
  return overallOk;
}
