English | [中文](README.md)
# litePlayer

A modern C++ lightweight media player built with C++23 and xmake.

## Requirements

- C++23 compatible compiler (e.g., GCC 13+, Clang 16+, MSVC 2022+)
- xmake (2.8+)

## Quick Start

### 1. Install xmake

macOS (Homebrew):
```bash
brew install xmake
```

Linux:
```bash
curl -fsSL https://xmake.io/shget.text | bash
```

Windows (PowerShell):
```bash
irm https://xmake.io/psget.text | iex
```

### 2. Build the Project

Run the following commands in the project root directory:

```bash
xmake f -m release
xmake
```

### 3. Generate clangd config (optional)

If clangd shows false diagnostics or missing headers, run:

```bash
xmake project -k compile_commands
```

This generates `compile_commands.json` in the project root for proper clangd indexing.

### 4. Project Structure

```
.
├── build/                      # Build output directory
│   └── <plat>/<arch>/release/
│                     └── litePlayer
├── compile_commands.json       # clangd index config (optional)
├── main.cpp            # Source file
```
