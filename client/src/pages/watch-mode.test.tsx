// @vitest-environment jsdom
import { act, cleanup, render } from "@testing-library/react";
import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";
import { BattlePlayback } from "./Home";
import type { CppBattleSimulationResponse, CppPlayerSnapshot, CppRenderCommand } from "@/lib/cppSnapshot";

const makePlayer = (id: number, name: string): CppPlayerSnapshot => ({
  id, teamId: id, seatId: id, inputIndex: id, name, hp: 100, maxHp: 100, mana: 0, maxMana: 100, manaRecovery: 0,
  physicalAttack: 10, physicalDefense: 0, magicAttack: 10, magicDefense: 0, wisdom: 100, speed: 100, alive: true,
});

const command = (text: string, animation: string, tone: number, value: number, after: number, newlineAfter: boolean): CppRenderCommand => ({
  sourcePlayerId: 1, targetPlayerId: 2, skillId: 1, value, valueAfter: after, renderTone: tone, frontEndAnimation: animation, text, newlineAfter,
});

const commands: CppRenderCommand[] = [
  command("羿使用三元剑，", "yi_tri_sword", 2, 0, 100, false),
  command("1段，", "yi_tri_sword", 0, 0, 100, false),
  command("beta", "yi_tri_sword", 0, 0, 100, false),
  command("受到", "yi_tri_sword", 0, 0, 100, false),
  command("10", "yi_tri_damage", 3, 10, 90, false),
  command("伤害", "yi_tri_damage", 0, 10, 90, false),
  command("2段，", "yi_tri_sword", 0, 0, 90, false),
  command("beta", "yi_tri_sword", 0, 0, 90, false),
  command("受到", "yi_tri_sword", 0, 0, 90, false),
  command("20", "yi_tri_damage", 3, 20, 70, false),
  command("伤害", "yi_tri_damage", 0, 20, 70, false),
  command("3段，", "yi_tri_sword", 0, 0, 70, false),
  command("beta", "yi_tri_sword", 0, 0, 70, false),
  command("受到", "yi_tri_sword", 0, 0, 70, false),
  command("30", "yi_tri_damage", 3, 30, 40, false),
  command("伤害", "yi_tri_damage", 0, 30, 40, true),
  command("战斗结束", "battle_end", 1, 0, 0, true),
];

const battle: CppBattleSimulationResponse = {
  rawText: "", longBattle: true, utf8ByteLength: 0, transportHash: 0, winnerTeamId: 1, momentCount: 1, executedActionCount: 1,
  initialPlayers: [makePlayer(1, "羿"), makePlayer(2, "beta")],
  finalPlayers: [makePlayer(1, "羿"), makePlayer(2, "beta")],
  commands,
};

