#include "ankiconnect.h"

#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

#include "httplib.h"
#include "manifest.h"
#include "parser.h"

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace {
Card makeCard(CardType type, const std::string& id, std::string front = "front",
              std::string back = "back", std::string deck = "Deck") {
  std::vector<std::string> tags{"t1"};
  Card                     card(deck, tags, type, front, back);
  card.id = id;
  return card;
}

std::string actionOf(const std::string& body) {
  return json::parse(body).value("action", std::string());
}

fs::path tempManifestPath(const std::string& name) {
  return fs::temp_directory_path() / name;
}
}  // namespace

// --- Payload builders -------------------------------------------------

TEST_CASE("modelNameFor maps CardType to Anki notetype names", "[ankiconnect]") {
  CHECK(modelNameFor(CardType::QA) == "Basic");
  CHECK(modelNameFor(CardType::QAR) == "Basic (and reversed card)");
  CHECK(modelNameFor(CardType::Cloze) == "Cloze");
}

TEST_CASE("buildNoteFields uses Front/Back for QA and QAR", "[ankiconnect]") {
  Card card = makeCard(CardType::QA, "aaaaaaaa", "Q", "A");
  json fields = buildNoteFields(card);
  CHECK(fields.at("Front") == "Q");
  CHECK(fields.at("Back") == "A");
  CHECK(!fields.contains("Text"));
}

TEST_CASE("buildNoteFields uses Text for Cloze", "[ankiconnect]") {
  Card card = makeCard(CardType::Cloze, "aaaaaaaa", "{{c1::x}}");
  json fields = buildNoteFields(card);
  CHECK(fields.at("Text") == "{{c1::x}}");
  CHECK(!fields.contains("Front"));
}

TEST_CASE("idTag and findNotesQueryForId use the md2anki-id prefix",
          "[ankiconnect]") {
  CHECK(idTag("aaaaaaaa") == "md2anki-id:aaaaaaaa");
  CHECK(findNotesQueryForId("aaaaaaaa") == "tag:md2anki-id:aaaaaaaa");
}

TEST_CASE("tagsWithId appends the id tag to the card's own tags",
          "[ankiconnect]") {
  Card card = makeCard(CardType::QA, "aaaaaaaa");
  auto tags = tagsWithId(card);
  REQUIRE(tags.size() == 2);
  CHECK(tags[0] == "t1");
  CHECK(tags[1] == "md2anki-id:aaaaaaaa");
  CHECK(joinTags(tags) == "t1 md2anki-id:aaaaaaaa");
}

TEST_CASE("buildAddNotePayload includes deck, model, fields and id tag",
          "[ankiconnect]") {
  Card card = makeCard(CardType::QA, "aaaaaaaa", "Q", "A", "MyDeck");
  json note = buildAddNotePayload(card);
  CHECK(note.at("deckName") == "MyDeck");
  CHECK(note.at("modelName") == "Basic");
  CHECK(note.at("fields").at("Front") == "Q");
  CHECK(note.at("tags") == json::array({"t1", "md2anki-id:aaaaaaaa"}));
}

// --- Add-vs-update decision logic --------------------------------------

TEST_CASE("syncToAnkiConnect adds a card with no existing Anki note",
          "[ankiconnect]") {
  ParseResult res;
  res.cards.push_back(makeCard(CardType::QA, "aaaaaaaa"));
  Manifest previous;
  Manifest manifest;
  fs::path manifestFile = tempManifestPath("md2anki_ac_add");

  bool sawFind = false, sawAdd = false, sawUpdate = false;
  PostFn fake = [&](const std::string&, const std::string& body,
                     std::string& outResponse) {
    std::string action = actionOf(body);
    if (action == "findNotes") {
      sawFind = true;
      outResponse = R"({"result": [], "error": null})";
    } else if (action == "createDeck") {
      outResponse = R"({"result": 1, "error": null})";
    } else if (action == "addNotes") {
      sawAdd = true;
      outResponse = R"({"result": [123], "error": null})";
    } else if (action == "updateNoteFields" || action == "updateNoteTags") {
      sawUpdate = true;
      outResponse = R"({"result": null, "error": null})";
    }
    return true;
  };

  bool ok = syncToAnkiConnect(res, previous, manifest, manifestFile,
                               "http://fake", fake);
  fs::remove(manifestFile);

  CHECK(ok);
  CHECK(sawFind);
  CHECK(sawAdd);
  CHECK_FALSE(sawUpdate);
}

