# md2anki

[![md2anki CI](https://github.com/Jaefae/md2anki/actions/workflows/ci.yml/badge.svg)](https://github.com/Jaefae/md2anki/actions/workflows/ci.yml)

A command-line tool that converts a lightweight, markdown-like plaintext format into an CSV deck importable to Anki.

See [Releases](https://github.com/Jaefae/md2anki/releases) for pre-compiled binaries.

## Features

- Simple `Q:`/`A:` and cloze (`C:`) card syntax
- Reversible question/answer cards (`Qr:`/`Ar:`)
- Deck and tag assignment per section
- Multiple cloze deletions per card (`1{...}`, `2{...}`, ...)
- Multi-line fields via indented continuation lines, with blank-line-tolerant code blocks
- Strict mode to fail the build on malformed cards instead of skipping them
- Directory input: recursively converts every `.md` file in a folder into one deck
- Stable card ids (`--write-ids`) so re-importing updates existing notes instead of duplicating them, and flags cards removed from source
- `--anki-connect` pushes cards directly into a running Anki instance via the AnkiConnect add-on, instead of (or alongside) writing a CSV, including automatic deletion of notes for cards removed from source

## Building

Requires CMake 3.x and a C++23 compiler.

```sh
cmake -B build -S .
cmake --build build
```

The `md2anki` executable is placed in `build/`. To also build the Catch2 test suite:

```sh
cmake -B build -S . -DENABLE_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

## Usage

```
md2anki [inputPath] [-o outputPath] [--anki-connect] [flags]
```

- `inputPath` — a `.md` file, or a directory to convert recursively
- `-o outputPath` — destination `.csv` file. Optional if `--anki-connect` is given, but at least one of the two is required.
- `--anki-connect` — push cards directly into a running Anki instance via the [AnkiConnect](https://ankiweb.net/shared/info/2055492159) add-on, instead of or alongside `-o`. Implies `--write-ids`. Anki (with the AnkiConnect add-on installed) must be running; if it isn't reachable yet, md2anki waits and retries every few seconds until it is — press Ctrl+C to cancel instead of waiting. Decks referenced by `#deck:` that don't exist yet in Anki are created automatically.
- `--anki-connect-url url` — override the AnkiConnect URL (default `http://127.0.0.1:8765`)
- `-s`, `--strict` — abort without writing output if any card fails to parse
- `--write-ids` — assign an id to every `Q:`/`Qr:`/`C:` card missing one and write it back into the source (implies `--strict`, so a run with parse errors never touches your files)

### Example

Given `example.md`:

```md
#deck: Example Deck
#tags: tips
Q: Will the tool support closures?
A: Yes

#tags: tips, closures
C: They will look something like 1{this}.
C: You can even have 1{multiple} 2{closures}!
```

```sh
md2anki example.md -o out.csv
```

produces `out.csv`, ready to import into Anki via **File > Import**:

```
#separator:Comma
#html:false
#deck column:1
#tags column:2
#notetype column:3
"Example Deck","tips",Basic,"Will the tool support closures?","Yes"
"Example Deck","tips closures",Cloze,"They will look something like {{c1::this}}."
"Example Deck","tips closures",Cloze,"You can even have {{c1::multiple}} {{c2::closures}}!"
```

## Syntax reference

| Line prefix       | Meaning                                       |
|-------------------|------------------------------------------------|
| `#deck: Name`      | Set the deck for all following cards            |
| `#tags: a, b`      | Set tags (comma or whitespace separated) for all following cards |
| `Q: ... / A: ...`  | Basic question/answer card                      |
| `Qr: ... / Ar: ...`| Reversible question/answer card                 |
| `C: ...`           | Cloze card; use `N{text}` for cloze deletion `N` (`1`-`99`) |
| `Q(id): ...`, `Qr(id): ...`, `C(id): ...` | Same cards, carrying a stable id. Written by `--write-ids`, not meant to be typed by hand. |
| indented continuation line | Extends the previous `Q:`/`A:`/`Qr:`/`Ar:`/`C:` field onto another line. Indent with a tab or two spaces; a blank line is tolerated as long as the following line is still indented. |

## FAQ

```md
#tags: faq, tips
Q: How do I reset tags/deck assignment?
A: Declare an empty header.

#tags: multiline
Q: What if my cards
	Have a lot of information and
	I can't fit them on one line?

	Or code with empty indentations?
	void foo() {
		bar();

		return;
	}
A: Tab-indented sections are added as newlines on a card.
	They end when the indentation stops, and a blank line inside an indented block doesn't end the card early - only a dedent does.

#tags: faq
Q: Can a card's Q:/A:/C: field span multiple lines?
A: Yes. Indent every continuation line with a tab (or two spaces) and it's folded onto the field. 

Q: What happens to a card that fails to parse (e.g. a missing A: line)?
A: It's skipped, logged as a [WARN], and the rest of the file still converts. Pass -s/--strict to abort the whole run instead.

Q: Can I pass a folder instead of a single file?
A: Yes. Every .md file under it (recursively) is parsed into the same output CSV.

Q: Does the input file need a .md extension?
A: Only when inputPath is a single file, omit it for folders. 

Q: How are tags separated, comma or whitespace?
A: Either. #tags: a, b and #tags: a b are equivalent.

Q: Can I write a literal { in cloze card text?
A: Yes, as long as it's not immediately preceded by a digit — only N{ (digits followed directly by {) starts a cloze deletion.

Q: Which cloze numbers are valid?
A: 1 through 99, matching Anki. 0{...} or 100{...} is reported as an error instead of being converted.

Q: My card imported with no notetype or is missing fields in Anki. What's wrong?
A: Anki must have Basic, Basic (and reversed card), and Cloze note types available, and the CSV import should map the header columns already present in the output file.

Q: Why does re-importing my deck create duplicate notes instead of updating them?
A: Anki's CSV importer matches notes by their first field's text, so editing a question's wording looks like a new card. Run `md2anki ... --write-ids` once to assign every card a stable id; the output CSV then carries a `#guid column` so Anki matches and updates existing notes on re-import instead.

Q: I deleted a card from my notes. Does md2anki clean it up in Anki too?
A: With CSV import, not automatically — Anki's CSV import has no way to delete notes. Once you've used `--write-ids`, md2anki tracks every id it has assigned in a `.md2anki-ids` manifest next to your notes (or at the root of the folder, in directory mode) and prints a `[WARN]` for any id that's disappeared from source, so you know which note to remove from Anki by hand. If you sync with `--anki-connect` instead, md2anki deletes the corresponding Anki note(s) automatically via AnkiConnect, using the same manifest to detect which ids disappeared from source.

Q: How does `--anki-connect` find the right note to update, if Anki's own note ids aren't the same as md2anki's?
A: Every note synced via `--anki-connect` gets a hidden `md2anki-id:<id>` tag matching its stable id. Re-running the sync looks notes up by that tag to decide whether to update an existing note or add a new one, so the notes' Anki-internal ids never need to be tracked separately.
```

More examples live in [`examples/`](examples/).

## License

[MIT](LICENSE)
