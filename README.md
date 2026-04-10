[English](README_en.md) | 中文
# litePlyer

基于 C++20、CMake 和 Conan 的现代 C++ 轻量级播放器

## 环境要求

- C++20 兼容的编译器（如 GCC 11+、Clang 14+、MSVC 2022+）
- CMake 3.20 或更高版本
- Conan 2.0 或更高版本

## 快速开始

### 1. 安装 Conan

pip 安装:
```bash
pip install conan
```

linux(apt):
```bash
sudo apt install conan
```

macos:
```bash
pip install conan
```

windows:
```bash
winget install conan
```
### 2. 构建项目

在项目根目录下执行：

```bash
conan profile detect
conan install --build=missing  // 下载预编译的依赖包, 若没有则从源码构建
cmake --preset conan-release
cmake --build --preset conan-release
```


### 3. 可执行文件

构建完成后，可执行文件位于 `build/Release`下

```bash
./build/Release/litePlayer
```

## 大致结构

```
.
├── build/              // 构建目录
│   └── Release/ 
│       └── litePlayer 
├── main.cpp            // 源文件
├── example.mp4         // 需替换为你自己的视频文件
```