TEST_CASE("syncToAnkiConnect creates missing decks before adding notes",
          "[ankiconnect]") {
  ParseResult res;
  res.cards.push_back(makeCard(CardType::QA, "aaaaaaaa", "Q1", "A1", "DeckA"));
  res.cards.push_back(makeCard(CardType::QA, "bbbbbbbb", "Q2", "A2", "DeckB"));
  Manifest previous;
  Manifest manifest;
  fs::path manifestFile = tempManifestPath("md2anki_ac_deck_create");

  std::vector<std::string> createdDecks;
  bool                     sawAddNotes = false;
  PostFn fake = [&](const std::string&, const std::string& body,
                     std::string& outResponse) {
    json request = json::parse(body);
    std::string action = request.value("action", std::string());
    if (action == "findNotes") {
      outResponse = R"({"result": [], "error": null})";
    } else if (action == "createDeck") {
      createdDecks.push_back(request["params"]["deck"].get<std::string>());
      outResponse = R"({"result": 1, "error": null})";
    } else if (action == "addNotes") {
      sawAddNotes = true;
      outResponse = R"({"result": [111, 112], "error": null})";
    }
    return true;
  };

  bool ok = syncToAnkiConnect(res, previous, manifest, manifestFile,
                               "http://fake", fake);
  fs::remove(manifestFile);

  CHECK(ok);
  CHECK(sawAddNotes);
  REQUIRE(createdDecks.size() == 2);
  CHECK(std::find(createdDecks.begin(), createdDecks.end(), "DeckA") !=
        createdDecks.end());
  CHECK(std::find(createdDecks.begin(), createdDecks.end(), "DeckB") !=
        createdDecks.end());
}

TEST_CASE("syncToAnkiConnect updates a card matched by its id tag",
          "[ankiconnect]") {
  ParseResult res;
  res.cards.push_back(makeCard(CardType::QA, "aaaaaaaa"));
  Manifest previous{"src", {"aaaaaaaa"}};
  Manifest manifest = previous;
  fs::path manifestFile = tempManifestPath("md2anki_ac_update");

  bool sawAdd = false;
  int  updateFieldsNoteId = -1;
  int  updateTagsNoteId   = -1;
  PostFn fake = [&](const std::string&, const std::string& body,
                     std::string& outResponse) {
    std::string action = actionOf(body);
    if (action == "findNotes") {
      outResponse = R"({"result": [555], "error": null})";
    } else if (action == "addNotes") {
      sawAdd = true;
      outResponse = R"({"result": [], "error": null})";
    } else if (action == "updateNoteFields") {
      updateFieldsNoteId = json::parse(body)["params"]["note"]["id"].get<int>();
      outResponse = R"({"result": null, "error": null})";
    } else if (action == "updateNoteTags") {
      updateTagsNoteId = json::parse(body)["params"]["note"].get<int>();
      outResponse = R"({"result": null, "error": null})";
    }
    return true;
  };

  bool ok = syncToAnkiConnect(res, previous, manifest, manifestFile,
                               "http://fake", fake);
  fs::remove(manifestFile);

  CHECK(ok);
  CHECK_FALSE(sawAdd);
  CHECK(updateFieldsNoteId == 555);
  CHECK(updateTagsNoteId == 555);
}

TEST_CASE("syncToAnkiConnect updates only the first note on multiple matches",
          "[ankiconnect]") {
  ParseResult res;
  res.cards.push_back(makeCard(CardType::QA, "aaaaaaaa"));
  Manifest previous{"src", {"aaaaaaaa"}};
  Manifest manifest = previous;
  fs::path manifestFile = tempManifestPath("md2anki_ac_multi");

  int updateFieldsNoteId = -1;
  PostFn fake = [&](const std::string&, const std::string& body,
                     std::string& outResponse) {
    std::string action = actionOf(body);
    if (action == "findNotes") {
      outResponse = R"({"result": [555, 556], "error": null})";
    } else if (action == "updateNoteFields") {
      updateFieldsNoteId = json::parse(body)["params"]["note"]["id"].get<int>();
      outResponse = R"({"result": null, "error": null})";
    } else {
      outResponse = R"({"result": null, "error": null})";
    }
    return true;
  };

  bool ok = syncToAnkiConnect(res, previous, manifest, manifestFile,
                               "http://fake", fake);
  fs::remove(manifestFile);

  CHECK(ok);
  CHECK(updateFieldsNoteId == 555);
}

// --- Ghost deletion -----------------------------------------------------

