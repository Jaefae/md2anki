# MD2Anki

[![md2anki CI](https://github.com/Jaefae/md2anki/actions/workflows/ci.yml/badge.svg)](https://github.com/Jaefae/md2anki/actions/workflows/ci.yml)

A command-line tool that converts a lightweight, markdown-like plaintext format into an CSV deck importable to Anki.

## Features

- Simple `Q:`/`A:` and cloze (`C:`) card syntax
- Reversible question/answer cards (`Qr:`/`Ar:`)
- Deck and tag assignment per section
- Multiple cloze deletions per card (`1{...}`, `2{...}`, ...)
- Multi-line fields via indented continuation lines, with blank-line-tolerant code blocks
- Strict mode to fail the build on malformed cards instead of skipping them
- Directory input: recursively converts every `.md` file in a folder into one deck

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
md2anki [inputPath] -o [outputPath] [flags]
```

- `inputPath` — a `.md` file, or a directory to convert recursively
- `-o outputPath` — destination `.csv` file (required)
- `-s`, `--strict` — abort without writing output if any card fails to parse

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
```

More examples live in [`examples/`](examples/).

## License

[MIT](LICENSE)
