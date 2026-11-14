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

    act(() => { vi.advanceTimersByTime(830); });
    expect(streamText()).toContain("2段");
    expect(streamText()).not.toContain("3段");
    expect(container.textContent).toContain("70/100");

    act(() => { vi.advanceTimersByTime(830); });
    expect(streamText()).toContain("3段");
    expect(container.textContent).toContain("40/100");

    act(() => { vi.advanceTimersByTime(1100); });
    expect(streamText()).toContain("战斗结束");
  });
});
