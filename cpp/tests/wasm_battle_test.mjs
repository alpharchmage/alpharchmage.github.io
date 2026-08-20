import path from "node:path";
import { readFile } from "node:fs/promises";
import { fileURLToPath } from "node:url";

globalThis.__dirname = path.dirname(fileURLToPath(import.meta.url));

const { default: createNameArenaModule } = await import("../../client/src/wasm/name_arena_core.mjs");
const wasmPath = fileURLToPath(new URL("../../client/src/wasm/name_arena_core.wasm", import.meta.url));
const wasmBinary = await readFile(wasmPath);

const module = await createNameArenaModule({
  wasmBinary,
  locateFile: (fileName) => new URL(`../../client/src/wasm/${fileName}`, import.meta.url).pathname,
});

const responseText = module.ccall("name_arena_simulate_battle", "string", ["string"], ["甲\n\n乙"]);
const response = JSON.parse(responseText);

if (response.error) {
  throw new Error(response.error);
}

if (!Array.isArray(response.commands) || response.commands.length === 0) {
  throw new Error("WASM 未返回完整战斗播放指令。");
}

if (!response.commands.some((command) => command.text === "攻击" && command.renderTone === 2)) {
  throw new Error("WASM 未返回蓝色攻击指令。");
}

if (!response.commands.some((command) => command.text.includes("获胜"))) {
  throw new Error("WASM 未返回胜负结算指令。");
}

console.log(`wasm_battle_test passed: ${response.commands.length} commands, winner team ${response.winnerTeamId}`);