describe("watch mode playback", () => {
  beforeEach(() => vi.useFakeTimers());
  afterEach(() => { cleanup(); vi.useRealTimers(); });

  it("reveals one skill segment at a time and settles damage per segment", () => {
    const { container } = render(<BattlePlayback battle={battle} watchMode onRestart={() => {}} />);
    const streamText = () => container.querySelector(".playback-stream")?.textContent ?? "";

    expect(streamText()).toContain("1段");
    expect(streamText()).not.toContain("2段");
    expect(container.textContent).toContain("90/100");

    act(() => { vi.advanceTimersByTime(630); });
    expect(streamText()).toContain("2段");
    expect(streamText()).not.toContain("3段");
    expect(container.textContent).toContain("70/100");

    act(() => { vi.advanceTimersByTime(630); });
    expect(streamText()).toContain("3段");
    expect(container.textContent).toContain("40/100");

    act(() => { vi.advanceTimersByTime(1100); });
    expect(streamText()).toContain("战斗结束");
  });

  it("does not hide a unit's health bar before a later skill segment defeats it", () => {
    const multiHitBattle: CppBattleSimulationResponse = {
      ...battle,
      commands: [
        { ...command("羿开始攻击，", "yi_tri_sword", 2, 0, 100, false), alive: true, playerMaxHp: 100 },
        { ...command("第一击：", "yi_tri_sword", 0, 0, 100, false), alive: true, playerMaxHp: 100 },
        { ...command("10", "yi_tri_damage", 3, 10, 90, false), alive: true, playerMaxHp: 100 },
        { ...command("第二击：", "yi_tri_sword", 0, 0, 90, false), alive: true, playerMaxHp: 100 },
        { ...command("90", "yi_tri_damage", 3, 90, 0, false), alive: false, playerMaxHp: 100 },
        { ...command("beta倒下", "death", 1, 0, 0, true), alive: false, playerMaxHp: 100 },
      ],
    };
    const { container } = render(<BattlePlayback battle={multiHitBattle} watchMode onRestart={() => {}} />);
    const betaUnit = () => Array.from(container.querySelectorAll(".unit-readout")).find((unit) => unit.textContent?.includes("beta"));

    expect(container.querySelector('.hp-meter[aria-label="beta 生命 90/100"]')).not.toBeNull();
    expect(betaUnit()?.classList.contains("is-defeated")).toBe(false);

    act(() => { vi.advanceTimersByTime(630); });
    expect(betaUnit()?.classList.contains("is-defeated")).toBe(true);
  });

  it("uses a clear stolen-skill announcement instead of the misleading chain-cast wording", () => {
    const stolenSkillBattle: CppBattleSimulationResponse = {
      ...battle,
      commands: [
        { ...command("张洋连放", "boss_stolen_cast", 0, 0, 100, false), targetPlayerId: 1 },
        { ...command("三元剑", "boss_stolen_cast", 2, 0, 100, true), targetPlayerId: 1 },
        command("战斗结束", "battle_end", 1, 0, 0, true),
      ],
    };
    const { container } = render(<BattlePlayback battle={stolenSkillBattle} onRestart={() => {}} />);
    const streamText = container.querySelector(".playback-stream")?.textContent ?? "";

    expect(streamText).toContain("张洋释放偷来的三元剑");
    expect(streamText).not.toContain("张洋连放");
  });

  it("keeps the healed hp segment blue and visibly merges it into the health bar", () => {
    const healingBattle: CppBattleSimulationResponse = {
      ...battle,
      initialPlayers: [makePlayer(1, "alpha"), { ...makePlayer(2, "beta"), hp: 40 }],
      finalPlayers: [makePlayer(1, "alpha"), { ...makePlayer(2, "beta"), hp: 60 }],
      commands: [
        { ...command("beta恢复20生命", "heal", 4, 20, 60, true), sourcePlayerId: 1, targetPlayerId: 2 },
        command("战斗结束", "battle_end", 1, 0, 0, true),
      ],
    };
    const { container } = render(<BattlePlayback battle={healingBattle} watchMode onRestart={() => {}} />);
    const healSegment = container.querySelector<HTMLElement>(".hp-heal");

    expect(container.querySelector('.hp-meter[aria-label="beta 生命 60/100"]')).not.toBeNull();
    expect(healSegment).not.toBeNull();
    expect(healSegment?.style.left).toBe("40%");
    expect(healSegment?.style.width).toBe("20%");
    expect(healSegment?.style.getPropertyValue("--hp-animation-duration")).toBe("310ms");

    act(() => { vi.advanceTimersByTime(218); });
    expect(healSegment?.classList.contains("is-erasing")).toBe(true);
  });

  it("renders food hp loss and applies every heal in the same event", () => {
    const gamma = { ...makePlayer(3, "gamma"), hp: 40 };
    const foodBattle: CppBattleSimulationResponse = {
      ...battle,
      initialPlayers: [makePlayer(1, "mili"), makePlayer(2, "beta"), gamma],
      finalPlayers: [makePlayer(1, "mili"), makePlayer(2, "beta"), gamma],
      commands: [
        command("beta生命降至30%", "food_damage", 14, 70, 30, true),
        { ...command("beta恢复20", "food_heal", 4, 20, 50, false), targetPlayerId: 2 },
        { ...command("gamma恢复20", "heal", 4, 20, 60, true), targetPlayerId: 3 },
        command("战斗结束", "battle_end", 1, 0, 0, true),
      ],
    };
    const { container } = render(<BattlePlayback battle={foodBattle} onRestart={() => {}} />);

    expect(container.querySelector('.hp-meter[aria-label="beta 生命 30/100"]')).not.toBeNull();
    const lossMark = container.querySelector(".hp-loss");
    expect(lossMark).not.toBeNull();
    expect(lossMark?.classList.contains("is-erasing")).toBe(false);

    act(() => { vi.advanceTimersByTime(17); });
    expect(lossMark?.classList.contains("is-erasing")).toBe(false);
    act(() => { vi.advanceTimersByTime(1); });
    expect(lossMark?.classList.contains("is-erasing")).toBe(true);

    act(() => { vi.advanceTimersByTime(30); });
    expect(container.querySelector('.hp-meter[aria-label="beta 生命 50/100"]')).not.toBeNull();
    expect(container.querySelector('.hp-meter[aria-label="gamma 生命 60/100"]')).not.toBeNull();
    expect(container.querySelectorAll(".hp-heal")).toHaveLength(2);
  });
});
