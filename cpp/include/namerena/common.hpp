#pragma once

// 本文件是所有 C++ 模块共同包含的基础配置。
// 本地 Windows/Linux 编译沿用你要求的 bits/stdc++.h；
// Emscripten 的标准库没有该 GNU 专用头，因此 WASM 目标改为显式引入实际会用到的标准组件。
#ifdef __EMSCRIPTEN__
#include<array>
#include<cstdint>
#include<iomanip>
#include<memory>
#include<set>
#include<sstream>
#include<string>
#include<vector>
#else
#include<bits/stdc++.h>
#ifdef _WIN32
// Windows 本地环境可直接获得 Windows API；当前核心不依赖具体 Windows 函数。
#include<windows.h>
#endif
#endif

using namespace std;

// 按项目码风，普通整数统一写 int；这里将它扩展为 long long。
// 处理 SHA-256 时会在需要 32 位溢出的地方显式转换到 uint32_t。
#define int long long
