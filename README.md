# 名字竞技场

这是一个使用 **C++17/WebAssembly** 结算战斗、使用 React/TypeScript 负责输入与渲染的名字竞技场项目。

浏览器端只提交原始名字文本和展示 C++ 返回的结果。角色属性、行动条、随机目标、技能、伤害、状态与胜负判定均由 C++ 核心计算。当前普通攻击从所有存活敌人中使用顺序相关的确定性随机数选择目标；相同输入顺序会复现结果，改变顺序会改变对局随机过程。

## 目录说明

| 路径 | 用途 |
|---|---|
| `cpp/` | C++ 战斗核心、WebAssembly 构建脚本与回归测试。 |
| `client/` | React/TypeScript 前端源代码，以及浏览器加载的 WASM 模块。 |
| `assets/` 与根目录 `index.html` | 已构建的 GitHub Pages 静态发布文件。 |
| `docs/` | 战斗随机性与项目结构说明。 |

## 本地构建

```bash
pnpm install
bash cpp/build_wasm.sh
pnpm build
```

构建后的 `dist/public/` 是静态发布内容。更新页面时，需要将其中的 `index.html` 和 `assets/` 同步到仓库根目录后提交，以供 GitHub Pages 使用。
