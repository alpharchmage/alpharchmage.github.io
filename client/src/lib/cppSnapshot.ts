import createNameArenaModule from "@/wasm/name_arena_core.mjs";
import wasmUrl from "@/wasm/name_arena_core.wasm?url";

/**
 * C++/WASM is the only combat authority. React submits raw text and renders the completed C++ response.
 * This file does not derive attributes, advance action gauges, select skills, calculate damage, or decide winners.
 */

export type CppPlayerSnapshot = {
  id: number;
  teamId: number;
  seatId: number;
  inputIndex: number;
  name: string;
  hp: number;
  maxHp: number;
  mana: number;
  maxMana: number;
  physicalAttack: number;
  physicalDefense: number;
  magicAttack: number;
  magicDefense: number;
  wisdom: number;
  speed: number;
  alive?: boolean;
};

export type CppRenderCommand = {
  sourcePlayerId: number;
  targetPlayerId: number;
  skillId: number;
  value: number;
  valueAfter: number;
  renderTone: number;
  frontEndAnimation: string;
  text: string;
  newlineAfter: boolean;
};

export type CppBattleSimulationResponse = {
  rawText: string;
  utf8ByteLength: number;
  transportHash: number;
  winnerTeamId: number;
  momentCount: number;
  executedActionCount: number;
  initialPlayers: CppPlayerSnapshot[];
  finalPlayers: CppPlayerSnapshot[];
  commands: CppRenderCommand[];
};

type WasmRuntime = {
  ccall: (name: string, returnType: "string", argumentTypes: string[], argumentsList: string[]) => string;
};

type WasmFactory = (options: { locateFile: (path: string) => string }) => Promise<WasmRuntime>;

let wasmPromise: Promise<WasmRuntime> | null = null;

async function loadCppCore(): Promise<WasmRuntime> {
  if (!wasmPromise) {
    wasmPromise = (createNameArenaModule as unknown as WasmFactory)({
      locateFile: (path) => path.endsWith(".wasm") ? wasmUrl : path,
    });
  }
  return wasmPromise;
}

export async function requestCppBattleSimulation(rawInput: string): Promise<CppBattleSimulationResponse> {
  const wasm = await loadCppCore();
  const responseText = wasm.ccall("name_arena_simulate_battle", "string", ["string"], [rawInput]);
  const response = JSON.parse(responseText) as CppBattleSimulationResponse | { error?: string };

  if ("error" in response && response.error) {
    throw new Error(response.error);
  }
  if (!("initialPlayers" in response) || !("finalPlayers" in response) || !("commands" in response)) {
    throw new Error("C++ 战斗模块返回了无效数据。");
  }
  return response;
}