TEST_CASE("syncToAnkiConnect deletes ghost notes and drops them from the manifest",
          "[ankiconnect]") {
  ParseResult res;  // no current cards -- "deadbeef" is now stale
  Manifest previous{"src", {"deadbeef"}};
  Manifest manifest = previous;
  fs::path manifestFile = tempManifestPath("md2anki_ac_ghost_delete");

  bool sawDelete = false;
  PostFn fake = [&](const std::string&, const std::string& body,
                     std::string& outResponse) {
    std::string action = actionOf(body);
    if (action == "findNotes") {
      outResponse = R"({"result": [999], "error": null})";
    } else if (action == "deleteNotes") {
      sawDelete = true;
      outResponse = R"({"result": null, "error": null})";
    }
    return true;
  };

  bool ok = syncToAnkiConnect(res, previous, manifest, manifestFile,
                               "http://fake", fake);
  Manifest onDisk = readManifest(manifestFile);
  fs::remove(manifestFile);

  CHECK(ok);
  CHECK(sawDelete);
  CHECK_FALSE(manifest.ids.contains("deadbeef"));
  CHECK_FALSE(onDisk.ids.contains("deadbeef"));
}

TEST_CASE("syncToAnkiConnect keeps a ghost id in the manifest when deletion fails",
          "[ankiconnect]") {
  ParseResult res;
  Manifest previous{"src", {"deadbeef"}};
  Manifest manifest = previous;
  fs::path manifestFile = tempManifestPath("md2anki_ac_ghost_fail");

  PostFn fake = [&](const std::string&, const std::string& body,
                     std::string& outResponse) {
    std::string action = actionOf(body);
    if (action == "findNotes") {
      outResponse = R"({"result": [999], "error": null})";
    } else if (action == "deleteNotes") {
      outResponse = R"({"result": null, "error": "deck was locked"})";
    }
    return true;
  };

  bool ok = syncToAnkiConnect(res, previous, manifest, manifestFile,
                               "http://fake", fake);
  fs::remove(manifestFile);

  CHECK_FALSE(ok);
  CHECK(manifest.ids.contains("deadbeef"));
}

// --- Retry / interrupt ---------------------------------------------------

TEST_CASE("postWithRetry retries transport failures until success",
          "[ankiconnect]") {
  resetAnkiConnectInterruptForTests();
  int    calls = 0;
  PostFn fake = [&](const std::string&, const std::string&, std::string& outResponse) {
    ++calls;
    if (calls < 3) return false;
    outResponse = "ok";
    return true;
  };

  std::string outResponse;
  bool ok = postWithRetry("http://fake", "{}", outResponse, fake,
                           std::chrono::milliseconds(10));

  CHECK(ok);
  CHECK(calls == 3);
  CHECK(outResponse == "ok");
}

TEST_CASE("postWithRetry returns false immediately when interrupted",
          "[ankiconnect]") {
  raiseAnkiConnectInterruptForTests();
  int    calls = 0;
  PostFn fake = [&](const std::string&, const std::string&, std::string&) {
    ++calls;
    return true;
  };

  std::string outResponse;
  bool ok = postWithRetry("http://fake", "{}", outResponse, fake,
                           std::chrono::milliseconds(10));

  CHECK_FALSE(ok);
  CHECK(calls == 0);
  resetAnkiConnectInterruptForTests();
}

// --- Real transport integration ------------------------------------------

TEST_CASE("ankiConnectInvoke round-trips through a real embedded HTTP server",
          "[ankiconnect]") {
  resetAnkiConnectInterruptForTests();
  httplib::Server server;
  server.Post("/", [](const httplib::Request& req, httplib::Response& res) {
    json request = json::parse(req.body);
    json response;
    response["error"] = nullptr;
    if (request["action"] == "findNotes") {
      response["result"] = json::array({42});
    } else {
      response["result"] = nullptr;
    }
    res.set_content(response.dump(), "application/json");
  });

  int port = server.bind_to_any_port("127.0.0.1");
  std::thread serverThread([&] { server.listen_after_bind(); });
  while (!server.is_running()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }

  std::string url = "http://127.0.0.1:" + std::to_string(port);
  AnkiConnectReply reply =
      ankiConnectInvoke("findNotes", json{{"query", "tag:md2anki-id:x"}}, url,
                         defaultPost);

  server.stop();
  serverThread.join();

  REQUIRE(reply.ok);
  REQUIRE(reply.result.is_array());
  CHECK(reply.result.at(0) == 42);
}
