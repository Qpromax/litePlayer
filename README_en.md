English | [中文](README.md)
# litePlayer

A modern C++ lightweight media player built with C++20, CMake, and Conan.

## Requirements

- C++20 compatible compiler (e.g., GCC 11+, Clang 14+, MSVC 2022+)
- CMake 3.20 or higher
- Conan 2.0 or higher

## Quick Start

### 1. Install Conan

Using pip:
```bash
pip install conan
```

Linux (apt):
```bash
sudo apt install conan
```

macOS:
```bash
pip install conan
```

Windows:
```bash
winget install conan
```

### 2. Build the Project

Run the following commands in the project root directory:

```bash
conan profile detect
conan install --build=missing   # Downloads pre-built dependencies, or builds from source if missing
cmake --preset conan-release
cmake --build --preset conan-release
```

### 3. Executable

After a successful build, the executable is located under `build/Release`:

```bash
./build/Release/litePlayer
```

## Project Structure

```
.
├── build/              # Build output directory
│   └── Release/ 
│       └── litePlayer 
├── main.cpp            # Source file
```
