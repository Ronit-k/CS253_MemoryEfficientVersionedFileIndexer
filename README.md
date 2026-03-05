# Memory-Efficient Versioned File Indexer

**CS253 Assignment 1**

<table>
<tr><td><b>Name</b></td><td>Ronit Kumar</td></tr>
<tr><td><b>Roll No</b></td><td>230875</td></tr>
<tr><td><b>Email</b></td><td>ronitk23@iitk.ac.in</td></tr>
</table>

---

## Overview

A command-line C++ tool that builds a word-level index over large text files using a fixed-size buffer (256–1024 KB), ensuring constant I/O memory usage regardless of file size. It supports versioned files and three query types: single word frequency (`word`), top-K most frequent words (`top`), and frequency difference between two versions (`diff`). Missing arguments are interactively prompted at runtime.

---

## Key Features

- Fixed-size buffered file reading (256–1024 KB) — memory does not grow with file size
- Case-insensitive word indexing
- Support for multiple file versions
- Word count, Top-K, and Diff queries
- Interactive mode — prompts for any missing arguments
- Object-oriented C++ design with inheritance, templates, and polymorphism

---

## Getting Started

### 1. Clone and enter the repository

```bash
git clone https://github.com/Ronit-k/CS253_MemoryEfficientVersionedFileIndexer.git
cd CS253_MemoryEfficientVersionedFileIndexer
```

### 2. Download sample data (~109 MB each)

```bash
wget https://media.githubusercontent.com/media/krishna-kumar-bais/CS253-Software-DevOps-Assignment1/main/test_logs.txt
wget https://media.githubusercontent.com/media/krishna-kumar-bais/CS253-Software-DevOps-Assignment1/main/verbose_logs.txt
```

### 3. Compile (requires C++17)

```bash
g++ -std=c++17 -O2 -o analyzer analyser.cpp
```

---

## Usage

### Command-Line Arguments

| Argument | Description |
|----------|-------------|
| `--query` | Query type: `word`, `top`, or `diff` |
| `--buffer` | Buffer size in KB (256–1024) |
| `--file` | Input file path (for `word` / `top`) |
| `--version` | Version label (for `word` / `top`) |
| `--word` | Word to search (for `word` / `diff`) |
| `--top` | Number of top words (for `top`) |
| `--file1`, `--file2` | File paths (for `diff`) |
| `--version1`, `--version2` | Version labels (for `diff`) |

### Word Count Query

```bash
./analyzer --file test_logs.txt --version v1 --buffer 512 --query word --word error
```
```
Indexing v1 [########################################] 100%
Version: v1
Count: 605079
Buffer Size (KB): 256
Execution Time (s): 1.02554
```

### Top-K Query

```bash
./analyzer --file test_logs.txt --version v1 --buffer 512 --query top --top 10
```
```
Indexing v1 [########################################] 100%
Top-10 words in version v1:
devops 1209558
debug 605150
error 605079
info 604266
warning 604149
orderservice 484437
paymentservice 484078
authservice 483842
searchservice 483162
userservice 483125
Buffer Size (KB): 256
Execution Time (s): 0.976015
```

### Diff Query

```bash
./analyzer --file1 test_logs.txt --file2 verbose_logs.txt --version1 v1 --version2 v2 --buffer 512 --query diff --word error
```
```
Indexing v1 [########################################] 100%
Indexing v2 [########################################] 100%
Difference (v2 - v1): -495377
Buffer Size (KB): 256
Execution Time (s): 1.9306
```

---

## Interactive Mode

If any required argument is missing, the program prompts for it instead of throwing an error. Running with no arguments starts a fully guided session:

```bash
./analyzer
```
```
Enter query type (word / diff / top): word
Enter buffer size in KB (256–1024): 512
Enter file path (--file): test_logs.txt
Enter version name (--version): v1
Enter word to search (--word): error
```

Partial arguments also work — only the missing ones are prompted. Execution timing starts after all inputs are collected.

---

## Architecture

- **`BufferedFileReader`** — reads files in fixed-size chunks; never loads the entire file into memory
- **`Tokenizer`** — extracts lowercase alphanumeric words from raw buffer data; handles words spanning chunk boundaries
- **`WordIndex<CountType>`** — template class storing word → frequency mappings in an `unordered_map`
- **`VersionedIndex`** — manages multiple named versions, each backed by a `WordIndex`
- **`Query`** (abstract) → **`WordCountQuery`**, **`DiffQuery`**, **`TopKQuery`** — query hierarchy using virtual dispatch

See the project report for detailed design discussion.

---

## Progress Bar

An optional progress bar is displayed during indexing if `progressbar.h` is present alongside `analyser.cpp`. It is auto-detected at compile time via `__has_include` — if the header is absent, the program compiles and runs identically without it. The progress bar writes to `stderr` so it does not interfere with query output on `stdout`.

```
Indexing v1 [####################--------------------] 50%
```

If `progressbar.h` is not present and you'd like to enable the progress bar, download it with:

```bash
wget https://raw.githubusercontent.com/Ronit-k/CS253_MemoryEfficientVersionedFileIndexer/main/progressbar.h
```

Then recompile to pick it up.

---

## Memory Efficiency

`BufferedFileReader` allocates a single fixed-size buffer (256–1024 KB) and streams the file in chunks — the file is never loaded entirely into memory. The `Tokenizer` handles words that span chunk boundaries using a partial-token accumulator. The `WordIndex` grows only with the number of unique words, not the file size.

---

## Error Handling

- Invalid buffer size (outside 256–1024 KB) → `invalid_argument`
- File not found → `runtime_error`
- Unknown or invalid query type → `invalid_argument`
- Missing required arguments after prompting → `invalid_argument`
- Duplicate version name → `runtime_error`
- Non-positive Top-K value → `invalid_argument`

---

This project was developed as part of the CS253 course at IIT Kanpur.
