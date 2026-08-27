/** @vitest-environment jsdom */
import { readFileSync } from "node:fs";
import { resolve } from "node:path";
import { act, render } from "@testing-library/react";
import { afterEach, describe, expect, it, vi } from "vitest";
import { BattlePlayback } from "./Home";
import type { CppBattleSimulationResponse } from "../lib/cppSnapshot";

const player = {
  id: 1,
  teamId: 1,
  seatId: 1,
  inputIndex: 1,
  name: "测试者",
  hp: 100,
  maxHp: 100,
  mana: 0,
  maxMana: 200,
  manaRecovery: 20,
  physicalAttack: 100,
  physicalDefense: 40,
  magicAttack: 180,
  magicDefense: 40,
  wisdom: 120,
  speed: 1000,
  alive: true,
};

const command = (frontEndAnimation: string, renderTone: number, value: number, valueAfter: number) => ({
  sourcePlayerId: 1,
  targetPlayerId: 1,
  skillId: 2,
  value,
  valueAfter,
  renderTone,
  frontEndAnimation,
  text: "事件",
  newlineAfter: true,
});

const battle: CppBattleSimulationResponse = {
  rawText: "测试者\n\n对手",
  utf8ByteLength: 0,
  transportHash: 1,
  winnerTeamId: 1,
  momentCount: 1,
  executedActionCount: 1,
  initialPlayers: [player],
  finalPlayers: [{ ...player, hp: 90 }],
  commands: [
    command("thunder_damage", 3, 10, 90),
    command("thunder_damage", 3, 10, 80),
    command("heal", 4, 10, 90),
  ],
};

