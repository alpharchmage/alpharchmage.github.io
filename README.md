# 名字竞技场

这是“名字竞技场”的必要源码仓库。战斗规则由 `cpp/name_arena.cpp` 编译为 WebAssembly；React/TypeScript 只负责提交名单并播放 C++ 输出的命令和快照。

## 运行

安装 Node.js 与 pnpm 后执行：

```bash
pnpm install --frozen-lockfile
pnpm dev
```

若修改了 C++ 战斗核心，请先确保 Emscripten 已可用，再执行：

```bash
bash cpp/build_wasm.sh
pnpm test
pnpm check
pnpm build
```

仓库特意不包含 `node_modules`、`dist`、运行日志及托管平台发布产物。
