import { type BattleUnit, simulateBattle, statsForName } from "../client/src/lib/battle";

function expect(condition: boolean, message: string) {
  if (!condition) throw new Error(message);
}

function profileOf(unit: BattleUnit) {
  return JSON.stringify({
    inputName: unit.inputName,
    identitySeed: unit.identitySeed,
    maxHp: unit.maxHp,
    attack: unit.attack,
    defense: unit.defense,
    speed: unit.speed,
    agility: unit.agility,
    magic: unit.magic,
    resistance: unit.resistance,
    insight: unit.insight,
  });
}

const orderedInput = "晨星\n静海\n\n赤原\n暮山";
const reorderedInput = "静海\n晨星\n\n赤原\n暮山";
const replayA = simulateBattle(orderedInput);
const replayB = simulateBattle(orderedInput);
const reorderedReplay = simulateBattle(reorderedInput);
const specialReplay = simulateBattle("观察者\n\nAL1S@!");

expect(JSON.stringify(replayA) === JSON.stringify(replayB), "同一输入未生成完全相同的回放。");
for (const name of ["晨星", "静海", "赤原", "暮山"]) {
  const inFirstReplay = replayA.units.find((unit) => unit.inputName === name);
  const inReorderedReplay = reorderedReplay.units.find((unit) => unit.inputName === name);
  expect(Boolean(inFirstReplay && inReorderedReplay), `没有在两局中找到名称 ${name}。`);
  expect(
    profileOf(inFirstReplay!) === profileOf(inReorderedReplay!),
    `名称 ${name} 的基础属性随位置发生变化。`,
  );
}
expect(replayA.seed !== reorderedReplay.seed, "改变输入顺序后对局种子没有变化。");
expect(JSON.stringify(replayA.events) !== JSON.stringify(reorderedReplay.events), "改变输入顺序后事件流没有变化。");
expect(specialReplay.units.some((unit) => unit.name === "天童爱丽丝" && unit.maxHp === 800), "特殊名字未映射为预期单位。");

console.log(JSON.stringify({
  stableMatchSeed: replayA.seed,
  reorderedMatchSeed: reorderedReplay.seed,
  stableProfiles: ["晨星", "静海", "赤原", "暮山"].map((name) => ({ name, stats: statsForName(name) })),
  firstReplayEventCount: replayA.events.length,
  reorderedReplayEventCount: reorderedReplay.events.length,
  specialDisplayName: specialReplay.units.find((unit) => unit.special)?.name,
}, null, 2));
