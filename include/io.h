#pragma once
#include <filesystem>
#include <string>
#include <vector>

#include "cli.h"
#include "parser.h"

/// Every .md file under `path` (recursively, sorted), or `path` itself when it
/// is not a directory.
std::vector<std::filesystem::path> collectMarkdownFiles(
    const std::filesystem::path& path);

/// Reads a whole file into `out`. Returns false, leaving `out` untouched, when
/// the file cannot be opened.
bool readTextFile(const std::filesystem::path& path, std::string& out);

/// Reads the files named by `cfg` and parses each with parseMarkdown().
ParseResult parseFiles(const Cfg& cfg);

bool saveFile(const Cfg& cfg, ParseResult& res);
