# 名字竞技场（Name Arena）

这是一个由 **C++17/WebAssembly** 结算战斗、由 **React/TypeScript** 播放命令流的名字竞技场。浏览器不计算伤害、概率、行动或胜负；`cpp/name_arena.cpp` 是唯一战斗权威。

## 快速开始

```bash
corepack enable
pnpm install --frozen-lockfile
bash cpp/build_wasm.sh
pnpm dev
```

## 完整验证

```bash
bash cpp/build_wasm.sh && pnpm test && pnpm check && pnpm build && git diff --check
```

## 文档入口

| 文档 | 内容 |
|---|---|
| [`name_arena_update_manual.md`](./name_arena_update_manual.md) | 日常更新、Buff/Debuff 图标、前端动画、WASM、GitHub 镜像、归档与排错的操作手册 |
| [`name_arena_maintenance_guide.md`](./name_arena_maintenance_guide.md) | `name_arena.cpp` 的数据结构、函数索引、颜色协议和前后台 Cmd 流程参考 |
| [`todo.md`](./todo.md) | 本会话的功能与验证历史 |

## 重要边界

修改任何战斗数值、目标、概率、状态或死亡规则时，请先改 C++；需要新前端显示时，再扩展 C++ 的 `Cmd`/JSON 协议、`client/src/lib/cppSnapshot.ts`、`Home.tsx` 与 `index.css`。修改 C++ 后必须重建 `client/src/wasm/name_arena_core.mjs` 和 `.wasm`。
