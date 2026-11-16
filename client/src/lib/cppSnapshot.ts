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
  isFamiliar?: boolean;
  isYi?: boolean;
  isHornet?: boolean;
  hornetSilk?: number;
  maxHornetSilk?: number;
  hornetInvulnActions?: number;
  hornetThreadPending?: boolean;
  isUnbreakable?: boolean;
  qi?: number;
  maxQi?: number;
  cangsha?: number;
  maxCangsha?: number;
  internalInjury?: number;
  seekingLayers?: number;
  talismanLayers?: number;
  medicineUses?: number;
  ownerPlayerId?: number;
  magicVulnerability?: number;
  lamentLayers?: number;
  targetLockLayers?: number;
  name: string;
  hp: number;
  maxHp: number;
  mana: number;
  maxMana: number;
  manaRecovery: number;
  physicalAttack: number;
  physicalDefense: number;
  magicAttack: number;
  magicDefense: number;
  wisdom: number;
  speed: number;
  freezeStrength?: number;
  freezeLayers?: number;
  ironwallStrength?: number;
  ironwallLayers?: number;
  defenseDownStrength?: number;
  defenseDownLayers?: number;
  damageDownStrength?: number;
  damageDownLayers?: number;
  squareStrength?: number;
  squareLayers?: number;
  speedUpStrength?: number;
  speedUpLayers?: number;
  speedDownStrength?: number;
  speedDownLayers?: number;
  burnStrength?: number;
  burnLayers?: number;
  poisonStrength?: number;
  poisonLayers?: number;
  parryLayers?: number;
  alive?: boolean;
  isRevived?: boolean;
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
  lamentLayers?: number;
  targetLockLayers?: number;
  qi?: number;
  internalInjury?: number;
  seekingLayers?: number;
  talismanLayers?: number;
  cangsha?: number;
  medicineUses?: number;
  hornetSilk?: number;
  sourceHornetSilk?: number;
  hornetInvulnActions?: number;
  hornetThreadPending?: boolean;
  freezeStrength?: number;
  freezeLayers?: number;
  ironwallStrength?: number;
  ironwallLayers?: number;
  defenseDownStrength?: number;
  defenseDownLayers?: number;
  damageDownStrength?: number;
  damageDownLayers?: number;
  squareStrength?: number;
  squareLayers?: number;
  speedUpStrength?: number;
  speedUpLayers?: number;
  speedDownStrength?: number;
  speedDownLayers?: number;
  burnStrength?: number;
  burnLayers?: number;
  poisonStrength?: number;
  poisonLayers?: number;
  parryLayers?: number;
  alive?: boolean;
  playerName?: string;
  playerMaxHp?: number;
  playerPhysicalAttack?: number;
  playerPhysicalDefense?: number;
  playerMagicAttack?: number;
  playerMagicDefense?: number;
  playerSpeed?: number;
  delayAfterMs?: number;
  newlineAfter: boolean;
};

export type CppBattleSimulationResponse = {
  rawText: string;
  longBattle?: boolean;
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
  ccall: (name: string, returnType: "string", argumentTypes: string[], argumentsList: Array<string | number>) => string;
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

export async function requestCppBattleSimulation(rawInput: string, longBattle = true): Promise<CppBattleSimulationResponse> {
  const wasm = await loadCppCore();
  const responseText = wasm.ccall("name_arena_simulate_battle", "string", ["string", "number"], [rawInput, longBattle ? 1 : 0]);
  const response = JSON.parse(responseText) as CppBattleSimulationResponse | { error?: string };

  if ("error" in response && response.error) {
    throw new Error(response.error);
  }
  if (!("initialPlayers" in response) || !("finalPlayers" in response) || !("commands" in response)) {
    throw new Error("C++ 战斗模块返回了无效数据。");
  }
  return response;
}