describe("BattlePlayback life animation integration", () => {
  afterEach(() => vi.useRealTimers());

  it("hides mana and status badges while switching unit bars to the compact layout", () => {
    const statusPlayer = { ...player, freezeStrength: 1, freezeLayers: 3 };
    const statusBattle: CppBattleSimulationResponse = {
      ...battle,
      initialPlayers: [statusPlayer],
      finalPlayers: [statusPlayer],
    };
    const normal = render(<BattlePlayback battle={statusBattle} onRestart={() => {}} />);
    expect(normal.container.querySelector(".mana-meter")).not.toBeNull();
    expect(normal.container.querySelector(".unit-statuses")).not.toBeNull();

    const compact = render(<BattlePlayback battle={statusBattle} compactMode onRestart={() => {}} />);
    expect(compact.container.querySelector(".mana-meter")).toBeNull();
    expect(compact.container.querySelector(".unit-statuses")).toBeNull();
    expect(compact.container.querySelector(".unit-list")?.classList.contains("is-compact")).toBe(true);
    expect(compact.container.querySelector(".unit-readout")?.classList.contains("is-compact")).toBe(true);
  });

  it("does not render fast-action text while keeping dash skill text in gold", () => {
    vi.useFakeTimers();
    const goldBattle: CppBattleSimulationResponse = {
      ...battle,
      commands: [
        { ...command("rage", 0, 5, 1250), text: "测试者发动", newlineAfter: false },
        { ...command("rage", 15, 5, 1250), text: "疾走术", newlineAfter: true },
      ],
    };
    const { container } = render(<BattlePlayback battle={goldBattle} onRestart={() => {}} />);

    expect(Array.from(container.querySelectorAll<HTMLElement>(".tone-gold")).map((element) => element.textContent)).toEqual(["疾走术"]);
    expect(container.textContent).not.toContain("快速行动");
    const css = readFileSync(resolve(process.cwd(), "client/src/index.css"), "utf8");
    expect(css).toContain(".tone-gold");
  });

  it("renders deep-green plague and synchronizes its current-health damage", () => {
    const plagueBattle: CppBattleSimulationResponse = {
      ...battle,
      initialPlayers: [player],
      finalPlayers: [{ ...player, hp: 20 }],
      commands: [
        { ...command("plague", 0, 80, 100), text: "测试者使用", newlineAfter: false },
        { ...command("plague", 14, 80, 100), text: "瘟疫", newlineAfter: false },
        { ...command("plague", 0, 80, 100), text: "，测试者受到", newlineAfter: false },
        { ...command("plague_damage", 3, 80, 20), text: "80", newlineAfter: false },
        { ...command("plague_damage", 0, 80, 20), text: "点伤害", newlineAfter: true },
      ],
    };
    const { container } = render(<BattlePlayback battle={plagueBattle} onRestart={() => {}} />);

    expect(container.querySelector(".tone-poison")?.textContent).toBe("瘟疫");
    expect(container.querySelector(".hp-loss")).not.toBeNull();
  });

  it("renders purple life wheel text and synchronizes the exchanged loss and recovery animations", () => {
    vi.useFakeTimers();
    const caster = { ...player, hp: 20, name: "施法者" };
    const highestEnemy = { ...player, id: 2, teamId: 2, seatId: 2, inputIndex: 2, hp: 80, name: "高血敌人" };
    const lowerEnemy = { ...player, id: 3, teamId: 2, seatId: 3, inputIndex: 3, hp: 60, name: "低血敌人" };
    const lifeWheelBattle: CppBattleSimulationResponse = {
      ...battle,
      initialPlayers: [caster, highestEnemy, lowerEnemy],
      finalPlayers: [{ ...caster, hp: 80 }, { ...highestEnemy, hp: 20 }, lowerEnemy],
      commands: [
        { ...command("life_wheel", 0, 0, 20), sourcePlayerId: 1, targetPlayerId: 2, text: "施法者使用", newlineAfter: false },
        { ...command("life_wheel", 16, 0, 20), sourcePlayerId: 1, targetPlayerId: 2, text: "生命之轮", newlineAfter: false },
        { ...command("life_wheel", 0, 0, 20), sourcePlayerId: 1, targetPlayerId: 2, text: "，施法者与高血敌人的血量互换!", newlineAfter: true },
        { ...command("life_wheel_damage", 0, 0, 20), sourcePlayerId: 1, targetPlayerId: 2, text: "高血敌人失去", newlineAfter: false },
        { ...command("life_wheel_damage", 3, 60, 20), sourcePlayerId: 1, targetPlayerId: 2, text: "60", newlineAfter: false },
        { ...command("life_wheel_damage", 0, 60, 20), sourcePlayerId: 1, targetPlayerId: 2, text: "生命", newlineAfter: true },
        { ...command("life_wheel", 0, 0, 80), sourcePlayerId: 1, targetPlayerId: 1, text: "施法者恢复", newlineAfter: false },
        { ...command("life_wheel_heal", 4, 60, 80), sourcePlayerId: 1, targetPlayerId: 1, text: "60", newlineAfter: false },
        { ...command("life_wheel_heal", 0, 60, 80), sourcePlayerId: 1, targetPlayerId: 1, text: "生命", newlineAfter: true },
      ],
    };
    const { container } = render(<BattlePlayback battle={lifeWheelBattle} onRestart={() => {}} />);
    act(() => vi.advanceTimersByTime(0));

    const units = Array.from(container.querySelectorAll<HTMLElement>(".unit-readout"));
    const casterUnit = units.find((unit) => unit.textContent?.includes("施法者"));
    const highestEnemyUnit = units.find((unit) => unit.textContent?.includes("高血敌人"));
    const lowerEnemyUnit = units.find((unit) => unit.textContent?.includes("低血敌人"));
    expect(container.querySelector(".battle-line")?.textContent).toBe("施法者使用生命之轮，施法者与高血敌人的血量互换!");
    expect(container.querySelector(".tone-life-wheel")?.textContent).toBe("生命之轮");
    expect(lowerEnemyUnit?.querySelector(".hp-loss")).toBeNull();

    act(() => vi.advanceTimersByTime(110));
    expect(highestEnemyUnit?.querySelector(".hp-loss")).not.toBeNull();
    expect(highestEnemyUnit?.querySelector(".hp-values")?.textContent).toContain("20/100");

    act(() => vi.advanceTimersByTime(110));
    act(() => vi.advanceTimersByTime(0));
    expect(container.querySelectorAll(".battle-line")[2]?.textContent).toBe("施法者恢复60生命");
    expect(casterUnit?.querySelector(".hp-heal")).not.toBeNull();
    expect(casterUnit?.querySelector(".hp-values")?.textContent).toContain("80/100");
  });

  it("revives a defeated teammate with a green health animation", () => {
    vi.useFakeTimers();
    const teammate = { ...player, id: 2, seatId: 2, inputIndex: 2, name: "死亡队友", hp: 0, alive: false, freezeStrength: 1, freezeLayers: 3, speedDownStrength: 5, speedDownLayers: 2, burnStrength: 20, burnLayers: 2, poisonStrength: 10, poisonLayers: 1 };
    const reviveBattle: CppBattleSimulationResponse = {
      ...battle,
      initialPlayers: [player, teammate],
      finalPlayers: [player, { ...teammate, hp: 60, alive: true, freezeStrength: 0, freezeLayers: 0, speedDownStrength: 0, speedDownLayers: 0, burnStrength: 0, burnLayers: 0, poisonStrength: 0, poisonLayers: 0 }],
      commands: [
        { ...command("death", 1, 0, 0), targetPlayerId: 2, text: "死亡队友消失了", newlineAfter: true, alive: false },
        { ...command("revive_heal", 4, 60, 60), targetPlayerId: 2, text: "死亡队友复活", newlineAfter: true, alive: true, freezeStrength: 0, freezeLayers: 0, speedDownStrength: 0, speedDownLayers: 0, burnStrength: 0, burnLayers: 0, poisonStrength: 0, poisonLayers: 0 },
      ],
    };
    const { container } = render(<BattlePlayback battle={reviveBattle} onRestart={() => {}} />);

    expect(container.querySelectorAll(".unit-readout")[1]?.classList.contains("is-defeated")).toBe(true);
    act(() => vi.advanceTimersByTime(110));
    act(() => vi.advanceTimersByTime(0));
    expect(container.querySelectorAll(".unit-readout")[1]?.classList.contains("is-defeated")).toBe(false);
    expect(container.querySelectorAll(".unit-readout")[1]?.classList.contains("is-revived")).toBe(true);
    expect(container.querySelectorAll(".battle-line")[1]?.textContent).toBe("死亡队友复活");
    expect(container.textContent).not.toContain("20000行动值");
    expect(container.querySelector(".hp-heal")).not.toBeNull();
    expect(container.querySelectorAll(".unit-readout")[1]?.querySelector(".unit-statuses")).toBeNull();
    expect(container.querySelectorAll(".unit-readout")[1]?.classList.contains("is-poisoned")).toBe(false);
    expect(container.querySelectorAll(".unit-readout")[1]?.querySelector<HTMLElement>(".hp-fill")?.style.transform).toBe("scaleX(0.6)");
    act(() => vi.runOnlyPendingTimers());
  });

  it("renders purple devour text and restores 15% maximum health to the killer", () => {
    vi.useFakeTimers();
    const devourer = { ...player, hp: 25, name: "吞噬者" };
    const defeated = { ...player, id: 2, teamId: 2, seatId: 2, inputIndex: 2, hp: 5, name: "被吞噬者" };
    const devourBattle: CppBattleSimulationResponse = {
      ...battle,
      initialPlayers: [devourer, defeated],
      finalPlayers: [{ ...devourer, hp: 55 }, { ...defeated, hp: 0, alive: false }],
      commands: [
        { ...command("death", 1, 0, 0), sourcePlayerId: 1, targetPlayerId: 2, text: "被吞噬者消失了", newlineAfter: true },
        { ...command("devour", 0, 0, 25), sourcePlayerId: 1, targetPlayerId: 2, text: "吞噬者", newlineAfter: false },
        { ...command("devour", 16, 0, 25), sourcePlayerId: 1, targetPlayerId: 2, text: "吞噬", newlineAfter: false },
        { ...command("devour", 0, 0, 25), sourcePlayerId: 1, targetPlayerId: 2, text: "被吞噬者 吞噬者 ", newlineAfter: false },
        { ...command("devour_heal", 4, 15, 40), sourcePlayerId: 1, targetPlayerId: 1, text: "恢复15%生命", newlineAfter: true },
      ],
    };
    const { container } = render(<BattlePlayback battle={devourBattle} onRestart={() => {}} />);
    act(() => vi.advanceTimersByTime(0));
    act(() => vi.advanceTimersByTime(64));
    act(() => vi.advanceTimersByTime(0));

    const devourerUnit = Array.from(container.querySelectorAll<HTMLElement>(".unit-readout")).find((unit) => unit.textContent?.includes("吞噬者"));
    expect(container.querySelectorAll(".battle-line")[1]?.textContent).toBe("吞噬者吞噬被吞噬者 吞噬者 恢复15%生命");
    expect(container.querySelector(".tone-life-wheel")?.textContent).toBe("吞噬");
    expect(devourerUnit?.querySelector(".hp-heal")).not.toBeNull();
    expect(devourerUnit?.querySelector(".hp-values")?.textContent).toContain("40/100");
  });

  it("keeps faster animations and poison styling while compacting left-side health rows", () => {
    const css = readFileSync(resolve(process.cwd(), "client/src/index.css"), "utf8");
    expect(css).toContain("gap: 8px; padding: 10px 24px");
    expect(css).toContain(".unit-readout { display: grid; gap: 2px; padding: 6px 12px; }");
    expect(css).toContain("height: 6px");
    expect(css).toContain("background: #9e72d1");
    expect(css).toContain("transition: transform .12s cubic-bezier");
    expect(css).not.toMatch(/\.unit-readout\.is-familiar\s*\{[^}]*border(?:-left)?\s*:/);
  });

  it("renders a guard description and synchronizes both split damage events", () => {
    vi.useFakeTimers();
    const attacker = { ...player, id: 1, teamId: 2, seatId: 1, inputIndex: 1, name: "攻击者" };
    const protectedTeammate = { ...player, id: 2, teamId: 1, seatId: 2, inputIndex: 2, name: "受护者" };
    const guardian = { ...player, id: 3, teamId: 1, seatId: 3, inputIndex: 3, name: "守护者" };
    const guardBattle: CppBattleSimulationResponse = {
      ...battle,
      initialPlayers: [attacker, protectedTeammate, guardian],
      finalPlayers: [{ ...attacker }, { ...protectedTeammate, hp: 60 }, { ...guardian, hp: 60 }],
      commands: [
        { ...command("guard", 0, 0, 100), sourcePlayerId: 3, targetPlayerId: 2, text: "守护者", newlineAfter: false },
        { ...command("guard", 10, 0, 100), sourcePlayerId: 3, targetPlayerId: 2, text: "守护", newlineAfter: false },
        { ...command("guard", 0, 0, 100), sourcePlayerId: 3, targetPlayerId: 2, text: "受护者", newlineAfter: true },
        { ...command("normal_attack", 0, 0, 100), sourcePlayerId: 1, targetPlayerId: 2, text: "受护者受到", newlineAfter: false },
        { ...command("normal_attack_damage", 3, 40, 60), sourcePlayerId: 1, targetPlayerId: 2, text: "40", newlineAfter: false },
        { ...command("normal_attack_damage", 0, 40, 60), sourcePlayerId: 1, targetPlayerId: 2, text: "伤害", newlineAfter: true },
        { ...command("normal_attack", 0, 0, 100), sourcePlayerId: 1, targetPlayerId: 3, text: "守护者受到", newlineAfter: false },
        { ...command("normal_attack_damage", 3, 40, 60), sourcePlayerId: 1, targetPlayerId: 3, text: "40", newlineAfter: false },
        { ...command("normal_attack_damage", 0, 40, 60), sourcePlayerId: 1, targetPlayerId: 3, text: "伤害", newlineAfter: true },
      ],
    };
    const { container } = render(<BattlePlayback battle={guardBattle} onRestart={() => {}} />);

    expect(container.querySelector(".battle-line")?.textContent).toBe("守护者守护受护者");
    expect(container.querySelector(".tone-ironwall")?.textContent).toBe("守护");
    act(() => vi.advanceTimersByTime(110));
    expect(container.querySelectorAll(".hp-values")[1]?.textContent).toContain("60/100");
    expect(container.querySelector(".hp-loss")).not.toBeNull();
    act(() => vi.advanceTimersByTime(110));
    expect(container.querySelectorAll(".hp-values")[2]?.textContent).toContain("60/100");
    act(() => vi.runOnlyPendingTimers());
  });

  it("renders gray last stand text while applying its silent status changes", () => {
    const wounded = { ...player, hp: 50, freezeStrength: 1, freezeLayers: 3, poisonStrength: 20, poisonLayers: 2 };
    const lastStandBattle: CppBattleSimulationResponse = {
      ...battle,
      initialPlayers: [wounded],
      finalPlayers: [{ ...wounded, freezeStrength: 0, freezeLayers: 0, poisonStrength: 0, poisonLayers: 0, ironwallStrength: 15, ironwallLayers: 5, speedUpStrength: 15, speedUpLayers: 5 }],
      commands: [
        { ...command("last_stand", 0, 0, 50), text: "测试者发动", newlineAfter: false, freezeStrength: 0, freezeLayers: 0, poisonStrength: 0, poisonLayers: 0, ironwallStrength: 15, ironwallLayers: 5, speedUpStrength: 15, speedUpLayers: 5 },
        { ...command("last_stand", 10, 0, 50), text: "垂死挣扎", newlineAfter: false, freezeStrength: 0, freezeLayers: 0, poisonStrength: 0, poisonLayers: 0, ironwallStrength: 15, ironwallLayers: 5, speedUpStrength: 15, speedUpLayers: 5 },
        { ...command("last_stand", 0, 0, 50), text: "，属性大幅上升!!!", newlineAfter: true, freezeStrength: 0, freezeLayers: 0, poisonStrength: 0, poisonLayers: 0, ironwallStrength: 15, ironwallLayers: 5, speedUpStrength: 15, speedUpLayers: 5 },
      ],
    };
    const { container } = render(<BattlePlayback battle={lastStandBattle} onRestart={() => {}} />);

    const unit = container.querySelector(".unit-readout");
    expect(container.querySelector(".battle-line")?.textContent).toBe("测试者发动垂死挣扎，属性大幅上升!!!");
    expect(container.querySelector(".tone-ironwall")?.textContent).toBe("垂死挣扎");
    expect(unit?.querySelector(".unit-status-freeze")).toBeNull();
    expect(unit?.querySelector(".unit-status-poison")).toBeNull();
    expect(unit?.querySelector(".unit-status-ironwall b")?.textContent).toBe("15/5");
    expect(unit?.querySelector(".unit-status-speed-up b")?.textContent).toBe("15/5");
  });

  it("renders a gray passive counter and applies its red damage animation to the damage source", () => {
    vi.useFakeTimers();
    const attacker = { ...player, id: 1, teamId: 1, seatId: 1, inputIndex: 1, hp: 100, name: "攻击者" };
    const counterer = { ...player, id: 2, teamId: 2, seatId: 2, inputIndex: 2, hp: 90, name: "反击者" };
    const counterBattle: CppBattleSimulationResponse = {
      ...battle,
      initialPlayers: [attacker, counterer],
      finalPlayers: [{ ...attacker, hp: 90 }, counterer],
      commands: [
        { ...command("counter", 0, 0, 100), sourcePlayerId: 2, targetPlayerId: 1, text: "反击者", newlineAfter: false },
        { ...command("counter", 10, 0, 100), sourcePlayerId: 2, targetPlayerId: 1, text: "反击! ", newlineAfter: false },
        { ...command("counter", 0, 0, 100), sourcePlayerId: 2, targetPlayerId: 1, text: "攻击者受到", newlineAfter: false },
        { ...command("counter_damage", 3, 10, 90), sourcePlayerId: 2, targetPlayerId: 1, text: "10", newlineAfter: false },
        { ...command("counter_damage", 0, 10, 90), sourcePlayerId: 2, targetPlayerId: 1, text: "点伤害", newlineAfter: true },
      ],
    };
    const { container } = render(<BattlePlayback battle={counterBattle} onRestart={() => {}} />);
    act(() => vi.advanceTimersByTime(0));

    const attackerUnit = Array.from(container.querySelectorAll<HTMLElement>(".unit-readout")).find((unit) => unit.textContent?.includes("攻击者"));
    expect(container.querySelector(".battle-line")?.textContent).toBe("反击者反击! 攻击者受到10点伤害");
    expect(container.querySelector(".tone-ironwall")?.textContent).toBe("反击! ");
    expect(attackerUnit?.querySelector(".hp-loss")).not.toBeNull();
    expect(attackerUnit?.querySelector(".hp-values")?.textContent).toContain("90/100");
  });

  it("keeps guard split damage and its passive counter on one complete line", () => {
    vi.useFakeTimers();
    const attacker = { ...player, id: 1, teamId: 2, seatId: 1, inputIndex: 1, name: "攻击者" };
    const protectedTeammate = { ...player, id: 2, teamId: 1, seatId: 2, inputIndex: 2, name: "受护者" };
    const guardian = { ...player, id: 3, teamId: 1, seatId: 3, inputIndex: 3, name: "守护者" };
    const guardCounterBattle: CppBattleSimulationResponse = {
      ...battle,
      initialPlayers: [attacker, protectedTeammate, guardian],
      finalPlayers: [{ ...attacker, hp: 90 }, { ...protectedTeammate, hp: 80 }, { ...guardian, hp: 80 }],
      commands: [
        { ...command("guard", 10, 0, 100), sourcePlayerId: 3, targetPlayerId: 2, text: "守护者守护受护者", newlineAfter: true },
        { ...command("normal_attack", 0, 0, 100), sourcePlayerId: 1, targetPlayerId: 2, text: "受护者受到", newlineAfter: false },
        { ...command("normal_attack_damage", 3, 20, 80), sourcePlayerId: 1, targetPlayerId: 2, text: "20", newlineAfter: false },
        { ...command("normal_attack_damage", 0, 20, 80), sourcePlayerId: 1, targetPlayerId: 2, text: "点伤害", newlineAfter: false },
        { ...command("counter", 0, 0, 80), sourcePlayerId: 2, targetPlayerId: 1, text: "受护者", newlineAfter: false },
        { ...command("counter", 10, 0, 80), sourcePlayerId: 2, targetPlayerId: 1, text: "反击! ", newlineAfter: false },
        { ...command("counter", 0, 0, 80), sourcePlayerId: 2, targetPlayerId: 1, text: "攻击者受到", newlineAfter: false },
        { ...command("counter_damage", 3, 10, 90), sourcePlayerId: 2, targetPlayerId: 1, text: "10", newlineAfter: false },
        { ...command("counter_damage", 0, 10, 90), sourcePlayerId: 2, targetPlayerId: 1, text: "点伤害", newlineAfter: false },
        { ...command("normal_attack", 0, 0, 100), sourcePlayerId: 1, targetPlayerId: 3, text: "守护者受到", newlineAfter: false },
        { ...command("normal_attack_damage", 3, 20, 80), sourcePlayerId: 1, targetPlayerId: 3, text: "20", newlineAfter: false },
        { ...command("normal_attack_damage", 0, 20, 80), sourcePlayerId: 1, targetPlayerId: 3, text: "点伤害", newlineAfter: true },
      ],
    };
    const { container } = render(<BattlePlayback battle={guardCounterBattle} onRestart={() => {}} />);
    act(() => vi.advanceTimersByTime(64));
    act(() => vi.advanceTimersByTime(0));

    expect(Array.from(container.querySelectorAll(".battle-line")).map((line) => line.textContent)).toEqual([
      "守护者守护受护者",
      "受护者受到20点伤害受护者反击! 攻击者受到10点伤害守护者受到20点伤害",
    ]);
    expect(container.querySelectorAll(".hp-loss")).toHaveLength(3);
  });

  it("pauses between fragments in short battles while long battles keep a complete line", () => {
    vi.useFakeTimers();
    const fragmentedCommands = [
      { ...command("normal_attack", 0, 0, 100), text: "测试者", newlineAfter: false },
      { ...command("normal_attack", 0, 0, 100), text: "发起攻击", newlineAfter: false },
      { ...command("normal_attack", 0, 0, 100), text: "，敌方受到", newlineAfter: false },
      { ...command("normal_attack_damage", 3, 10, 90), text: "10", newlineAfter: false },
      { ...command("normal_attack_damage", 0, 10, 90), text: "伤害", newlineAfter: true },
    ];
    const shortBattle: CppBattleSimulationResponse = { ...battle, longBattle: false, commands: fragmentedCommands };
    const shortPlayback = render(<BattlePlayback battle={shortBattle} onRestart={() => {}} />);

    expect(shortPlayback.container.querySelector(".battle-line")?.textContent).toBe("测试者");
    act(() => vi.advanceTimersByTime(59));
    expect(shortPlayback.container.querySelector(".battle-line")?.textContent).toBe("测试者");
    act(() => vi.advanceTimersByTime(1));
    expect(shortPlayback.container.querySelector(".battle-line")?.textContent).toBe("测试者发起攻击");
    act(() => vi.advanceTimersByTime(100));
    expect(shortPlayback.container.querySelector(".battle-line")?.textContent).toBe("测试者发起攻击，敌方受到");

    const longBattle: CppBattleSimulationResponse = { ...battle, longBattle: true, commands: fragmentedCommands };
    const longPlayback = render(<BattlePlayback battle={longBattle} onRestart={() => {}} />);
    expect(longPlayback.container.querySelector(".battle-line")?.textContent).toBe("测试者发起攻击，敌方受到10伤害");
  });

  it("keeps damage and healing overlays synchronized with their current event", () => {
    vi.useFakeTimers();
    const { container } = render(<BattlePlayback battle={battle} onRestart={() => {}} />);

    act(() => vi.advanceTimersByTime(0));
    expect(container.querySelectorAll(".hp-loss")).toHaveLength(1);
    expect(container.querySelectorAll(".battle-line")).toHaveLength(1);

    act(() => vi.advanceTimersByTime(13));
    expect(container.querySelectorAll(".hp-loss.is-erasing")).toHaveLength(1);

    act(() => vi.advanceTimersByTime(97));
    act(() => vi.advanceTimersByTime(0));
    expect(container.querySelectorAll(".hp-loss")).toHaveLength(1);
    expect(container.querySelectorAll(".hp-heal")).toHaveLength(0);
    expect(container.querySelectorAll(".battle-line")).toHaveLength(2);

    act(() => vi.advanceTimersByTime(110));
    act(() => vi.advanceTimersByTime(0));
    expect(container.querySelectorAll(".hp-loss")).toHaveLength(0);
    expect(container.querySelectorAll(".hp-heal")).toHaveLength(1);
    expect(container.querySelectorAll(".battle-line")).toHaveLength(3);

    act(() => vi.advanceTimersByTime(110));
    act(() => vi.advanceTimersByTime(0));
    expect(container.querySelectorAll(".hp-loss")).toHaveLength(0);
    expect(container.querySelectorAll(".hp-heal")).toHaveLength(0);
  });

  it("renders the ironwall skill marker in silver", () => {
    const ironwallBattle: CppBattleSimulationResponse = {
      ...battle,
      commands: [{ ...command("ironwall", 10, 0, 0), text: "铁壁" }],
    };
    const { container } = render(<BattlePlayback battle={ironwallBattle} onRestart={() => {}} />);

    const marker = container.querySelector(".tone-ironwall");
    expect(marker?.textContent).toBe("铁壁");
    const shield = marker?.querySelector(".ironwall-icon");
    expect(shield?.className).toBe("ironwall-icon");
    expect(shield?.getAttribute("src")).toContain("data:image/svg+xml;base64,");
  });

  it("renders only the rage label in orange and the ironwall removal marker in silver", () => {
    vi.useFakeTimers();
    const statusBattle: CppBattleSimulationResponse = {
      ...battle,
      commands: [
        { ...command("rage", 0, 10, 1500), text: "测试者", newlineAfter: false },
        { ...command("rage", 12, 10, 1500), text: "暴走", newlineAfter: true },
        { ...command("rage_remove", 0, 0, 1000), text: "测试者从", newlineAfter: false },
        { ...command("rage_remove", 12, 0, 1000), text: "暴走", newlineAfter: false },
        { ...command("rage_remove", 0, 0, 1000), text: "中解除", newlineAfter: true },
        { ...command("ironwall_remove", 0, 0, 40), text: "测试者从", newlineAfter: false },
        { ...command("ironwall_remove", 10, 0, 40), text: "铁壁", newlineAfter: false },
        { ...command("ironwall_remove", 0, 0, 40), text: "中解除", newlineAfter: true },
      ],
    };
    const { container } = render(<BattlePlayback battle={statusBattle} onRestart={() => {}} />);

    expect(container.querySelector(".battle-line")?.textContent).toBe("测试者暴走");
    expect(container.querySelectorAll(".tone-rage")[0]?.textContent).toBe("暴走");
    act(() => vi.advanceTimersByTime(110));
    act(() => vi.advanceTimersByTime(0));
    expect(container.querySelectorAll(".battle-line")[1]?.textContent).toBe("测试者从暴走中解除");
    expect(container.querySelectorAll(".tone-rage")[1]?.textContent).toBe("暴走");

    act(() => vi.advanceTimersByTime(110));
    act(() => vi.advanceTimersByTime(0));
    expect(container.querySelectorAll(".battle-line")[2]?.textContent).toBe("测试者从铁壁中解除");
    expect(container.querySelector(".tone-ironwall")?.textContent).toBe("铁壁");
  });

  it("renders the earthquake skill marker in brown and synchronizes its damage", () => {
    vi.useFakeTimers();
    const earthquakeBattle: CppBattleSimulationResponse = {
      ...battle,
      finalPlayers: [{ ...player, hp: 84 }],
      commands: [
        { ...command("earthquake", 11, 0, 100), text: "地裂术", newlineAfter: true },
        { ...command("earthquake_damage", 3, 16, 84), text: "16", newlineAfter: true },
      ],
    };
    const { container } = render(<BattlePlayback battle={earthquakeBattle} onRestart={() => {}} />);

    expect(container.querySelector(".tone-earthquake")?.textContent).toBe("地裂术");
    act(() => vi.advanceTimersByTime(110));
    act(() => vi.advanceTimersByTime(0));
    expect(container.querySelectorAll(".hp-loss")).toHaveLength(1);
  });

  it("renders stab damage and synchronizes the target health loss", () => {
    vi.useFakeTimers();
    const enemy = { ...player, id: 2, teamId: 2, seatId: 2, inputIndex: 2, name: "敌方" };
    const stabBattle: CppBattleSimulationResponse = {
      ...battle,
      initialPlayers: [player, enemy],
      finalPlayers: [player, { ...enemy, hp: 85 }],
      commands: [
        { ...command("stab", 0, 0, 100), targetPlayerId: 2, text: "测试者", newlineAfter: false },
        { ...command("stab", 3, 0, 100), targetPlayerId: 2, text: "戳刺", newlineAfter: false },
        { ...command("stab", 0, 0, 100), targetPlayerId: 2, text: "，敌方受到", newlineAfter: false },
        { ...command("stab_damage", 3, 15, 85), targetPlayerId: 2, text: "15", newlineAfter: false },
        { ...command("stab_damage", 0, 15, 85), targetPlayerId: 2, text: "伤害", newlineAfter: true },
      ],
    };
    const { container } = render(<BattlePlayback battle={stabBattle} onRestart={() => {}} />);
    act(() => vi.advanceTimersByTime(0));

    const target = Array.from(container.querySelectorAll<HTMLElement>(".unit-readout")).find((unit) => unit.textContent?.includes("敌方"));
    expect(container.querySelector(".battle-line")?.textContent).toBe("测试者戳刺，敌方受到15伤害");
    expect(container.querySelectorAll(".tone-damage")[0]?.textContent).toBe("戳刺");
    expect(target?.querySelector(".hp-loss")).not.toBeNull();
  });

  it("renders blue critical strike text and synchronizes its target health loss", () => {
    vi.useFakeTimers();
    const enemy = { ...player, id: 2, teamId: 2, seatId: 2, inputIndex: 2, name: "敌方" };
    const criticalBattle: CppBattleSimulationResponse = {
      ...battle,
      initialPlayers: [player, enemy],
      finalPlayers: [player, { ...enemy, hp: 70 }],
      commands: [
        { ...command("critical_strike", 0, 0, 100), targetPlayerId: 2, text: "测试者", newlineAfter: false },
        { ...command("critical_strike", 2, 0, 100), targetPlayerId: 2, text: "会心一击", newlineAfter: false },
        { ...command("critical_strike", 0, 0, 100), targetPlayerId: 2, text: "，敌方受到", newlineAfter: false },
        { ...command("critical_strike_damage", 3, 30, 70), targetPlayerId: 2, text: "30", newlineAfter: false },
        { ...command("critical_strike_damage", 0, 30, 70), targetPlayerId: 2, text: "伤害", newlineAfter: true },
      ],
    };
    const { container } = render(<BattlePlayback battle={criticalBattle} onRestart={() => {}} />);
    act(() => vi.advanceTimersByTime(0));

    const target = Array.from(container.querySelectorAll<HTMLElement>(".unit-readout")).find((unit) => unit.textContent?.includes("敌方"));
    expect(container.querySelector(".battle-line")?.textContent).toBe("测试者会心一击，敌方受到30伤害");
    expect(container.querySelector(".tone-skill")?.textContent).toBe("会心一击");
    expect(target?.querySelector(".hp-loss")).not.toBeNull();
  });

  it("renders red lifesteal attack text and synchronizes damage with green recovery", () => {
    vi.useFakeTimers();
    const enemy = { ...player, id: 2, teamId: 2, seatId: 2, inputIndex: 2, name: "敌方" };
    const lifestealBattle: CppBattleSimulationResponse = {
      ...battle,
      initialPlayers: [{ ...player, hp: 50 }, enemy],
      finalPlayers: [{ ...player, hp: 60 }, { ...enemy, hp: 90 }],
      commands: [
        { ...command("lifesteal_attack", 0, 0, 90), sourcePlayerId: 1, targetPlayerId: 2, text: "测试者", newlineAfter: false },
        { ...command("lifesteal_attack", 3, 0, 90), sourcePlayerId: 1, targetPlayerId: 2, text: "吸血攻击", newlineAfter: false },
        { ...command("lifesteal_attack", 0, 0, 90), sourcePlayerId: 1, targetPlayerId: 2, text: "，敌方受到", newlineAfter: false },
        { ...command("lifesteal_damage", 3, 10, 90), sourcePlayerId: 1, targetPlayerId: 2, text: "10", newlineAfter: false },
        { ...command("lifesteal_damage", 0, 10, 90), sourcePlayerId: 1, targetPlayerId: 2, text: "伤害，吸取", newlineAfter: false },
        { ...command("lifesteal_heal", 4, 10, 60), sourcePlayerId: 1, targetPlayerId: 1, text: "10", newlineAfter: false },
        { ...command("lifesteal_heal", 0, 10, 60), sourcePlayerId: 1, targetPlayerId: 1, text: "血量", newlineAfter: true },
      ],
    };
    const { container } = render(<BattlePlayback battle={lifestealBattle} onRestart={() => {}} />);
    act(() => vi.advanceTimersByTime(0));

    const units = Array.from(container.querySelectorAll<HTMLElement>(".unit-readout"));
    const caster = units.find((unit) => unit.textContent?.includes("测试者"));
    const target = units.find((unit) => unit.textContent?.includes("敌方"));
    expect(container.querySelector(".battle-line")?.textContent).toBe("测试者吸血攻击，敌方受到10伤害，吸取10血量");
    expect(container.querySelectorAll(".tone-damage")[0]?.textContent).toBe("吸血攻击");
    const lifestealIcon = container.querySelector(".lifesteal-icon");
    expect(lifestealIcon?.getAttribute("src")).toContain("data:image/svg+xml;base64,");
    expect(lifestealIcon?.parentElement?.textContent).toBe("吸血攻击");
    expect(container.querySelector(".tone-heal")?.textContent).toBe("10");
    expect(caster?.querySelector(".hp-heal")).not.toBeNull();
    expect(target?.querySelector(".hp-loss")).not.toBeNull();
  });

  it("renders Galahad One's red slash and restores the scientific witch by the full dealt damage", () => {
    vi.useFakeTimers();
    const witch = { ...player, id: 2, teamId: 1, seatId: 2, inputIndex: 2, name: "科学性实验魔女", hp: 2800, maxHp: 3000, physicalAttack: 0, physicalDefense: 0, magicAttack: 250, magicDefense: 100, speed: 3000, isFamiliar: true, ownerPlayerId: 1 };
    const galahad = { ...player, id: 4, teamId: 1, seatId: 4, inputIndex: 4, name: "加拉哈德1号", hp: 5000, maxHp: 5000, physicalAttack: 200, physicalDefense: 0, magicAttack: 180, magicDefense: 0, speed: 2500, isFamiliar: true, ownerPlayerId: 2 };
    const enemy = { ...player, id: 5, teamId: 2, seatId: 5, inputIndex: 5, name: "敌方", hp: 1000, maxHp: 1000 };
    const galahadSlashBattle: CppBattleSimulationResponse = {
      ...battle,
      initialPlayers: [{ ...player, id: 1, name: "mili", hp: 3000, maxHp: 3000 }, witch, galahad, enemy],
      finalPlayers: [{ ...player, id: 1, name: "mili", hp: 3000, maxHp: 3000 }, { ...witch, hp: 3000 }, galahad, { ...enemy, hp: 800 }],
      commands: [
        { ...command("galahad_slash", 0, 0, 1000), sourcePlayerId: 4, targetPlayerId: 5, text: "加拉哈德1号 ", newlineAfter: false },
        { ...command("galahad_slash", 3, 0, 1000), sourcePlayerId: 4, targetPlayerId: 5, text: "斩击", newlineAfter: false },
        { ...command("galahad_slash", 0, 0, 1000), sourcePlayerId: 4, targetPlayerId: 5, text: "敌方，敌方受到", newlineAfter: false },
        { ...command("galahad_slash_damage", 3, 200, 800), sourcePlayerId: 4, targetPlayerId: 5, text: "200", newlineAfter: false },
        { ...command("galahad_slash_damage", 0, 200, 800), sourcePlayerId: 4, targetPlayerId: 5, text: "伤害", newlineAfter: true },
        { ...command("galahad_witch_heal", 4, 200, 3000), sourcePlayerId: 4, targetPlayerId: 2, text: "科学性实验魔女恢复", newlineAfter: false },
        { ...command("galahad_witch_heal", 0, 200, 3000), sourcePlayerId: 4, targetPlayerId: 2, text: "200生命", newlineAfter: true },
      ],
    };
    const { container, unmount } = render(<BattlePlayback battle={galahadSlashBattle} onRestart={() => {}} />);
    act(() => vi.advanceTimersByTime(0));

    const renderedEnemy = Array.from(container.querySelectorAll<HTMLElement>(".unit-readout")).find((unit) => unit.textContent?.includes("敌方"));
    expect(renderedEnemy?.querySelector(".hp-loss")).not.toBeNull();
    expect(container.querySelector(".battle-line")?.textContent).toBe("加拉哈德1号 斩击敌方，敌方受到200伤害");
    expect(container.querySelector(".tone-damage")?.textContent).toBe("斩击");

    act(() => vi.advanceTimersByTime(110));
    act(() => vi.advanceTimersByTime(0));

    const units = Array.from(container.querySelectorAll<HTMLElement>(".unit-readout"));
    const renderedWitch = units.find((unit) => unit.textContent?.includes("科学性实验魔女"));
    const battleLines = Array.from(container.querySelectorAll(".battle-line")).map((line) => line.textContent);
    expect(battleLines).toContain("科学性实验魔女恢复200生命");
    expect(container.querySelector(".tone-heal")?.textContent).toContain("科学性实验魔女恢复");
    expect(renderedWitch?.querySelector(".hp-heal")).not.toBeNull();
    unmount();
  });

  it("does not heal the scientific witch after a blue Galahad laser shot", () => {
    vi.useFakeTimers();
    const witch = { ...player, id: 2, teamId: 1, seatId: 2, inputIndex: 2, name: "科学性实验魔女", hp: 700, maxHp: 1000 };
    const galahad = { ...player, id: 4, teamId: 1, seatId: 4, inputIndex: 4, name: "加拉哈德1号", hp: 1200, maxHp: 1200 };
    const enemy = { ...player, id: 5, teamId: 2, seatId: 5, inputIndex: 5, name: "敌方", hp: 1000, maxHp: 1000 };
    const laserBattle: CppBattleSimulationResponse = {
      ...battle,
      initialPlayers: [witch, galahad, enemy],
      finalPlayers: [witch, galahad, { ...enemy, hp: 700, burnStrength: 30, burnLayers: 2 }],
      commands: [
        { ...command("galahad_laser", 0, 0, 1000), sourcePlayerId: 4, targetPlayerId: 5, text: "加拉哈德1号 ", newlineAfter: false },
        { ...command("galahad_laser", 7, 0, 1000), sourcePlayerId: 4, targetPlayerId: 5, text: "激光枪射击", newlineAfter: false },
        { ...command("galahad_laser", 0, 0, 1000), sourcePlayerId: 4, targetPlayerId: 5, text: "敌方，敌方受到", newlineAfter: false },
        { ...command("galahad_laser_damage", 3, 300, 700), sourcePlayerId: 4, targetPlayerId: 5, text: "300", newlineAfter: false },
        { ...command("galahad_laser_damage", 0, 300, 700), sourcePlayerId: 4, targetPlayerId: 5, text: "伤害", newlineAfter: true },
      ],
    };
    const { container, unmount } = render(<BattlePlayback battle={laserBattle} onRestart={() => {}} />);
    act(() => vi.advanceTimersByTime(0));
    expect(container.querySelector(".tone-thunder")?.textContent).toBe("激光枪射击");
    expect(container.querySelector(".tone-heal")).toBeNull();
    expect(container.querySelector(".galahad_witch_heal")).toBeNull();
    expect(Array.from(container.querySelectorAll<HTMLElement>(".unit-readout")).find((unit) => unit.textContent?.includes("科学性实验魔女"))?.querySelector(".hp-heal")).toBeNull();
    unmount();
  });

  it("renders orange fireball and burn removal while synchronizing both damage events", () => {
    vi.useFakeTimers();
    const enemy = { ...player, id: 2, teamId: 2, seatId: 2, inputIndex: 2, name: "敌方" };
    const fireballBattle: CppBattleSimulationResponse = {
      ...battle,
      initialPlayers: [player, enemy],
      finalPlayers: [player, { ...enemy, hp: 65 }],
      commands: [
        { ...command("fireball", 0, 0, 100), sourcePlayerId: 1, targetPlayerId: 2, text: "测试者使用", newlineAfter: false },
        { ...command("fireball", 13, 0, 100), sourcePlayerId: 1, targetPlayerId: 2, text: "火球术", newlineAfter: false },
        { ...command("fireball", 0, 0, 100), sourcePlayerId: 1, targetPlayerId: 2, text: "敌方受到", newlineAfter: false },
        { ...command("fireball_damage", 3, 30, 70), sourcePlayerId: 1, targetPlayerId: 2, text: "30", newlineAfter: false },
        { ...command("fireball_damage", 0, 30, 70), sourcePlayerId: 1, targetPlayerId: 2, text: "点伤害，", newlineAfter: false },
        { ...command("burn_apply", 0, 1, 2), sourcePlayerId: 1, targetPlayerId: 2, text: "敌方被", newlineAfter: false },
        { ...command("burn_apply", 13, 1, 2), sourcePlayerId: 1, targetPlayerId: 2, text: "点燃了，获得1烧伤强度，层数为2", newlineAfter: true },
        { ...command("burn_tick", 0, 5, 65), sourcePlayerId: 2, targetPlayerId: 2, text: "敌方受到", newlineAfter: false },
        { ...command("burn_damage", 3, 5, 65), sourcePlayerId: 2, targetPlayerId: 2, text: "5", newlineAfter: false },
        { ...command("burn_tick", 13, 5, 65), sourcePlayerId: 2, targetPlayerId: 2, text: "烧伤", newlineAfter: false },
        { ...command("burn_remove", 0, 0, 65), sourcePlayerId: 2, targetPlayerId: 2, text: "伤害，从", newlineAfter: false },
        { ...command("burn_remove", 13, 0, 65), sourcePlayerId: 2, targetPlayerId: 2, text: "烧伤", newlineAfter: false },
        { ...command("burn_remove", 0, 0, 65), sourcePlayerId: 2, targetPlayerId: 2, text: "中解除", newlineAfter: true },
      ],
    };
    const { container } = render(<BattlePlayback battle={fireballBattle} onRestart={() => {}} />);
    const target = Array.from(container.querySelectorAll<HTMLElement>(".unit-readout")).find((unit) => unit.textContent?.includes("敌方"));

    expect(container.querySelector(".tone-fire")?.textContent).toBe("火球术");
    const fireIcon = container.querySelector(".fire-icon");
    expect(fireIcon?.getAttribute("src")).toContain("data:image/svg+xml;base64,");
    expect(fireIcon?.parentElement?.textContent).toBe("火球术");
    expect(container.querySelector(".battle-line")?.textContent).toBe("测试者使用火球术敌方受到30点伤害，敌方被点燃了，获得1烧伤强度，层数为2");
    expect(target?.querySelector(".hp-loss")).not.toBeNull();

    act(() => vi.advanceTimersByTime(110));
    act(() => vi.advanceTimersByTime(0));
    const battleLines = Array.from(container.querySelectorAll(".battle-line"));
    const fireTones = Array.from(container.querySelectorAll(".tone-fire"));
    expect(battleLines[battleLines.length - 1]?.textContent).toBe("敌方受到5烧伤伤害，从烧伤中解除");
    expect(fireTones[fireTones.length - 1]?.textContent).toBe("烧伤");
    expect(target?.querySelector(".hp-loss")).not.toBeNull();
  });

  it("renders deep-green poison, stacks strength and layers, ticks per layer, and clears all poison on detox", () => {
    vi.useFakeTimers();
    const enemy = { ...player, id: 2, teamId: 2, seatId: 2, inputIndex: 2, name: "敌方" };
    const poisonBattle: CppBattleSimulationResponse = {
      ...battle,
      initialPlayers: [player, enemy],
      finalPlayers: [player, { ...enemy, hp: 20, poisonStrength: 0, poisonLayers: 0 }],
      commands: [
        { ...command("poison", 0, 0, 100), sourcePlayerId: 1, targetPlayerId: 2, text: "测试者使用", newlineAfter: false, poisonStrength: 20, poisonLayers: 1 },
        { ...command("poison", 14, 0, 100), sourcePlayerId: 1, targetPlayerId: 2, text: "投毒", newlineAfter: false, poisonStrength: 20, poisonLayers: 1 },
        { ...command("poison_apply", 0, 20, 1), sourcePlayerId: 1, targetPlayerId: 2, text: "，敌方获得", newlineAfter: false, poisonStrength: 20, poisonLayers: 1 },
        { ...command("poison_apply", 14, 20, 1), sourcePlayerId: 1, targetPlayerId: 2, text: "20中毒", newlineAfter: false, poisonStrength: 20, poisonLayers: 1 },
        { ...command("poison_apply", 0, 20, 1), sourcePlayerId: 1, targetPlayerId: 2, text: "强度，层数为", newlineAfter: false, poisonStrength: 20, poisonLayers: 1 },
        { ...command("poison_apply", 14, 20, 1), sourcePlayerId: 1, targetPlayerId: 2, text: "1", newlineAfter: true, poisonStrength: 20, poisonLayers: 1 },
        { ...command("poison", 0, 0, 100), sourcePlayerId: 1, targetPlayerId: 2, text: "测试者使用", newlineAfter: false, poisonStrength: 40, poisonLayers: 2 },
        { ...command("poison", 14, 0, 100), sourcePlayerId: 1, targetPlayerId: 2, text: "投毒", newlineAfter: false, poisonStrength: 40, poisonLayers: 2 },
        { ...command("poison_apply", 0, 40, 2), sourcePlayerId: 1, targetPlayerId: 2, text: "，敌方获得", newlineAfter: false, poisonStrength: 40, poisonLayers: 2 },
        { ...command("poison_apply", 14, 40, 2), sourcePlayerId: 1, targetPlayerId: 2, text: "40中毒", newlineAfter: false, poisonStrength: 40, poisonLayers: 2 },
        { ...command("poison_apply", 0, 40, 2), sourcePlayerId: 1, targetPlayerId: 2, text: "强度，层数为", newlineAfter: false, poisonStrength: 40, poisonLayers: 2 },
        { ...command("poison_apply", 14, 40, 2), sourcePlayerId: 1, targetPlayerId: 2, text: "2", newlineAfter: true, poisonStrength: 40, poisonLayers: 2 },
        { ...command("poison_tick", 0, 0, 60), sourcePlayerId: 2, targetPlayerId: 2, text: "敌方受到", newlineAfter: false, poisonStrength: 40, poisonLayers: 2 },
        { ...command("poison_damage", 3, 40, 60), sourcePlayerId: 2, targetPlayerId: 2, text: "40", newlineAfter: false, poisonStrength: 40, poisonLayers: 2 },
        { ...command("poison_tick", 0, 40, 60), sourcePlayerId: 2, targetPlayerId: 2, text: "点", newlineAfter: false, poisonStrength: 40, poisonLayers: 2 },
        { ...command("poison_tick", 14, 40, 60), sourcePlayerId: 2, targetPlayerId: 2, text: "毒素伤害", newlineAfter: true, poisonStrength: 40, poisonLayers: 2 },
        { ...command("poison_tick", 0, 0, 20), sourcePlayerId: 2, targetPlayerId: 2, text: "敌方受到", newlineAfter: false, poisonStrength: 40, poisonLayers: 2 },
        { ...command("poison_damage", 3, 40, 20), sourcePlayerId: 2, targetPlayerId: 2, text: "40", newlineAfter: false, poisonStrength: 40, poisonLayers: 2 },
        { ...command("poison_tick", 0, 40, 20), sourcePlayerId: 2, targetPlayerId: 2, text: "点", newlineAfter: false, poisonStrength: 40, poisonLayers: 2 },
        { ...command("poison_tick", 14, 40, 20), sourcePlayerId: 2, targetPlayerId: 2, text: "毒素伤害", newlineAfter: true, poisonStrength: 40, poisonLayers: 2 },
        { ...command("poison_remove", 14, 0, 0), sourcePlayerId: 2, targetPlayerId: 2, text: "解毒成功", newlineAfter: true, poisonStrength: 0, poisonLayers: 0 },
      ],
    };
    const { container } = render(<BattlePlayback battle={poisonBattle} onRestart={() => {}} />);
    act(() => vi.advanceTimersByTime(0));

    const enemyUnit = Array.from(container.querySelectorAll<HTMLElement>(".unit-readout")).find((unit) => unit.textContent?.includes("敌方"));
    expect(container.querySelector(".tone-poison")?.textContent).toBe("投毒");
    expect(container.querySelector(".poison-icon")?.getAttribute("src")).toContain("data:image/svg+xml;base64,");
    expect(enemyUnit?.querySelector(".unit-status-poison b")?.textContent).toBe("20/1");
    expect(enemyUnit?.classList.contains("is-poisoned")).toBe(true);

    act(() => vi.advanceTimersByTime(110));
    expect(container.querySelectorAll(".battle-line")[1]?.textContent).toBe("测试者使用投毒，敌方获得40中毒强度，层数为2");
    expect(enemyUnit?.querySelector(".unit-status-poison b")?.textContent).toBe("40/2");

    act(() => vi.advanceTimersByTime(110));
    expect(container.querySelectorAll(".battle-line")[2]?.textContent).toBe("敌方受到40点毒素伤害");
    expect(Array.from(container.querySelectorAll(".battle-line")[2]?.querySelectorAll(".tone-poison") ?? []).map((node) => node.textContent)).toEqual(["毒素伤害"]);
    expect(enemyUnit?.querySelector(".hp-loss")).not.toBeNull();

    act(() => vi.advanceTimersByTime(110));
    expect(container.querySelectorAll(".battle-line")[3]?.textContent).toBe("敌方受到40点毒素伤害");

    act(() => vi.advanceTimersByTime(110));
    expect(container.querySelectorAll(".battle-line")[4]?.textContent).toBe("解毒成功");
    expect(enemyUnit?.querySelector(".unit-status-poison")).toBeNull();
    expect(enemyUnit?.classList.contains("is-poisoned")).toBe(false);
  });

  it("plays unique earthquake targets as consecutive synchronized events", () => {
    vi.useFakeTimers();
    const enemyOne = { ...player, id: 2, teamId: 2, seatId: 2, inputIndex: 2, name: "敌方甲" };
    const enemyTwo = { ...player, id: 3, teamId: 2, seatId: 3, inputIndex: 3, name: "敌方乙" };
    const earthquakeBattle: CppBattleSimulationResponse = {
      ...battle,
      initialPlayers: [player, enemyOne, enemyTwo],
      finalPlayers: [player, { ...enemyOne, hp: 84 }, { ...enemyTwo, hp: 88 }],
      commands: [
        { ...command("earthquake", 11, 0, 100), text: "地裂术", newlineAfter: true },
        { ...command("earthquake_damage", 3, 16, 84), targetPlayerId: 2, text: "16", newlineAfter: true },
        { ...command("earthquake_damage", 3, 12, 88), targetPlayerId: 3, text: "12", newlineAfter: true },
      ],
    };
    const { container } = render(<BattlePlayback battle={earthquakeBattle} onRestart={() => {}} />);

    act(() => vi.advanceTimersByTime(110));
    act(() => vi.advanceTimersByTime(0));
    const units = Array.from(container.querySelectorAll<HTMLElement>(".unit-readout"));
    const firstTarget = units.find((unit) => unit.textContent?.includes("敌方甲"));
    const secondTarget = units.find((unit) => unit.textContent?.includes("敌方乙"));
    expect(firstTarget?.querySelector(".hp-loss")).not.toBeNull();
    expect(secondTarget?.querySelector(".hp-loss")).toBeNull();

    act(() => vi.advanceTimersByTime(92));
    act(() => vi.advanceTimersByTime(0));
    expect(firstTarget?.querySelector(".hp-loss")).toBeNull();
    expect(secondTarget?.querySelector(".hp-loss")).not.toBeNull();
    expect(container.querySelectorAll(".battle-line")).toHaveLength(3);
  });

  it("renders the gray parry swords and clears the two-turn parry state when counterattack triggers", () => {
    vi.useFakeTimers();
    const defender = { ...player, id: 2, teamId: 2, seatId: 2, inputIndex: 2, name: "招架者" };
    const parryBattle: CppBattleSimulationResponse = {
      ...battle,
      initialPlayers: [player, defender],
      finalPlayers: [{ ...player, hp: 65 }, { ...defender, parryLayers: 0 }],
      commands: [
        { ...command("parry", 0, 2, 100), sourcePlayerId: 2, targetPlayerId: 2, text: "招架者开始", newlineAfter: false, parryLayers: 2 },
        { ...command("parry", 10, 2, 100), sourcePlayerId: 2, targetPlayerId: 2, text: "招架", newlineAfter: true, parryLayers: 2 },
        { ...command("parry_counter_start", 0, 0, 100), sourcePlayerId: 1, targetPlayerId: 2, text: "招架者", newlineAfter: false, parryLayers: 0 },
        { ...command("parry_counter_start", 0, 0, 100), sourcePlayerId: 1, targetPlayerId: 2, text: "触发", newlineAfter: false, parryLayers: 0 },
        { ...command("parry_counter_start", 10, 0, 100), sourcePlayerId: 1, targetPlayerId: 2, text: "招架反击", newlineAfter: false, parryLayers: 0 },
        { ...command("parry_counter_damage", 0, 0, 100), sourcePlayerId: 2, targetPlayerId: 1, text: "，测试者受到", newlineAfter: false, parryLayers: 0 },
        { ...command("parry_counter_damage", 3, 35, 65), sourcePlayerId: 2, targetPlayerId: 1, text: "35", newlineAfter: false, parryLayers: 0 },
        { ...command("parry_counter_damage", 0, 35, 65), sourcePlayerId: 2, targetPlayerId: 1, text: "点伤害", newlineAfter: true, parryLayers: 0 },
      ],
    };
    const { container } = render(<BattlePlayback battle={parryBattle} onRestart={() => {}} />);
    act(() => vi.advanceTimersByTime(0));

    const defenderUnit = Array.from(container.querySelectorAll<HTMLElement>(".unit-readout")).find((unit) => unit.textContent?.includes("招架者"));
    expect(defenderUnit?.querySelector(".unit-status-parry b")?.textContent).toBe("1/2");
    expect(defenderUnit?.querySelector(".unit-status-parry img")?.getAttribute("src")).toContain("data:image/svg+xml;base64,");
    expect(container.querySelector(".tone-ironwall")?.textContent).toBe("招架");

    act(() => vi.advanceTimersByTime(110));
    act(() => vi.advanceTimersByTime(0));
    expect(Array.from(container.querySelectorAll(".battle-line")).at(-1)?.textContent).toBe("招架者触发招架反击，测试者受到35点伤害");
    expect(defenderUnit?.querySelector(".unit-status-parry")).toBeNull();
    expect(container.querySelector(".unit-readout .hp-loss")).not.toBeNull();
    expect(container.textContent).not.toContain("中解除");
  });

  it("renders a normal trigger, static gray counterattack, and immediate red counter damage flash on the same line", () => {
    vi.useFakeTimers();
    const parryImpactBattle: CppBattleSimulationResponse = {
      ...battle,
      commands: [
        { ...command("parry_counter_start", 0, 0, 100), text: "招架者", newlineAfter: false },
        { ...command("parry_counter_start", 0, 0, 100), text: "触发", newlineAfter: false },
        { ...command("parry_counter_start", 10, 0, 100), text: "招架反击", newlineAfter: false },
        { ...command("parry_counter_damage", 0, 0, 100), text: "，测试者受到", newlineAfter: false },
        { ...command("parry_counter_damage", 3, 35, 65), text: "35", newlineAfter: false },
        { ...command("parry_counter_damage", 0, 35, 65), text: "点伤害", newlineAfter: true },
        { ...command("normal_attack", 0, 0, 65), text: "下一条", newlineAfter: true },
      ],
    };
    const { container } = render(<BattlePlayback battle={parryImpactBattle} onRestart={() => {}} />);

    expect(container.querySelector(".battle-line")?.textContent).toBe("招架者触发招架反击，测试者受到35点伤害");
    expect(container.querySelector(".battle-line.is-parry-flash")).toBeNull();
    expect(container.querySelectorAll(".battle-line")).toHaveLength(1);
    const impactNumber = container.querySelector(".parry-counter-number");
    expect(impactNumber?.textContent).toBe("35");
    expect(impactNumber?.classList.contains("tone-damage")).toBe(true);
    expect(container.querySelector(".parry-counter-flash-ring")).not.toBeNull();
    expect(Array.from(container.querySelectorAll<HTMLElement>("span")).find((span) => span.textContent === "触发")?.classList.contains("tone-normal")).toBe(true);
    expect(Array.from(container.querySelectorAll<HTMLElement>("span")).find((span) => span.textContent === "招架反击")?.classList.contains("tone-ironwall")).toBe(true);
    const impactCss = readFileSync(resolve(process.cwd(), "client/src/index.css"), "utf8");
    expect(impactCss).toContain("font-size: 1.32em");
    expect(impactCss).toContain("animation: parry-counter-flash-ring 260ms");
    expect(impactCss).not.toContain("parry-counter-text-impact");

    act(() => vi.advanceTimersByTime(64));
    act(() => vi.advanceTimersByTime(0));
    expect(Array.from(container.querySelectorAll(".battle-line")).at(-1)?.textContent).toBe("下一条");
  });

  it("advances thunder lead-in blank lines faster before the first hit", () => {
    vi.useFakeTimers();
    const blankLine = { ...command("", 0, 0, 0), text: "" };
    const thunderLeadInBattle: CppBattleSimulationResponse = {
      ...battle,
      commands: [blankLine, blankLine, blankLine, command("thunder_damage", 3, 10, 90)],
    };
    const { container } = render(<BattlePlayback battle={thunderLeadInBattle} onRestart={() => {}} />);

    act(() => vi.advanceTimersByTime(24));
    expect(container.querySelectorAll(".hp-loss")).toHaveLength(0);

    act(() => vi.advanceTimersByTime(24));
    expect(container.querySelectorAll(".hp-loss")).toHaveLength(0);

    act(() => vi.advanceTimersByTime(24));
    act(() => vi.advanceTimersByTime(0));
    expect(container.querySelectorAll(".hp-loss")).toHaveLength(1);
  });

  it("keeps early thunder hits fast but pauses before the final strike", () => {
    vi.useFakeTimers();
    const thunderBattle: CppBattleSimulationResponse = {
      ...battle,
      commands: [
        command("thunder_damage", 3, 10, 90),
        command("thunder_damage_last", 3, 10, 80),
      ],
    };
    const { container } = render(<BattlePlayback battle={thunderBattle} onRestart={() => {}} />);

    act(() => vi.advanceTimersByTime(0));
    expect(container.querySelectorAll(".hp-loss")).toHaveLength(1);

    act(() => vi.advanceTimersByTime(109));
    expect(container.querySelectorAll(".hp-loss")).toHaveLength(1);
    expect(container.querySelectorAll(".battle-line")).toHaveLength(1);
    expect(container.querySelector(".hp-values")?.textContent).toBe("HP90/100");

    act(() => vi.advanceTimersByTime(1));
    act(() => vi.advanceTimersByTime(0));
    expect(container.querySelectorAll(".hp-loss")).toHaveLength(1);
    expect(container.querySelectorAll(".battle-line")).toHaveLength(2);
    expect(container.querySelector(".hp-values")?.textContent).toBe("HP80/100");
  });

  it("slows ordinary event playback when long battle is disabled", () => {
    vi.useFakeTimers();
    const shortBattle: CppBattleSimulationResponse = {
      ...battle,
      longBattle: false,
      commands: [command("normal_attack_damage", 3, 10, 90), command("normal_attack_damage", 3, 10, 80)],
    };
    const { container } = render(<BattlePlayback battle={shortBattle} onRestart={() => {}} />);

    act(() => vi.advanceTimersByTime(95));
    expect(container.querySelectorAll(".battle-line")).toHaveLength(1);

    act(() => vi.advanceTimersByTime(1));
    expect(container.querySelectorAll(".battle-line")).toHaveLength(2);
  });

  it("renders heal casting and the light-green restored amount on one line", () => {
    vi.useFakeTimers();
    const healCastBattle: CppBattleSimulationResponse = {
      ...battle,
      commands: [
        { ...command("heal_magic", 0, 0, 90), text: "测试者使用", newlineAfter: false },
        { ...command("heal_magic", 4, 0, 90), text: "治愈魔法", newlineAfter: false },
        { ...command("heal_magic", 0, 0, 90), text: "，恢复", newlineAfter: false },
        { ...command("heal", 4, 10, 90), text: "10", newlineAfter: false },
        { ...command("heal", 0, 10, 90), text: "生命", newlineAfter: true },
      ],
    };
    const { container } = render(<BattlePlayback battle={healCastBattle} onRestart={() => {}} />);
    act(() => vi.advanceTimersByTime(0));

    expect(container.querySelector(".battle-line")?.textContent).toBe("测试者使用治愈魔法，恢复10生命");
    expect(Array.from(container.querySelectorAll(".tone-heal")).map((node) => node.textContent)).toEqual(["治愈魔法", "10"]);
    expect(container.querySelectorAll(".hp-heal")).toHaveLength(1);
  });

  it("renders a friendly heal log and light-green overlay on the actual teammate target", () => {
    vi.useFakeTimers();
    const teammate = { ...player, id: 2, seatId: 2, inputIndex: 2, name: "队友", hp: 50 };
    const friendlyHealBattle: CppBattleSimulationResponse = {
      ...battle,
      initialPlayers: [player, teammate],
      finalPlayers: [player, { ...teammate, hp: 60 }],
      commands: [
        { ...command("heal_magic", 0, 0, 60), sourcePlayerId: 1, targetPlayerId: 2, text: "测试者使用", newlineAfter: false },
        { ...command("heal_magic", 2, 0, 60), sourcePlayerId: 1, targetPlayerId: 2, text: "治愈魔法，为队友恢复", newlineAfter: false },
        { ...command("heal", 4, 10, 60), sourcePlayerId: 1, targetPlayerId: 2, text: "10", newlineAfter: false },
        { ...command("heal", 0, 10, 60), sourcePlayerId: 1, targetPlayerId: 2, text: "生命", newlineAfter: true },
      ],
    };
    const { container } = render(<BattlePlayback battle={friendlyHealBattle} onRestart={() => {}} />);
    act(() => vi.advanceTimersByTime(0));

    const units = Array.from(container.querySelectorAll<HTMLElement>(".unit-readout"));
    const casterUnit = units.find((unit) => unit.textContent?.includes("测试者"));
    const teammateUnit = units.find((unit) => unit.textContent?.includes("队友"));
    expect(container.querySelector(".battle-line")?.textContent).toBe("测试者使用治愈魔法，为队友恢复10生命");
    expect(casterUnit?.querySelector(".hp-heal")).toBeNull();
    expect(teammateUnit?.querySelector(".hp-heal")).not.toBeNull();
  });

  it("renders purify on a teammate, clears every negative status including stacked poison, and animates the recovery", () => {
    vi.useFakeTimers();
    const teammate = {
      ...player, id: 2, seatId: 2, inputIndex: 2, name: "被净化队友", hp: 50,
      freezeStrength: 2, freezeLayers: 6, speedDownStrength: 10, speedDownLayers: 4, burnStrength: 30, burnLayers: 4, poisonStrength: 40, poisonLayers: 2,
    };
    const purifyBattle: CppBattleSimulationResponse = {
      ...battle,
      initialPlayers: [player, teammate], finalPlayers: [player, { ...teammate, hp: 70, freezeStrength: 0, freezeLayers: 0, speedDownStrength: 0, speedDownLayers: 0, burnStrength: 0, burnLayers: 0, poisonStrength: 0, poisonLayers: 0 }],
      commands: [
        { ...command("purify", 0, 0, 70), sourcePlayerId: 1, targetPlayerId: 2, text: "测试者使用", newlineAfter: false, freezeStrength: 0, freezeLayers: 0, speedDownStrength: 0, speedDownLayers: 0, burnStrength: 0, burnLayers: 0, poisonStrength: 0, poisonLayers: 0 },
        { ...command("purify", 4, 0, 70), sourcePlayerId: 1, targetPlayerId: 2, text: "净化", newlineAfter: false, freezeStrength: 0, freezeLayers: 0, speedDownStrength: 0, speedDownLayers: 0, burnStrength: 0, burnLayers: 0, poisonStrength: 0, poisonLayers: 0 },
        { ...command("purify", 0, 0, 70), sourcePlayerId: 1, targetPlayerId: 2, text: "，为被净化队友清除所有负面状态，恢复", newlineAfter: false, freezeStrength: 0, freezeLayers: 0, speedDownStrength: 0, speedDownLayers: 0, burnStrength: 0, burnLayers: 0, poisonStrength: 0, poisonLayers: 0 },
        { ...command("status_heal", 4, 20, 70), sourcePlayerId: 1, targetPlayerId: 2, text: "20", newlineAfter: false, freezeStrength: 0, freezeLayers: 0, speedDownStrength: 0, speedDownLayers: 0, burnStrength: 0, burnLayers: 0, poisonStrength: 0, poisonLayers: 0 },
        { ...command("status_heal", 0, 20, 70), sourcePlayerId: 1, targetPlayerId: 2, text: "生命", newlineAfter: true, freezeStrength: 0, freezeLayers: 0, speedDownStrength: 0, speedDownLayers: 0, burnStrength: 0, burnLayers: 0, poisonStrength: 0, poisonLayers: 0 },
      ],
    };
    const { container } = render(<BattlePlayback battle={purifyBattle} onRestart={() => {}} />);
    act(() => vi.advanceTimersByTime(0));

    const teammateUnit = Array.from(container.querySelectorAll<HTMLElement>(".unit-readout")).find((unit) => unit.textContent?.includes("被净化队友"));
    expect(container.querySelector(".battle-line")?.textContent).toBe("测试者使用净化，为被净化队友清除所有负面状态，恢复20生命");
    expect(Array.from(container.querySelectorAll(".tone-heal")).map((node) => node.textContent)).toEqual(["净化", "20"]);
    expect(teammateUnit?.querySelectorAll(".unit-status")).toHaveLength(0);
    expect(teammateUnit?.querySelector(".hp-heal")).not.toBeNull();
  });

  it("smoothly scrolls the log to the bottom as soon as its content overflows", () => {
    vi.useFakeTimers();
    const { container } = render(<BattlePlayback battle={battle} onRestart={() => {}} />);
    const stream = container.querySelector<HTMLElement>(".playback-stream");
    const scrollTo = vi.fn();
    expect(stream).not.toBeNull();
    Object.defineProperties(stream!, {
      clientHeight: { configurable: true, value: 100 },
      scrollHeight: { configurable: true, value: 240 },
      scrollTop: { configurable: true, writable: true, value: 0 },
      scrollTo: { configurable: true, value: scrollTo },
    });

    act(() => vi.advanceTimersByTime(110));
    expect(scrollTo).toHaveBeenCalledWith({ top: 140, behavior: "smooth" });
  });

  it("shows only the three highest-layer statuses after a name and omits blood intensity", () => {
    vi.useFakeTimers();
    const statusBattle: CppBattleSimulationResponse = {
      ...battle,
      commands: [{
        ...command("status_snapshot", 0, 0, 100),
        freezeStrength: 1, freezeLayers: 2, ironwallStrength: 100, ironwallLayers: 6, speedUpStrength: 10, speedUpLayers: 4,
        speedDownStrength: 5, speedDownLayers: 3, burnStrength: 9, burnLayers: 5,
      }],
    };
    const { container } = render(<BattlePlayback battle={statusBattle} onRestart={() => {}} />);
    act(() => vi.advanceTimersByTime(0));

    const statuses = container.querySelectorAll(".unit-status");
    expect(statuses).toHaveLength(3);
    expect(container.querySelector(".unit-name")?.textContent).toBe("测试者");
    expect(container.querySelector(".unit-readout strong em")?.textContent).toBe("#队伍1");
    expect(container.querySelector(".unit-status-ironwall")).not.toBeNull();
    expect(container.querySelector(".unit-status-burn")).not.toBeNull();
    expect(container.querySelector(".unit-status-speed-up")).not.toBeNull();
    expect(container.querySelector(".unit-status-speed-down")).toBeNull();
    expect(container.querySelector(".unit-status-freeze")).toBeNull();
    expect(container.querySelector(".unit-status-speed-up img")?.getAttribute("src")).toContain("data:image/svg+xml;base64,");
    expect(container.querySelector(".unit-status-ironwall b")?.textContent).toBe("100/6");
    expect(container.querySelector(".unit-status-burn b")?.textContent).toBe("9/5");
    expect(container.querySelector(".unit-status-speed-up b")?.textContent).toBe("10/4");
    expect(container.textContent).not.toContain("嗜血");
  });

  it("updates the status number when a later C++ status snapshot decrements its layer", () => {
    vi.useFakeTimers();
    const statusProgressBattle: CppBattleSimulationResponse = {
      ...battle,
      commands: [
        { ...command("status_sync", 0, 0, 100), ironwallStrength: 100, ironwallLayers: 4 },
        { ...command("status_sync", 0, 0, 100), ironwallStrength: 100, ironwallLayers: 3 },
        { ...command("status_sync", 0, 0, 100), ironwallStrength: 0, ironwallLayers: 0 },
      ],
    };
    const { container } = render(<BattlePlayback battle={statusProgressBattle} onRestart={() => {}} />);
    act(() => vi.advanceTimersByTime(0));
    expect(container.querySelector(".unit-status-ironwall b")?.textContent).toBe("100/4");

    act(() => vi.advanceTimersByTime(110));
    expect(container.querySelector(".unit-status-ironwall b")?.textContent).toBe("100/3");

    act(() => vi.advanceTimersByTime(110));
    expect(container.querySelector(".unit-status-ironwall")).toBeNull();
  });

  it("keeps a long name, three status icons, and the team marker in the compact status row", () => {
    vi.useFakeTimers();
    const longName = "这是一个在窄屏中需要省略的非常非常长的角色名字";
    const compactBattle: CppBattleSimulationResponse = {
      ...battle,
      initialPlayers: [{ ...player, name: longName }],
      finalPlayers: [{ ...player, name: longName }],
      commands: [{
        ...command("status_snapshot", 0, 0, 100),
        freezeLayers: 5, ironwallLayers: 4, speedUpStrength: 10, speedUpLayers: 3,
      }],
    };
    const { container } = render(<BattlePlayback battle={compactBattle} onRestart={() => {}} />);
    act(() => vi.advanceTimersByTime(0));

    expect(container.querySelector(".unit-name")?.textContent).toBe(longName);
    expect(container.querySelectorAll(".unit-status")).toHaveLength(3);
    expect(container.querySelector(".unit-readout strong em")?.textContent).toBe("#队伍1");
    const statusCss = readFileSync(resolve(process.cwd(), "client/src/index.css"), "utf8");
    expect(statusCss).toContain(".unit-readout strong { display: flex; min-width: 0;");
    expect(statusCss).toContain(".unit-readout .unit-name { min-width: 0; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }");
    expect(statusCss).toContain(".unit-statuses { display: inline-flex; flex: 0 1 auto;");
  });

  it("renders mili's visible-name silver milk heal as one line", () => {
    vi.useFakeTimers();
    const mili = { ...player, name: "mili", hp: 2100, maxHp: 3000, physicalAttack: 200, physicalDefense: 45, magicAttack: 200, magicDefense: 50, speed: 2000 };
    const milkBattle: CppBattleSimulationResponse = {
      ...battle,
      initialPlayers: [mili],
      finalPlayers: [{ ...mili, hp: 3000 }],
      commands: [
        { ...command("mili_milk", 0, 0, 3000), skillId: 27, text: "mili饮用", newlineAfter: false },
        { ...command("mili_milk", 10, 0, 3000), skillId: 27, text: "双岛牛奶!! mili恢复", newlineAfter: false },
        { ...command("heal", 4, 900, 3000), skillId: 27, text: "30%血量", newlineAfter: true },
      ],
    };
    const { container } = render(<BattlePlayback battle={milkBattle} onRestart={() => {}} />);
    act(() => vi.advanceTimersByTime(0));
    expect(container.querySelector(".unit-name")?.textContent).toBe("mili");
    expect(container.querySelector(".tone-ironwall")?.textContent).toBe("双岛牛奶!! mili恢复");
    expect(container.querySelector(".tone-heal")?.textContent).toBe("30%血量");
    expect(container.querySelector(".battle-line")?.textContent).toBe("mili饮用双岛牛奶!! mili恢复30%血量");
    act(() => vi.runOnlyPendingTimers());
  });

  it("allows multiple cyan summons below one owner, then removes every familiar when the owner dies", () => {
    vi.useFakeTimers();
    const owner = { ...player, id: 1, teamId: 1, seatId: 1, inputIndex: 1, name: "召唤者", hp: 100 };
    const enemy = { ...player, id: 2, teamId: 2, seatId: 2, inputIndex: 2, name: "敌人", hp: 100 };
    const familiar = {
      ...player, id: 3, teamId: 1, seatId: 3, inputIndex: 3, name: "幻魔", hp: 0, maxHp: 150, mana: 0, maxMana: 0,
      physicalAttack: 80, physicalDefense: 20, magicAttack: 0, magicDefense: 20, wisdom: 0, speed: 1500,
      isFamiliar: true, ownerPlayerId: 1, magicVulnerability: 20, alive: false,
    };
    const secondFamiliar = { ...familiar, id: 4, seatId: 4, inputIndex: 4 };
    const summonBattle: CppBattleSimulationResponse = {
      ...battle,
      initialPlayers: [owner, enemy],
      finalPlayers: [{ ...owner, hp: 0, alive: false }, enemy, familiar, secondFamiliar],
      commands: [
        { ...command("mana_cost", 0, 40, 160), sourcePlayerId: 1, targetPlayerId: 1, text: "", newlineAfter: false },
        { ...command("summon", 0, 0, 100), sourcePlayerId: 1, targetPlayerId: 3, text: "召唤者 ", newlineAfter: false },
        { ...command("summon", 17, 0, 100), sourcePlayerId: 1, targetPlayerId: 3, text: "召唤", newlineAfter: false },
        { ...command("summon", 0, 0, 100), sourcePlayerId: 1, targetPlayerId: 3, text: " 出了 ", newlineAfter: false },
        { ...command("summon", 16, 0, 100), sourcePlayerId: 1, targetPlayerId: 3, text: "幻魔", newlineAfter: false },
        { ...command("summon_spawn", 0, 0, 150), sourcePlayerId: 1, targetPlayerId: 3, text: "", newlineAfter: true },
        { ...command("summon", 0, 0, 100), sourcePlayerId: 1, targetPlayerId: 4, text: "召唤者 ", newlineAfter: false },
        { ...command("summon", 17, 0, 100), sourcePlayerId: 1, targetPlayerId: 4, text: "召唤", newlineAfter: false },
        { ...command("summon", 0, 0, 100), sourcePlayerId: 1, targetPlayerId: 4, text: " 出了 ", newlineAfter: false },
        { ...command("summon", 16, 0, 100), sourcePlayerId: 1, targetPlayerId: 4, text: "幻魔", newlineAfter: false },
        { ...command("summon_spawn", 0, 0, 150), sourcePlayerId: 1, targetPlayerId: 4, text: "", newlineAfter: true },
        { ...command("normal_attack", 0, 0, 100), sourcePlayerId: 3, targetPlayerId: 2, text: "幻魔发起攻击", newlineAfter: true },
        { ...command("death", 1, 0, 0), sourcePlayerId: 2, targetPlayerId: 1, text: "召唤者消失了", newlineAfter: true },
        { ...command("familiar_depart", 1, 0, 0), sourcePlayerId: 1, targetPlayerId: 3, text: "幻魔随本体消失了", newlineAfter: true },
        { ...command("familiar_depart", 1, 0, 0), sourcePlayerId: 1, targetPlayerId: 4, text: "幻魔随本体消失了", newlineAfter: true },
      ],
    };
    const { container } = render(<BattlePlayback battle={summonBattle} onRestart={() => {}} />);
    act(() => vi.advanceTimersByTime(0));

    const ownerSlot = container.querySelector<HTMLElement>(".unit-owner-slot.has-reserved-familiar");
    const ownerSlotUnits = ownerSlot?.querySelectorAll<HTMLElement>(".unit-readout");
    expect(container.querySelector(".tone-summon")?.textContent).toBe("召唤");
    expect(container.querySelector(".tone-life-wheel")?.textContent).toBe("幻魔");
    expect(container.querySelector(".battle-line")?.textContent).toBe("召唤者 召唤 出了 幻魔");
    expect(ownerSlotUnits).toHaveLength(2);
    expect(ownerSlotUnits?.[0]?.textContent).toContain("召唤者");
    expect(ownerSlotUnits?.[1]?.classList.contains("is-familiar")).toBe(true);
    expect(ownerSlotUnits?.[1]?.querySelector(".hp-values")?.textContent).toContain("150/150");
    expect(ownerSlotUnits?.[1]?.querySelector(".unit-status-magic-vuln b")?.textContent).toBe("20/∞");

    act(() => vi.advanceTimersByTime(150));
    const afterSecondSummonUnits = ownerSlot?.querySelectorAll<HTMLElement>(".unit-readout");
    expect(afterSecondSummonUnits).toHaveLength(3);
    expect(Array.from(afterSecondSummonUnits ?? []).filter((unit) => unit.classList.contains("is-familiar"))).toHaveLength(2);

    act(() => vi.advanceTimersByTime(64));
    expect(Array.from(container.querySelectorAll(".battle-line")).at(-1)?.textContent).toBe("幻魔发起攻击");

    act(() => vi.advanceTimersByTime(64));
    const afterOwnerDeathUnits = ownerSlot?.querySelectorAll<HTMLElement>(".unit-readout");
    expect(afterOwnerDeathUnits?.[0]?.classList.contains("is-defeated")).toBe(true);
    expect(afterOwnerDeathUnits?.[1]?.classList.contains("is-defeated")).toBe(false);
    expect(afterOwnerDeathUnits?.[2]?.classList.contains("is-defeated")).toBe(false);

    act(() => vi.advanceTimersByTime(64));
    const afterFamiliarDeathUnits = ownerSlot?.querySelectorAll<HTMLElement>(".unit-readout");
    expect(afterFamiliarDeathUnits?.[1]?.classList.contains("is-defeated")).toBe(true);
    expect(afterFamiliarDeathUnits?.[2]?.classList.contains("is-defeated")).toBe(false);

    for (let index = 0; index < 3; index += 1) {
      act(() => vi.advanceTimersByTime(110));
      act(() => vi.advanceTimersByTime(0));
    }
    const afterAllFamiliarDeaths = ownerSlot?.querySelectorAll<HTMLElement>(".unit-readout");
    expect(afterAllFamiliarDeaths?.[2]?.classList.contains("is-defeated")).toBe(true);
    act(() => vi.runOnlyPendingTimers());
  });

  it("summons a moon child that chants for the mili team and chants once more on death", () => {
    vi.useFakeTimers();
    const mili = { ...player, id: 1, teamId: 1, seatId: 1, inputIndex: 1, name: "mili", hp: 2000, maxHp: 3000 };
    const ally = { ...player, id: 2, teamId: 1, seatId: 2, inputIndex: 2, name: "同阵营", hp: 500, maxHp: 1000 };
    const moonChild = {
      ...player, id: 3, teamId: 1, seatId: 3, inputIndex: 3, name: "月之子", hp: 0, maxHp: 50,
      physicalAttack: 100, physicalDefense: -50, magicAttack: 0, magicDefense: 0, wisdom: 0, speed: 2500,
      isFamiliar: true, ownerPlayerId: 1, alive: false,
    };
    const enemy = { ...player, id: 4, teamId: 2, seatId: 4, inputIndex: 4, name: "敌人", hp: 100 };
    const moonBattle: CppBattleSimulationResponse = {
      ...battle,
      initialPlayers: [mili, ally, enemy],
      finalPlayers: [{ ...mili, hp: 2120 }, { ...ally, hp: 540 }, moonChild, enemy],
      commands: [
        { ...command("moon_summon", 0, 0, 50), sourcePlayerId: 1, targetPlayerId: 3, text: "mili召唤出了", newlineAfter: false },
        { ...command("moon_summon", 16, 0, 50), sourcePlayerId: 1, targetPlayerId: 3, text: "月之子", newlineAfter: true },
        { ...command("moon_chant", 0, 0, 2060), sourcePlayerId: 3, targetPlayerId: 1, text: "月之子", newlineAfter: false },
        { ...command("moon_chant", 16, 0, 2060), sourcePlayerId: 3, targetPlayerId: 1, text: "吟唱，", newlineAfter: false },
        { ...command("moon_chant", 0, 0, 2060), sourcePlayerId: 3, targetPlayerId: 1, text: "为全体恢复2%血量", newlineAfter: true },
        { ...command("status_sync", 0, 0, 2060), sourcePlayerId: 3, targetPlayerId: 1, text: "", newlineAfter: true },
        { ...command("status_sync", 0, 0, 520), sourcePlayerId: 3, targetPlayerId: 2, text: "", newlineAfter: true },
        { ...command("death", 1, 0, 0), sourcePlayerId: 4, targetPlayerId: 3, text: "月之子消失了", newlineAfter: true },
        { ...command("moon_chant", 0, 0, 2120), sourcePlayerId: 3, targetPlayerId: 1, text: "月之子", newlineAfter: false },
        { ...command("moon_chant", 16, 0, 2120), sourcePlayerId: 3, targetPlayerId: 1, text: "吟唱，", newlineAfter: false },
        { ...command("moon_chant", 0, 0, 2120), sourcePlayerId: 3, targetPlayerId: 1, text: "为全体恢复2%血量", newlineAfter: true },
        { ...command("status_sync", 0, 0, 2120), sourcePlayerId: 3, targetPlayerId: 1, text: "", newlineAfter: true },
        { ...command("status_sync", 0, 0, 540), sourcePlayerId: 3, targetPlayerId: 2, text: "", newlineAfter: true },
      ],
    };
    const { container } = render(<BattlePlayback battle={moonBattle} onRestart={() => {}} />);
    act(() => vi.advanceTimersByTime(0));
    expect(container.querySelector(".tone-life-wheel")?.textContent).toBe("月之子");
    expect(container.querySelector(".battle-line")?.textContent).toBe("mili召唤出了月之子");
    for (let index = 0; index < 12; index += 1) {
      act(() => vi.advanceTimersByTime(110));
      act(() => vi.advanceTimersByTime(0));
    }
    const battleLines = Array.from(container.querySelectorAll(".battle-line")).map((line) => line.textContent);
    expect(battleLines.filter((line) => line === "月之子吟唱，为全体恢复2%血量")).toHaveLength(2);
    expect(battleLines).toContain("月之子消失了");
    expect(battleLines.join("\n")).not.toContain("？？？恢复");
    act(() => vi.runOnlyPendingTimers());
  });

  it("summons the scientific witch and Lancelot zero immediately, then keeps both special modes under normal attack text", () => {
    vi.useFakeTimers();
    const mili = { ...player, id: 1, teamId: 1, seatId: 1, inputIndex: 1, name: "mili", hp: 3000, maxHp: 3000 };
    const enemy = { ...player, id: 2, teamId: 2, seatId: 2, inputIndex: 2, name: "敌人", hp: 90, maxHp: 100, freezeStrength: 1, freezeLayers: 1, burnStrength: 20, burnLayers: 1 };
    const witch = {
      ...player, id: 3, teamId: 1, seatId: 3, inputIndex: 3, name: "科学性实验魔女", hp: 3000, maxHp: 3000,
      physicalAttack: 0, physicalDefense: 0, magicAttack: 250, magicDefense: 100, wisdom: 0, speed: 3000,
      isFamiliar: true, ownerPlayerId: 1,
    };
    const lancelot = {
      ...player, id: 4, teamId: 1, seatId: 4, inputIndex: 4, name: "兰斯洛特0号", hp: 20000, maxHp: 20000,
      physicalAttack: 150, physicalDefense: 100, magicAttack: 0, magicDefense: 0, wisdom: 0, speed: 2000,
      isFamiliar: true, ownerPlayerId: 3,
    };
    const witchBattle: CppBattleSimulationResponse = {
      ...battle,
      initialPlayers: [mili, enemy],
      finalPlayers: [mili, enemy, witch, lancelot],
      commands: [
        { ...command("witch_summon", 0, 0, 3000), sourcePlayerId: 1, targetPlayerId: 3, text: "mili召唤出了", newlineAfter: false },
        { ...command("witch_summon", 10, 0, 3000), sourcePlayerId: 1, targetPlayerId: 3, text: "科学性实验魔女", newlineAfter: false },
        { ...command("summon_spawn", 0, 0, 3000), sourcePlayerId: 1, targetPlayerId: 3, text: "", newlineAfter: true },
        { ...command("lancelot_summon", 0, 0, 20000), sourcePlayerId: 3, targetPlayerId: 4, text: "科学性实验魔女召唤出了", newlineAfter: false },
        { ...command("lancelot_summon", 16, 0, 20000), sourcePlayerId: 3, targetPlayerId: 4, text: "兰斯洛特0号", newlineAfter: false },
        { ...command("summon_spawn", 0, 0, 20000), sourcePlayerId: 3, targetPlayerId: 4, text: "", newlineAfter: true },
        { ...command("normal_attack", 0, 0, 100), sourcePlayerId: 4, targetPlayerId: 2, text: "兰斯洛特0号发起攻击，敌人受到", newlineAfter: false },
        { ...command("normal_attack_damage", 3, 10, 90), sourcePlayerId: 4, targetPlayerId: 2, text: "10", newlineAfter: false },
        { ...command("normal_attack_damage", 0, 10, 90), sourcePlayerId: 4, targetPlayerId: 2, text: "伤害", newlineAfter: true, freezeStrength: 1, freezeLayers: 1 },
        { ...command("status_sync", 0, 0, 90), sourcePlayerId: 4, targetPlayerId: 2, text: "", newlineAfter: true, freezeStrength: 1, freezeLayers: 1 },
        { ...command("normal_attack", 0, 0, 90), sourcePlayerId: 4, targetPlayerId: 2, text: "兰斯洛特0号发起攻击", newlineAfter: true, burnStrength: 20, burnLayers: 1 },
      ],
    };
    const { container } = render(<BattlePlayback battle={witchBattle} onRestart={() => {}} />);
    act(() => vi.advanceTimersByTime(0));
    for (let index = 0; index < 14; index += 1) {
      act(() => vi.advanceTimersByTime(110));
      act(() => vi.advanceTimersByTime(0));
    }
    const battleLines = Array.from(container.querySelectorAll(".battle-line")).map((line) => line.textContent);
    expect(battleLines).toContain("mili召唤出了科学性实验魔女");
    expect(battleLines).toContain("科学性实验魔女召唤出了兰斯洛特0号");
    expect(battleLines).toContain("兰斯洛特0号发起攻击，敌人受到10伤害");
    expect(battleLines).toContain("兰斯洛特0号发起攻击");
    expect(battleLines.join("\n")).not.toContain("兰斯洛特0号冰冻");
    expect(battleLines.join("\n")).not.toContain("兰斯洛特0号烧伤");
    const ownerSlotUnits = container.querySelector(".unit-owner-slot")?.querySelectorAll<HTMLElement>(".unit-readout");
    expect(ownerSlotUnits).toHaveLength(3);
    expect(ownerSlotUnits?.[1]?.textContent).toContain("科学性实验魔女");
    expect(ownerSlotUnits?.[2]?.textContent).toContain("兰斯洛特0号");
    act(() => vi.runOnlyPendingTimers());
  });

  it("brews rebirth potion three times, sings the four-line ritual, and transforms scrap into Galahad One", () => {
    vi.useFakeTimers();
    const mili = { ...player, id: 1, teamId: 1, seatId: 1, inputIndex: 1, name: "mili", hp: 3000, maxHp: 3000 };
    const witch = {
      ...player, id: 2, teamId: 1, seatId: 2, inputIndex: 2, name: "科学性实验魔女", hp: 3000, maxHp: 3000,
      physicalAttack: 0, physicalDefense: 0, magicAttack: 250, magicDefense: 100, wisdom: 0, speed: 3000,
      isFamiliar: true, ownerPlayerId: 1,
    };
    const lancelot = {
      ...player, id: 3, teamId: 1, seatId: 3, inputIndex: 3, name: "兰斯洛特0号", hp: 0, maxHp: 800,
      physicalAttack: 150, physicalDefense: 100, magicAttack: 0, magicDefense: 0, wisdom: 0, speed: 2000,
      isFamiliar: true, ownerPlayerId: 2, alive: false,
    };
    const galahad = {
      ...player, id: 4, teamId: 1, seatId: 4, inputIndex: 4, name: "加拉哈德1号", hp: 1200, maxHp: 1200,
      physicalAttack: 160, physicalDefense: 0, magicAttack: 180, magicDefense: 0, wisdom: 0, speed: 2500,
      isFamiliar: true, ownerPlayerId: 2,
    };
    const enemy = { ...player, id: 5, teamId: 2, seatId: 5, inputIndex: 5, name: "敌人" };
    const potionCast = (text: string, renderTone: number, newlineAfter: boolean) => ({
      ...command("rebirth_potion", renderTone, 0, 3000), sourcePlayerId: 2, targetPlayerId: 2, text, newlineAfter,
    });
    const potionBattle: CppBattleSimulationResponse = {
      ...battle,
      initialPlayers: [mili, witch, lancelot, enemy],
      finalPlayers: [mili, witch, lancelot, galahad, enemy],
      commands: [
        { ...command("death", 1, 0, 0), sourcePlayerId: 5, targetPlayerId: 3, text: "兰斯洛特0号消失了", newlineAfter: true, alive: false },
        { ...command("broken_lancelot_summon", 0, 0, 800), sourcePlayerId: 2, targetPlayerId: 4, text: "兰斯洛特0号变成了废铁!", newlineAfter: false, playerName: "破烂的兰斯洛特0号", playerMaxHp: 800, playerPhysicalAttack: 0, playerPhysicalDefense: -20, playerMagicAttack: 0, playerMagicDefense: 0, playerSpeed: 0 },
        { ...command("summon_spawn", 0, 0, 800), sourcePlayerId: 2, targetPlayerId: 4, text: "", newlineAfter: true, playerName: "破烂的兰斯洛特0号", playerMaxHp: 800, playerPhysicalAttack: 0, playerPhysicalDefense: -20, playerMagicAttack: 0, playerMagicDefense: 0, playerSpeed: 0 },
        potionCast("科学性实验魔女开始调制重生药水，Lulila talila~~~", 4, true),
        potionCast("科学性实验魔女开始调制重生药水，Lulila talila~~~", 4, true),
        potionCast("the magic potion of reanimation~~~", 4, true),
        potionCast("rise from bed my darling~~~", 4, true),
        potionCast("so I can see you again ~~~", 4, true),
        potionCast("so I can kill you again", 4, true),
        { ...command("familiar_transform", 4, 1200, 1200), sourcePlayerId: 2, targetPlayerId: 4, text: "加拉哈德1号复活了", newlineAfter: true, alive: true, playerName: "加拉哈德1号", playerMaxHp: 1200, playerPhysicalAttack: 160, playerPhysicalDefense: 0, playerMagicAttack: 180, playerMagicDefense: 0, playerSpeed: 2500 },
      ],
    };
    const { container } = render(<BattlePlayback battle={potionBattle} onRestart={() => {}} />);
    act(() => vi.advanceTimersByTime(0));
    for (let index = 0; index < 14; index += 1) {
      act(() => vi.advanceTimersByTime(110));
      act(() => vi.advanceTimersByTime(0));
    }
    const battleLines = Array.from(container.querySelectorAll(".battle-line")).map((line) => line.textContent);
    expect(battleLines).toContain("兰斯洛特0号变成了废铁!");
    expect(battleLines).toContain("科学性实验魔女开始调制重生药水，Lulila talila~~~");
    expect(battleLines).toContain("the magic potion of reanimation~~~");
    expect(battleLines).toContain("rise from bed my darling~~~");
    expect(battleLines).toContain("so I can see you again ~~~");
    expect(battleLines).toContain("so I can kill you again");
    expect(battleLines).toContain("加拉哈德1号复活了");
    const unitNames = Array.from(container.querySelectorAll<HTMLElement>(".unit-name")).map((element) => element.textContent);
    expect(unitNames).toContain("加拉哈德1号");
    expect(unitNames).not.toContain("破烂的兰斯洛特0号");
    const galahadReadout = Array.from(container.querySelectorAll<HTMLElement>(".unit-readout")).find((element) => element.querySelector(".unit-name")?.textContent === "加拉哈德1号");
    expect(galahadReadout?.querySelector(".hp-values")?.textContent).toContain("1200/1200");
    expect(galahadReadout?.classList.contains("is-defeated")).toBe(false);
    act(() => vi.runOnlyPendingTimers());
  });

  it("renders all three scientific witch thresholds and three distinct blue laser targets without healing", () => {
    vi.useFakeTimers();
    const witch = { ...player, id: 2, teamId: 1, seatId: 2, inputIndex: 2, name: "科学性实验魔女", hp: 90, maxHp: 100 };
    const galahad = { ...player, id: 3, teamId: 1, seatId: 3, inputIndex: 3, name: "加拉哈德1号", hp: 1200, maxHp: 1200, physicalAttack: 160, physicalDefense: 100, magicDefense: 100 };
    const enemies = [
      { ...player, id: 4, teamId: 2, seatId: 4, inputIndex: 4, name: "敌人甲", hp: 1000, maxHp: 1000 },
      { ...player, id: 5, teamId: 2, seatId: 5, inputIndex: 5, name: "敌人乙", hp: 1000, maxHp: 1000 },
      { ...player, id: 6, teamId: 2, seatId: 6, inputIndex: 6, name: "敌人丙", hp: 1000, maxHp: 1000 },
    ];
    const phaseBattle: CppBattleSimulationResponse = {
      ...battle,
      initialPlayers: [witch, galahad, ...enemies],
      finalPlayers: [witch, galahad, { ...enemies[0], hp: 700 }, { ...enemies[1], hp: 700 }, { ...enemies[2], hp: 700 }],
      commands: [
        { ...command("witch_phase_50", 16, 0, 90), sourcePlayerId: 2, targetPlayerId: 3, text: "科学性实验魔女使用加拉哈德你永远不会倒下", newlineAfter: true },
        { ...command("witch_phase_30", 16, 0, 90), sourcePlayerId: 2, targetPlayerId: 3, text: "科学性实验魔女使用talila tulila，加拉哈德1号属性提升", newlineAfter: true },
        { ...command("witch_phase_10", 16, 0, 90), sourcePlayerId: 2, targetPlayerId: 3, text: "科学性实验魔女使用多路化，加拉哈德1号的激光枪指向三个敌人", newlineAfter: true },
        { ...command("galahad_laser", 7, 0, 1200), sourcePlayerId: 3, targetPlayerId: 4, text: "加拉哈德1号 激光枪射击敌人甲，敌人甲受到", newlineAfter: false },
        { ...command("galahad_laser_damage", 3, 300, 700), sourcePlayerId: 3, targetPlayerId: 4, text: "300", newlineAfter: true },
        { ...command("galahad_laser", 7, 0, 1200), sourcePlayerId: 3, targetPlayerId: 5, text: "加拉哈德1号 激光枪射击敌人乙，敌人乙受到", newlineAfter: false },
        { ...command("galahad_laser_damage", 3, 300, 700), sourcePlayerId: 3, targetPlayerId: 5, text: "300", newlineAfter: true },
        { ...command("galahad_laser", 7, 0, 1200), sourcePlayerId: 3, targetPlayerId: 6, text: "加拉哈德1号 激光枪射击敌人丙，敌人丙受到", newlineAfter: false },
        { ...command("galahad_laser_damage", 3, 300, 700), sourcePlayerId: 3, targetPlayerId: 6, text: "300", newlineAfter: true },
      ],
    };
    const { container, unmount } = render(<BattlePlayback battle={phaseBattle} onRestart={() => {}} />);
    act(() => vi.advanceTimersByTime(0));
    for (let index = 0; index < 12; index += 1) {
      act(() => vi.advanceTimersByTime(110));
      act(() => vi.advanceTimersByTime(0));
    }
    const battleText = container.textContent ?? "";
    expect(battleText).toContain("加拉哈德你永远不会倒下");
    expect(battleText).toContain("talila tulila");
    expect(battleText).toContain("多路化");
    expect(container.querySelectorAll(".tone-thunder")).toHaveLength(3);
    expect(container.querySelectorAll(".tone-heal")).toHaveLength(0);
    expect(battleText).not.toContain("恢复");
    unmount();
  });

  it("renders K-2's silver summon, red iron-blood lotus, and an extra enemy burn tick", () => {
    vi.useFakeTimers();
    const mili = { ...player, id: 1, teamId: 1, seatId: 1, inputIndex: 1, name: "mili", hp: 3000, maxHp: 3000 };
    const enemy = { ...player, id: 2, teamId: 2, seatId: 2, inputIndex: 2, name: "敌人", hp: 98, maxHp: 100 };
    const k2 = {
      ...player, id: 3, teamId: 1, seatId: 3, inputIndex: 3, name: "K-2", hp: 500, maxHp: 500,
      physicalAttack: 150, physicalDefense: 25, magicAttack: 0, magicDefense: 0, wisdom: 0, speed: 1500,
      isFamiliar: true, ownerPlayerId: 1,
    };
    const k2Battle: CppBattleSimulationResponse = {
      ...battle,
      initialPlayers: [mili, enemy],
      finalPlayers: [{ ...mili, burnStrength: 2, burnLayers: 10 }, { ...enemy, hp: 96, burnStrength: 2, burnLayers: 9 }, { ...k2, burnStrength: 2, burnLayers: 10 }],
      commands: [
        { ...command("k2_summon", 0, 0, 500), sourcePlayerId: 1, targetPlayerId: 3, text: "mili召唤出了", newlineAfter: false },
        { ...command("k2_summon", 10, 0, 500), sourcePlayerId: 1, targetPlayerId: 3, text: "K-2", newlineAfter: true },
        { ...command("summon_spawn", 0, 0, 500), sourcePlayerId: 1, targetPlayerId: 3, text: "", newlineAfter: true },
        { ...command("iron_blood_lotus", 0, 0, 3000), sourcePlayerId: 3, targetPlayerId: 1, text: "K-2使用", newlineAfter: false },
        { ...command("iron_blood_lotus", 3, 0, 3000), sourcePlayerId: 3, targetPlayerId: 1, text: "Iron Lotus", newlineAfter: false },
        { ...command("iron_blood_lotus", 0, 0, 3000), sourcePlayerId: 3, targetPlayerId: 1, text: "，对全体施加1级5层烧伤", newlineAfter: true },
        { ...command("status_sync", 0, 0, 3000), sourcePlayerId: 3, targetPlayerId: 1, text: "", newlineAfter: true, burnStrength: 1, burnLayers: 5 },
        { ...command("status_sync", 0, 0, 98), sourcePlayerId: 3, targetPlayerId: 2, text: "", newlineAfter: true, burnStrength: 1, burnLayers: 5 },
        { ...command("status_sync", 0, 0, 500), sourcePlayerId: 3, targetPlayerId: 3, text: "", newlineAfter: true, burnStrength: 1, burnLayers: 5 },
        { ...command("iron_blood_lotus", 0, 0, 500), sourcePlayerId: 3, targetPlayerId: 3, text: "K-2使用", newlineAfter: false },
        { ...command("iron_blood_lotus", 3, 0, 500), sourcePlayerId: 3, targetPlayerId: 3, text: "Iron Lotus", newlineAfter: false },
        { ...command("iron_blood_lotus", 0, 0, 500), sourcePlayerId: 3, targetPlayerId: 3, text: "，对全体施加1级5层烧伤", newlineAfter: true },
        { ...command("status_sync", 0, 0, 3000), sourcePlayerId: 3, targetPlayerId: 1, text: "", newlineAfter: true, burnStrength: 2, burnLayers: 10 },
        { ...command("status_sync", 0, 0, 98), sourcePlayerId: 3, targetPlayerId: 2, text: "", newlineAfter: true, burnStrength: 2, burnLayers: 10 },
        { ...command("status_sync", 0, 0, 500), sourcePlayerId: 3, targetPlayerId: 3, text: "", newlineAfter: true, burnStrength: 2, burnLayers: 10 },
        { ...command("k2_burn", 13, 0, 500), sourcePlayerId: 3, targetPlayerId: 3, text: "K-2令全体再次烧伤", newlineAfter: true },
        { ...command("status_sync", 0, 0, 96), sourcePlayerId: 3, targetPlayerId: 2, text: "", newlineAfter: true, burnStrength: 2, burnLayers: 9 },
      ],
    };
    const { container } = render(<BattlePlayback battle={k2Battle} onRestart={() => {}} />);
    act(() => vi.advanceTimersByTime(0));
    for (let index = 0; index < 20; index += 1) {
      act(() => vi.advanceTimersByTime(110));
      act(() => vi.advanceTimersByTime(0));
    }
    expect(container.querySelector(".tone-ironwall")?.textContent).toBe("K-2");
    const battleLines = Array.from(container.querySelectorAll(".battle-line")).map((line) => line.textContent);
    expect(battleLines).toContain("mili召唤出了K-2");
    expect(battleLines).toContain("K-2使用Iron Lotus，对全体施加1级5层烧伤");
    expect(battleLines).toContain("K-2令全体再次烧伤");
    expect(battleLines.join("\n")).not.toContain("敌人再次受到");
    expect(container.querySelector<HTMLElement>('.unit-name[title="烧伤 2/10"]')).toBeNull();
    expect(container.querySelector<HTMLElement>('.unit-status-burn[title="烧伤 2/9"]')?.textContent).toBe("2/9");
    expect(container.querySelectorAll(".unit-readout.is-familiar")).toHaveLength(1);
    expect(Array.from(container.querySelectorAll<HTMLElement>(".unit-readout")).find((unit) => unit.querySelector(".unit-name")?.textContent === "K-2")?.querySelector(".hp-values")?.textContent).toContain("500/500");
    act(() => vi.runOnlyPendingTimers());
  });

  it("adds every mili familiar, including the witch's nested Lancelot, to the left-side familiar list", () => {
    vi.useFakeTimers();
    const mili = { ...player, id: 1, teamId: 1, seatId: 1, inputIndex: 1, name: "mili", hp: 3000, maxHp: 3000 };
    const moon = { ...player, id: 2, teamId: 1, seatId: 2, inputIndex: 2, name: "月之子", hp: 50, maxHp: 50, physicalDefense: -50, isFamiliar: true, ownerPlayerId: 1 };
    const k2 = { ...player, id: 3, teamId: 1, seatId: 3, inputIndex: 3, name: "K-2", hp: 500, maxHp: 500, physicalDefense: 25, isFamiliar: true, ownerPlayerId: 1 };
    const witch = { ...player, id: 4, teamId: 1, seatId: 4, inputIndex: 4, name: "科学性实验魔女", hp: 1000, maxHp: 1000, isFamiliar: true, ownerPlayerId: 1 };
    const lancelot = { ...player, id: 5, teamId: 1, seatId: 5, inputIndex: 5, name: "兰斯洛特0号", hp: 800, maxHp: 800, isFamiliar: true, ownerPlayerId: 4 };
    const familiarBattle: CppBattleSimulationResponse = {
      ...battle,
      initialPlayers: [mili],
      finalPlayers: [mili, moon, k2, witch, lancelot],
      commands: [
        { ...command("summon_spawn", 0, 0, 50), sourcePlayerId: 1, targetPlayerId: 2, text: "", newlineAfter: true },
        { ...command("summon_spawn", 0, 0, 500), sourcePlayerId: 1, targetPlayerId: 3, text: "", newlineAfter: true },
        { ...command("summon_spawn", 0, 0, 1000), sourcePlayerId: 1, targetPlayerId: 4, text: "", newlineAfter: true },
        { ...command("summon_spawn", 0, 0, 1000), sourcePlayerId: 1, targetPlayerId: 4, text: "", newlineAfter: true },
        { ...command("summon_spawn", 0, 0, 800), sourcePlayerId: 4, targetPlayerId: 5, text: "", newlineAfter: true },
      ],
    };
    const { container } = render(<BattlePlayback battle={familiarBattle} onRestart={() => {}} />);
    act(() => vi.advanceTimersByTime(0));
    for (let index = 0; index < 8; index += 1) {
      act(() => vi.advanceTimersByTime(110));
      act(() => vi.advanceTimersByTime(0));
    }
    const names = Array.from(container.querySelectorAll<HTMLElement>(".unit-owner-slot .unit-name")).map((element) => element.textContent);
    expect(names).toEqual(["mili", "月之子", "K-2", "科学性实验魔女", "兰斯洛特0号"]);
    expect(container.querySelectorAll(".unit-owner-slot .unit-readout.is-familiar")).toHaveLength(4);
    act(() => vi.runOnlyPendingTimers());
  });

  it("removes the witch and her nested Lancelot together when the witch dies", () => {
    vi.useFakeTimers();
    const mili = { ...player, id: 1, teamId: 1, seatId: 1, inputIndex: 1, name: "mili" };
    const witch = { ...player, id: 2, teamId: 1, seatId: 2, inputIndex: 2, name: "科学性实验魔女", hp: 1000, maxHp: 1000, isFamiliar: true, ownerPlayerId: 1 };
    const lancelot = { ...player, id: 3, teamId: 1, seatId: 3, inputIndex: 3, name: "兰斯洛特0号", hp: 800, maxHp: 800, isFamiliar: true, ownerPlayerId: 2 };
    const witchDepartureBattle: CppBattleSimulationResponse = {
      ...battle,
      initialPlayers: [mili],
      finalPlayers: [mili],
      commands: [
        { ...command("summon_spawn", 0, 0, 1000), sourcePlayerId: 1, targetPlayerId: 2, text: "", newlineAfter: true },
        { ...command("summon_spawn", 0, 0, 800), sourcePlayerId: 2, targetPlayerId: 3, text: "", newlineAfter: true },
        { ...command("death", 0, 0, 0), sourcePlayerId: 0, targetPlayerId: 2, text: "科学性实验魔女消失了", newlineAfter: true, alive: false },
        { ...command("familiar_depart", 0, 0, 0), sourcePlayerId: 2, targetPlayerId: 3, text: "兰斯洛特0号随本体消失了", newlineAfter: true, alive: false },
        { ...command("familiar_depart", 0, 0, 0), sourcePlayerId: 1, targetPlayerId: 2, text: "科学性实验魔女随本体消失了", newlineAfter: true, alive: false },
      ],
    };
    const { container } = render(<BattlePlayback battle={witchDepartureBattle} onRestart={() => {}} />);
    act(() => vi.advanceTimersByTime(0));
    for (let index = 0; index < 10; index += 1) {
      act(() => vi.advanceTimersByTime(110));
      act(() => vi.advanceTimersByTime(0));
    }
    expect(Array.from(container.querySelectorAll<HTMLElement>(".unit-owner-slot .unit-name")).map((element) => element.textContent)).toEqual(["mili"]);
    expect(container.textContent).toContain("兰斯洛特0号随本体消失了");
    act(() => vi.runOnlyPendingTimers());
  });

  it("renders red Lament with a two-layer mourning badge, then accepts allied and self targets while layers are consumed", () => {
    vi.useFakeTimers();
    const mili = { ...player, id: 1, teamId: 1, seatId: 1, inputIndex: 1, name: "mili" };
    const mourner = { ...player, id: 2, teamId: 2, seatId: 2, inputIndex: 2, name: "哀悼者" };
    const ally = { ...player, id: 3, teamId: 2, seatId: 3, inputIndex: 3, name: "队友" };
    const lamentBattle: CppBattleSimulationResponse = {
      ...battle,
      initialPlayers: [mili, mourner, ally],
      finalPlayers: [mili, { ...mourner, hp: 85, lamentLayers: 0 }, { ...ally, hp: 80, lamentLayers: 1 }],
      commands: [
        { ...command("lament", 0, 0, 100), sourcePlayerId: 1, targetPlayerId: 2, text: "mili使用", newlineAfter: false, lamentLayers: 2 },
        { ...command("lament", 3, 0, 100), sourcePlayerId: 1, targetPlayerId: 2, text: "Lament", newlineAfter: false, lamentLayers: 2 },
        { ...command("lament", 0, 0, 100), sourcePlayerId: 1, targetPlayerId: 2, text: "，哀悼者获得2层", newlineAfter: false, lamentLayers: 2 },
        { ...command("lament", 3, 0, 100), sourcePlayerId: 1, targetPlayerId: 2, text: "哀悼", newlineAfter: true, lamentLayers: 2 },
      ],
    };
    const { container, unmount } = render(<BattlePlayback battle={lamentBattle} onRestart={() => {}} />);
    act(() => vi.advanceTimersByTime(0));
    act(() => vi.advanceTimersByTime(110));
    expect(container.querySelector(".tone-damage")?.textContent).toBe("Lament");
    expect(container.querySelector<HTMLElement>('.unit-status-lament[title="哀悼 1/2"]')?.textContent).toBe("1/2");
    expect(container.querySelector<HTMLImageElement>('.unit-status-lament img')?.src).toContain("data:image/svg+xml;base64,");
    const statusCss = readFileSync(resolve(process.cwd(), "client/src/index.css"), "utf8");
    expect(statusCss).toContain(".unit-status-lament img { width: 14px; height: 14px;");
    unmount();

    const attackBattle: CppBattleSimulationResponse = {
      ...lamentBattle,
      initialPlayers: [{ ...mourner, lamentLayers: 2 }, ally],
      commands: [
        { ...command("normal_attack", 0, 0, 100), sourcePlayerId: 2, targetPlayerId: 3, text: "哀悼者发起攻击，队友受到", newlineAfter: false },
        { ...command("normal_attack_damage", 3, 20, 80), sourcePlayerId: 2, targetPlayerId: 3, text: "20", newlineAfter: true },
        { ...command("status_sync", 0, 0, 80), sourcePlayerId: 2, targetPlayerId: 2, text: "", newlineAfter: true, lamentLayers: 1 },
        { ...command("normal_attack", 0, 0, 100), sourcePlayerId: 2, targetPlayerId: 2, text: "哀悼者发起攻击，哀悼者受到", newlineAfter: false },
        { ...command("normal_attack_damage", 3, 15, 85), sourcePlayerId: 2, targetPlayerId: 2, text: "15", newlineAfter: true },
        { ...command("status_sync", 0, 0, 85), sourcePlayerId: 2, targetPlayerId: 2, text: "", newlineAfter: true, lamentLayers: 0 },
      ],
    };
    const replay = render(<BattlePlayback battle={attackBattle} onRestart={() => {}} />);
    act(() => vi.advanceTimersByTime(0));
    for (let index = 0; index < 20; index += 1) {
      act(() => vi.advanceTimersByTime(110));
      act(() => vi.advanceTimersByTime(0));
    }
    expect(replay.container.textContent).toContain("哀悼者发起攻击，队友受到20");
    expect(replay.container.textContent).toContain("哀悼者发起攻击，哀悼者受到15");
    expect(replay.container.querySelector(".unit-status-lament")).toBeNull();
    act(() => vi.runOnlyPendingTimers());
  });

  it("renders world.search(you); as a soft multicolor skill while applying its physical damage", () => {
    vi.useFakeTimers();
    const mili = { ...player, id: 1, teamId: 1, seatId: 1, inputIndex: 1, name: "mili", physicalAttack: 200, magicDefense: 50 };
    const target = { ...player, id: 2, teamId: 2, seatId: 2, inputIndex: 2, name: "目标", hp: 100, maxHp: 100 };
    const searchBattle: CppBattleSimulationResponse = {
      ...battle,
      initialPlayers: [mili, target],
      finalPlayers: [mili, { ...target, hp: 40 }],
      commands: [
        { ...command("world_search", 0, 0, 100), sourcePlayerId: 1, targetPlayerId: 2, text: "mili使用", newlineAfter: false },
        { ...command("world_search", 0, 0, 100), sourcePlayerId: 1, targetPlayerId: 2, text: "world.search(you);", newlineAfter: false },
        { ...command("world_search_damage", 3, 60, 40), sourcePlayerId: 1, targetPlayerId: 2, text: "，目标受到60伤害", newlineAfter: true },
      ],
    };
    const { container } = render(<BattlePlayback battle={searchBattle} onRestart={() => {}} />);
    act(() => vi.advanceTimersByTime(0));
    act(() => vi.advanceTimersByTime(110));
    expect(container.querySelector(".world-search-text")?.textContent).toBe("world.search(you);");
    expect(container.querySelector('[aria-label="目标 生命 40/100"]')).not.toBeNull();
    act(() => vi.runOnlyPendingTimers());
  });

  it("plays execute once before each world.execute damage and restarts the strong screen shake on every hit", () => {
    vi.useFakeTimers();
    const mili = { ...player, id: 1, teamId: 1, seatId: 1, inputIndex: 1, name: "mili", hp: 70, maxHp: 3000 };
    const first = { ...player, id: 2, teamId: 2, seatId: 2, inputIndex: 2, name: "甲", hp: 30, maxHp: 100 };
    const last = { ...player, id: 3, teamId: 3, seatId: 3, inputIndex: 3, name: "乙", hp: 50, maxHp: 100 };
    const executeBattle: CppBattleSimulationResponse = {
      ...battle,
      initialPlayers: [mili, first, last],
      finalPlayers: [last],
      commands: [
        { ...command("world_execute", 0, 0, 70), sourcePlayerId: 1, targetPlayerId: 1, text: "mili使用", newlineAfter: false },
        { ...command("world_execute", 0, 0, 70), sourcePlayerId: 1, targetPlayerId: 1, text: "world.execute(me);", newlineAfter: true },
        { ...command("world_execute", 0, 0, 70), sourcePlayerId: 1, targetPlayerId: 1, text: "execute", newlineAfter: true },
        { ...command("world_execute_damage", 3, 30, 40), sourcePlayerId: 1, targetPlayerId: 1, text: "mili受到30伤害", newlineAfter: true },
        { ...command("world_execute", 0, 0, 30), sourcePlayerId: 1, targetPlayerId: 2, text: "execute", newlineAfter: true },
        { ...command("world_execute_damage", 3, 30, 0), sourcePlayerId: 1, targetPlayerId: 2, text: "甲受到30伤害", newlineAfter: true },
        { ...command("death", 0, 0, 0), sourcePlayerId: 0, targetPlayerId: 2, text: "甲消失了", newlineAfter: true, alive: false },
        { ...command("world_execute", 0, 0, 20), sourcePlayerId: 1, targetPlayerId: 3, text: "execute", newlineAfter: true },
        { ...command("world_execute_damage", 3, 20, 30), sourcePlayerId: 1, targetPlayerId: 3, text: "乙受到20伤害", newlineAfter: true },
        { ...command("world_execute", 0, 0, 20), sourcePlayerId: 1, targetPlayerId: 1, text: "execute", newlineAfter: true },
        { ...command("world_execute_damage", 3, 20, 0), sourcePlayerId: 1, targetPlayerId: 1, text: "mili受到20伤害", newlineAfter: true },
        { ...command("death", 0, 0, 0), sourcePlayerId: 0, targetPlayerId: 1, text: "mili消失了", newlineAfter: true, alive: false },
        { ...command("battle_end", 1, 0, 0), sourcePlayerId: 0, targetPlayerId: 0, text: "队伍3获胜", newlineAfter: true },
      ],
    };
    const { container } = render(<BattlePlayback battle={executeBattle} onRestart={() => {}} />);
    act(() => vi.advanceTimersByTime(0));
    for (let index = 0; index < 28; index += 1) {
      act(() => vi.advanceTimersByTime(110));
      act(() => vi.advanceTimersByTime(0));
    }
    expect(Array.from(container.querySelectorAll(".world-execute-text")).map((element) => element.textContent)).toEqual(["world.execute(me);", "execute", "execute", "execute", "execute"]);
    expect(container.querySelector(".arena-screen.is-execute-shaking")).not.toBeNull();
    expect(container.textContent).toContain("乙受到20伤害");
    expect(Array.from(container.querySelectorAll(".battle-line")).at(-1)?.textContent).toBe("队伍3获胜");
    act(() => vi.runOnlyPendingTimers());
  });

  it("renders only the guard label in silver, keeps guarded names normal, and separates split damage with a space", () => {
    vi.useFakeTimers();
    const attacker = { ...player, id: 1, teamId: 2, seatId: 1, inputIndex: 1, name: "攻击者" };
    const protectedTeammate = { ...player, id: 2, teamId: 1, seatId: 2, inputIndex: 2, name: "受护者" };
    const guardian = { ...player, id: 3, teamId: 1, seatId: 3, inputIndex: 3, name: "守护者" };
    const silverGuardBattle: CppBattleSimulationResponse = {
      ...battle,
      initialPlayers: [attacker, protectedTeammate, guardian],
      finalPlayers: [attacker, { ...protectedTeammate, hp: 60 }, { ...guardian, hp: 60 }],
      commands: [
        { ...command("guard", 10, 0, 100), sourcePlayerId: 3, targetPlayerId: 2, text: "守护者", newlineAfter: false },
        { ...command("guard", 10, 0, 100), sourcePlayerId: 3, targetPlayerId: 2, text: "守护", newlineAfter: false },
        { ...command("guard", 10, 0, 100), sourcePlayerId: 3, targetPlayerId: 2, text: "受护者", newlineAfter: false },
        { ...command("normal_attack", 10, 0, 100), sourcePlayerId: 1, targetPlayerId: 2, text: "受护者", newlineAfter: false },
        { ...command("normal_attack", 0, 0, 100), sourcePlayerId: 1, targetPlayerId: 2, text: "受到", newlineAfter: false },
        { ...command("normal_attack_damage", 3, 40, 60), sourcePlayerId: 1, targetPlayerId: 2, text: "40", newlineAfter: false },
        { ...command("normal_attack", 0, 0, 100), sourcePlayerId: 1, targetPlayerId: 3, text: " ", newlineAfter: false },
        { ...command("normal_attack", 10, 0, 100), sourcePlayerId: 1, targetPlayerId: 3, text: "守护者", newlineAfter: false },
        { ...command("normal_attack", 0, 0, 100), sourcePlayerId: 1, targetPlayerId: 3, text: "受到", newlineAfter: false },
        { ...command("normal_attack_damage", 3, 40, 60), sourcePlayerId: 1, targetPlayerId: 3, text: "40", newlineAfter: true },
      ],
    };
    const { container } = render(<BattlePlayback battle={silverGuardBattle} onRestart={() => {}} />);
    act(() => vi.advanceTimersByTime(0));

    expect(Array.from(container.querySelectorAll(".tone-ironwall")).map((node) => node.textContent)).toEqual(["守护"]);
    expect(container.querySelector(".battle-line")?.textContent).toContain("40 守护者受到40");
    expect(container.querySelectorAll(".tone-damage")).toHaveLength(2);
    act(() => vi.runOnlyPendingTimers());
  });

  it("renders poison failure in deep green without a blank playback line", () => {
    vi.useFakeTimers();
    const poisonFailBattle: CppBattleSimulationResponse = {
      ...battle,
      commands: [
        { ...command("poison", 0, 0, 100), text: "测试者使用", newlineAfter: false },
        { ...command("poison", 14, 0, 100), text: "投毒", newlineAfter: false },
        { ...command("poison_fail", 0, 0, 100), text: "   ", newlineAfter: true },
      ],
    };
    const { container } = render(<BattlePlayback battle={poisonFailBattle} onRestart={() => {}} />);

    expect(container.querySelector(".battle-line")?.textContent).toBe("测试者使用投毒失败");
    expect(Array.from(container.querySelectorAll(".tone-poison")).map((node) => node.textContent)).toEqual(["投毒", "失败"]);
    expect(container.querySelectorAll(".battle-line")).toHaveLength(1);
    act(() => vi.runOnlyPendingTimers());
  });

  it("updates status badges on the first short-battle text segment instead of waiting for the full line", () => {
    vi.useFakeTimers();
    const poisoned = { ...player, id: 2, teamId: 2, seatId: 2, inputIndex: 2, name: "中毒者" };
    const shortStatusBattle: CppBattleSimulationResponse = {
      ...battle,
      longBattle: false,
      initialPlayers: [player, poisoned],
      finalPlayers: [player, { ...poisoned, poisonStrength: 25, poisonLayers: 1 }],
      commands: [
        { ...command("poison", 0, 0, 100), sourcePlayerId: 1, targetPlayerId: 2, text: "测试者使用", newlineAfter: false, poisonStrength: 25, poisonLayers: 1 },
        { ...command("poison", 14, 0, 100), sourcePlayerId: 1, targetPlayerId: 2, text: "投毒", newlineAfter: false, poisonStrength: 25, poisonLayers: 1 },
        { ...command("poison_apply", 0, 25, 1), sourcePlayerId: 1, targetPlayerId: 2, text: "，中毒者获得", newlineAfter: true, poisonStrength: 25, poisonLayers: 1 },
      ],
    };
    const { container } = render(<BattlePlayback battle={shortStatusBattle} onRestart={() => {}} />);
    const poisonedUnit = Array.from(container.querySelectorAll<HTMLElement>(".unit-readout")).find((unit) => unit.textContent?.includes("中毒者"));

    expect(container.querySelector(".battle-line")?.textContent).toBe("测试者使用");
    expect(poisonedUnit?.querySelector(".unit-status-poison b")?.textContent).toBe("25/1");
    act(() => vi.runOnlyPendingTimers());
  });

  it("keeps the same immediate status rate in long battles when damage follows a status snapshot", () => {
    vi.useFakeTimers();
    const poisoned = { ...player, id: 2, teamId: 2, seatId: 2, inputIndex: 2, name: "长局中毒者" };
    const longStatusBattle: CppBattleSimulationResponse = {
      ...battle,
      longBattle: true,
      initialPlayers: [player, poisoned],
      finalPlayers: [player, { ...poisoned, poisonStrength: 25, poisonLayers: 1, hp: 90 }],
      commands: [
        { ...command("status_sync", 0, 0, 100), targetPlayerId: 2, text: "", newlineAfter: false, poisonStrength: 25, poisonLayers: 1 },
        { ...command("poison_damage", 3, 10, 90), sourcePlayerId: 2, targetPlayerId: 2, text: "10", newlineAfter: true },
      ],
    };
    const { container } = render(<BattlePlayback battle={longStatusBattle} onRestart={() => {}} />);
    const poisonedUnit = Array.from(container.querySelectorAll<HTMLElement>(".unit-readout")).find((unit) => unit.textContent?.includes("长局中毒者"));

    expect(poisonedUnit?.querySelector(".unit-status-poison b")?.textContent).toBe("25/1");
    act(() => vi.runOnlyPendingTimers());
  });

  it("shows both string debuffs from the C++ snapshot on the first purple text segment", () => {
    vi.useFakeTimers();
    const target = { ...player, id: 2, teamId: 2, seatId: 2, inputIndex: 2, name: "放松警惕者" };
    const bossStringBattle: CppBattleSimulationResponse = {
      ...battle,
      initialPlayers: [player, target],
      finalPlayers: [player, { ...target, defenseDownStrength: 16, defenseDownLayers: 3, damageDownStrength: 16, damageDownLayers: 3 }],
      commands: [
        { ...command("boss_string", 0, 0, 100), sourcePlayerId: 1, targetPlayerId: 2, text: "张洋 ", newlineAfter: false, defenseDownStrength: 16, defenseDownLayers: 3, damageDownStrength: 16, damageDownLayers: 3 },
        { ...command("boss_string", 5, 10, 100), sourcePlayerId: 1, targetPlayerId: 2, text: "串", newlineAfter: true, defenseDownStrength: 16, defenseDownLayers: 3, damageDownStrength: 16, damageDownLayers: 3 },
      ],
    };
    const { container } = render(<BattlePlayback battle={bossStringBattle} onRestart={() => {}} />);
    const targetUnit = Array.from(container.querySelectorAll<HTMLElement>(".unit-readout")).find((unit) => unit.textContent?.includes("放松警惕者"));

    expect(container.querySelector(".battle-line")?.textContent).toBe("张洋 串");
    expect(targetUnit?.querySelector(".unit-status-defense-down b")?.textContent).toBe("16/3");
    expect(targetUnit?.querySelector(".unit-status-damage-down b")?.textContent).toBe("16/3");
    act(() => vi.runOnlyPendingTimers());
  });

  it("shows the purple square icon and 1/5 snapshot when the boss casts magic", () => {
    vi.useFakeTimers();
    const target = { ...player, id: 2, teamId: 2, seatId: 2, inputIndex: 2, name: "被方者" };
    const secondTarget = { ...player, id: 3, teamId: 2, seatId: 3, inputIndex: 3, name: "第二被方者" };
    const thirdTarget = { ...player, id: 4, teamId: 2, seatId: 4, inputIndex: 4, name: "第三被方者" };
    const magicBattle: CppBattleSimulationResponse = {
      ...battle,
      initialPlayers: [player, target, secondTarget, thirdTarget],
      finalPlayers: [player, { ...target, squareStrength: 1, squareLayers: 5 }, { ...secondTarget, squareStrength: 1, squareLayers: 5 }, { ...thirdTarget, squareStrength: 1, squareLayers: 5 }],
      commands: [
        { ...command("boss_magic", 0, 0, 100), sourcePlayerId: 1, targetPlayerId: 1, text: "张洋使用", newlineAfter: false },
        { ...command("boss_magic", 5, 0, 100), sourcePlayerId: 1, targetPlayerId: 1, text: "魔", newlineAfter: true },
        { ...command("boss_magic_square", 0, 1, 100), sourcePlayerId: 1, targetPlayerId: 2, text: "被方者被", newlineAfter: false, squareStrength: 1, squareLayers: 5 },
        { ...command("boss_magic_square", 5, 1, 100), sourcePlayerId: 1, targetPlayerId: 2, text: "方住了，", newlineAfter: false, squareStrength: 1, squareLayers: 5 },
        { ...command("boss_magic_square", 0, 1, 100), sourcePlayerId: 1, targetPlayerId: 3, text: "第二被方者被", newlineAfter: false, squareStrength: 1, squareLayers: 5 },
        { ...command("boss_magic_square", 5, 1, 100), sourcePlayerId: 1, targetPlayerId: 3, text: "方住了，", newlineAfter: false, squareStrength: 1, squareLayers: 5 },
        { ...command("boss_magic_square", 0, 1, 100), sourcePlayerId: 1, targetPlayerId: 4, text: "第三被方者被", newlineAfter: false, squareStrength: 1, squareLayers: 5 },
        { ...command("boss_magic_square", 5, 1, 100), sourcePlayerId: 1, targetPlayerId: 4, text: "方住了", newlineAfter: true, squareStrength: 1, squareLayers: 5 },
      ],
    };
    const { container } = render(<BattlePlayback battle={magicBattle} onRestart={() => {}} />);

    expect(container.querySelector(".battle-line")?.textContent).toBe("张洋使用魔");
    act(() => vi.advanceTimersByTime(64));
    const targetUnits = Array.from(container.querySelectorAll<HTMLElement>(".unit-readout")).filter((unit) => /被方者/.test(unit.textContent ?? ""));
    expect(Array.from(container.querySelectorAll(".battle-line")).at(-1)?.textContent).toBe("被方者被方住了，第二被方者被方住了，第三被方者被方住了");
    expect(container.textContent).not.toContain("获得5层1强度的方");
    expect(targetUnits).toHaveLength(3);
    expect(targetUnits.every((unit) => unit.querySelector(".unit-status-square b")?.textContent === "1/5")).toBe(true);
    expect(targetUnits[0]?.querySelector(".unit-status-square img")?.getAttribute("src")).toContain("data:image/svg+xml;base64,");
    act(() => vi.runOnlyPendingTimers());
  });

  it("renders the boss steal label in red without creating a damage animation", () => {
    vi.useFakeTimers();
    const target = { ...player, id: 2, teamId: 2, seatId: 2, inputIndex: 2, name: "被偷者" };
    const stealBattle: CppBattleSimulationResponse = {
      ...battle,
      initialPlayers: [player, target],
      finalPlayers: [player, target],
      commands: [
        { ...command("boss_steal", 0, 0, 100), sourcePlayerId: 1, targetPlayerId: 2, text: "张洋 ", newlineAfter: false },
        { ...command("boss_steal", 3, 0, 100), sourcePlayerId: 1, targetPlayerId: 2, text: "偷", newlineAfter: false },
        { ...command("boss_steal", 0, 0, 100), sourcePlayerId: 1, targetPlayerId: 2, text: "了被偷者的火球术!", newlineAfter: true },
      ],
    };
    const { container } = render(<BattlePlayback battle={stealBattle} onRestart={() => {}} />);

    expect(container.querySelector(".battle-line")?.textContent).toBe("张洋 偷了被偷者的火球术!");
    expect(Array.from(container.querySelectorAll(".tone-damage")).map((node) => node.textContent)).toEqual(["偷"]);
    expect(container.querySelector(".hp-loss")).toBeNull();
    act(() => vi.runOnlyPendingTimers());
  });

  it("confirms every stolen queue item separately, including duplicate skills and the silent fast action effect", () => {
    vi.useFakeTimers();
    const boss = { ...player, id: 1, teamId: 1, seatId: 1, inputIndex: 1, name: "张洋" };
    const target = { ...player, id: 2, teamId: 2, seatId: 2, inputIndex: 2, name: "被偷者" };
    const stolenQueueBattle: CppBattleSimulationResponse = {
      ...battle,
      initialPlayers: [boss, target],
      finalPlayers: [boss, { ...target, hp: 80 }],
      commands: [
        { ...command("boss_stolen_cast", 0, 0, 100), sourcePlayerId: 1, targetPlayerId: 1, skillId: 25, text: "张洋连放", newlineAfter: false },
        { ...command("boss_stolen_cast", 15, 0, 100), sourcePlayerId: 1, targetPlayerId: 1, skillId: 25, text: "快速行动", newlineAfter: true },
        { ...command("boss_stolen_cast", 0, 0, 100), sourcePlayerId: 1, targetPlayerId: 1, skillId: 25, text: "张洋连放", newlineAfter: false },
        { ...command("boss_stolen_cast", 13, 0, 100), sourcePlayerId: 1, targetPlayerId: 1, skillId: 25, text: "火球术", newlineAfter: true },
        { ...command("fireball_damage", 3, 10, 90), sourcePlayerId: 1, targetPlayerId: 2, text: "10", newlineAfter: true },
        { ...command("boss_stolen_cast", 0, 0, 100), sourcePlayerId: 1, targetPlayerId: 1, skillId: 25, text: "张洋连放", newlineAfter: false },
        { ...command("boss_stolen_cast", 13, 0, 100), sourcePlayerId: 1, targetPlayerId: 1, skillId: 25, text: "火球术", newlineAfter: true },
        { ...command("fireball_damage", 3, 10, 80), sourcePlayerId: 1, targetPlayerId: 2, text: "10", newlineAfter: true },
      ],
    };
    const { container } = render(<BattlePlayback battle={stolenQueueBattle} onRestart={() => {}} />);

    for (let index = 0; index < 6; index += 1) {
      act(() => vi.advanceTimersByTime(110));
      act(() => vi.advanceTimersByTime(0));
    }
    expect(Array.from(container.querySelectorAll(".battle-line")).map((line) => line.textContent)).toEqual([
      "张洋连放快速行动",
      "张洋连放火球术",
      "10",
      "张洋连放火球术",
      "10",
    ]);
    expect(Array.from(container.querySelectorAll(".tone-gold")).map((node) => node.textContent)).toEqual(["快速行动"]);
    expect(Array.from(container.querySelectorAll(".tone-fire")).map((node) => node.textContent)).toEqual(["火球术", "火球术"]);
    expect(container.textContent).not.toContain("20000行动值");
  });

  it("shows the failed-steal string message and plays a silent self-heal without heal spell text", () => {
    vi.useFakeTimers();
    const boss = { ...player, id: 1, teamId: 1, seatId: 1, inputIndex: 1, name: "张洋", hp: 40, maxHp: 100 };
    const target = { ...player, id: 2, teamId: 2, seatId: 2, inputIndex: 2, name: "被偷者" };
    const stealFallbackBattle: CppBattleSimulationResponse = {
      ...battle,
      initialPlayers: [boss, target],
      finalPlayers: [{ ...boss, hp: 65 }, target],
      commands: [
        { ...command("boss_steal_fail", 0, 0, 40), sourcePlayerId: 1, targetPlayerId: 2, text: "张洋啥也没偷到，", newlineAfter: false },
        { ...command("boss_steal_fail", 0, 0, 40), sourcePlayerId: 1, targetPlayerId: 2, text: "美美开串", newlineAfter: true },
        { ...command("heal", 4, 25, 65), sourcePlayerId: 1, targetPlayerId: 1, text: "25", newlineAfter: true },
      ],
    };
    const { container } = render(<BattlePlayback battle={stealFallbackBattle} onRestart={() => {}} />);

    expect(container.querySelector(".battle-line")?.textContent).toBe("张洋啥也没偷到，美美开串");
    expect(container.querySelector(".hp-loss")).toBeNull();
    act(() => vi.advanceTimersByTime(110));
    expect(Array.from(container.querySelectorAll(".battle-line")).at(-1)?.textContent).toBe("25");
    expect(container.textContent).not.toContain("治愈魔法");
    expect(container.querySelector(".hp-heal")).not.toBeNull();
    act(() => vi.runOnlyPendingTimers());
  });
});
