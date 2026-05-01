[English](README_en.md) | 中文
# litePlyer

基于 C++23 和 xmake 的现代 C++ 轻量级播放器

## 环境要求

- C++23 兼容的编译器（如 GCC 13+、Clang 16+、MSVC 2022+）
- xmake（2.8+）

## 配置

### 1. 安装 xmake

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

### 2. 构建项目

在项目根目录下执行：

```bash
xmake f -m release
xmake
```

### 3. 生成 clangd 配置（可选）

如果编辑器里 clangd 出现误报或找不到头文件，执行：

```bash
xmake project -k compile_commands
```

会在项目根目录生成 `compile_commands.json`，供 clangd 正确索引工程。

### 4. 大致结构

```
.
├── build/              // 构建目录
│   └── <plat>/<arch>/release/
│                     └── litePlayer
├── compile_commands.json   // clangd 索引配置（可选生成）
├── main.cpp            // 源文件
```
