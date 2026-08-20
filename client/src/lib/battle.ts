/**
 * Deterministic battle core.
 * Identity seed: canonical name -> stable profile. Match seed: ordered teams -> simulation events.
 * The same UTF-8 FNV-1a and uint32 operations are intended for the future C++/WASM core.
 */
export type UnitStats = {
  maxHp: number;
  attack: number;
  defense: number;
  speed: number;
  agility: number;
  magic: number;
  resistance: number;
  insight: number;
};

export type BattleUnit = UnitStats & {
  id: string;
  inputName: string;
  name: string;
  identitySeed: number;
  team: number;
  inputIndex: number;
  special: boolean;
  hp: number;
  kills: number;
  defeated: boolean;
};

export type BattleEvent = {
  eventIndex: number;
  actorId: string;
  actorName: string;
  actorTeam: number;
  targetId: string;
  targetName: string;
  targetTeam: number;
  damage: number;
  critical: boolean;
  targetHp: number;
  defeated: boolean;
};

export type TeamResult = { team: number; members: BattleUnit[]; score: number; won: boolean };

export type BattleReplay = {
  input: string;
  seed: number;
  units: BattleUnit[];
  events: BattleEvent[];
  teams: TeamResult[];
  winnerTeam: number;
  rounds: number;
};

const encoder = new TextEncoder();

const SPECIAL_UNITS: Record<string, { displayName: string; stats: UnitStats }> = {
  "AL1S@!": {
    displayName: "天童爱丽丝",
    stats: { maxHp: 800, attack: 90, defense: 90, speed: 70, agility: 85, magic: 120, resistance: 95, insight: 100 },
  },
};

export function canonicalName(name: string) {
  return name.trim().normalize("NFC");
}

function canonicalInput(input: string) {
  return input.replace(/\r\n?/g, "\n");
}

/** 32-bit FNV-1a over UTF-8 bytes; returns an unsigned integer. */
export function fnv1a32(value: string): number {
  let hash = 0x811c9dc5;
  const bytes = encoder.encode(value);
  for (let index = 0; index < bytes.length; index += 1) {
    hash ^= bytes[index];
    hash = Math.imul(hash, 0x01000193) >>> 0;
  }
  return hash >>> 0;
}

/** Name-only identity seed: moving a name never changes this value. */
export function identitySeedForName(name: string): number {
  return fnv1a32(`namerena:v1:identity\0${canonicalName(name)}`) || 0x9e3779b9;
}

function orderedTeamText(teams: string[][]) {
  return teams.map((team) => team.join("\n")).join("\n\n");
}

/** Ordered-match seed: every team and intra-team position participates in this value. */
export function matchSeedForTeams(teams: string[][]): number {
  return fnv1a32(`namerena:v1:match\0${orderedTeamText(teams)}`) || 0x9e3779b9;
}

export function seedFromInput(input: string): number {
  return matchSeedForTeams(parseTeams(input));
}

/** Stateless 32-bit mixer used for independent fields derived from one identity seed. */
function mix32(value: number): number {
  let mixed = value >>> 0;
  mixed ^= mixed >>> 16;
  mixed = Math.imul(mixed, 0x7feb352d) >>> 0;
  mixed ^= mixed >>> 15;
  mixed = Math.imul(mixed, 0x846ca68b) >>> 0;
  mixed ^= mixed >>> 16;
  return mixed >>> 0;
}

function derivedInteger(seed: number, salt: number, min: number, max: number) {
  const mixed = mix32((seed ^ Math.imul(salt, 0x9e3779b9)) >>> 0);
  return min + (mixed % (max - min + 1));
}

/** Stable profile for ordinary names. No match-wide RNG state is consumed here. */
export function statsForName(name: string): UnitStats {
  const seed = identitySeedForName(name);
  return {
    maxHp: derivedInteger(seed, 1, 220, 420),
    attack: derivedInteger(seed, 2, 44, 94),
    defense: derivedInteger(seed, 3, 40, 90),
    speed: derivedInteger(seed, 4, 42, 94),
    agility: derivedInteger(seed, 5, 38, 88),
    magic: derivedInteger(seed, 6, 40, 90),
    resistance: derivedInteger(seed, 7, 40, 90),
    insight: derivedInteger(seed, 8, 40, 88),
  };
}

class XorShift32 {
  constructor(private state: number) {}
  next() {
    let value = this.state || 0x9e3779b9;
    value ^= value << 13;
    value ^= value >>> 17;
    value ^= value << 5;
    this.state = value >>> 0;
    return this.state / 0x1_0000_0000;
  }
  integer(min: number, max: number) {
    return min + Math.floor(this.next() * (max - min + 1));
  }
}

/** Empty lines split teams; all non-empty name lines preserve their original order. */
export function parseTeams(input: string): string[][] {
  const groups = canonicalInput(input).split(/\n[\t ]*\n+/);
  return groups
    .map((group) => group.split("\n").map(canonicalName).filter(Boolean))
    .filter((group) => group.length > 0);
}

function createUnit(inputName: string, team: number, inputIndex: number): BattleUnit {
  const specialDefinition = SPECIAL_UNITS[inputName];
  const stats = specialDefinition ? specialDefinition.stats : statsForName(inputName);
  return {
    id: `u-${inputIndex}`,
    inputName,
    name: specialDefinition ? specialDefinition.displayName : inputName,
    identitySeed: identitySeedForName(inputName),
    team,
    inputIndex,
    special: Boolean(specialDefinition),
    hp: stats.maxHp,
    kills: 0,
    defeated: false,
    ...stats,
  };
}

function livingTeams(units: BattleUnit[]) {
  return new Set(units.filter((unit) => !unit.defeated).map((unit) => unit.team));
}

function actionOrder(units: BattleUnit[]) {
  return units
    .filter((unit) => !unit.defeated)
    .sort((left, right) => right.speed - left.speed || right.agility - left.agility || left.inputIndex - right.inputIndex);
}

function scoreUnit(unit: BattleUnit, won: boolean) {
  const survival = unit.defeated ? 0 : Math.round((unit.hp / unit.maxHp) * 180);
  return (won ? 180 : 40) + survival + unit.kills * 55;
}

/**
 * Simulates from name profiles plus the ordered-match RNG only. It never uses Date or Math.random.
 * Reordering name lines changes matchSeed and may change events, but cannot change any unit profile.
 */
export function simulateBattle(input: string): BattleReplay {
  const teams = parseTeams(input);
  if (teams.length < 2) throw new Error("请至少用一个空行分隔两支队伍。");

  const seed = matchSeedForTeams(teams);
  const random = new XorShift32(seed);
  let inputIndex = 0;
  const units = teams.flatMap((team, teamIndex) => team.map((name) => createUnit(name, teamIndex, inputIndex++)));
  const events: BattleEvent[] = [];
  let rounds = 0;

  while (livingTeams(units).size > 1 && rounds < 120) {
    rounds += 1;
    for (const actor of actionOrder(units)) {
      if (actor.defeated || livingTeams(units).size < 2) continue;
      const enemies = units.filter((candidate) => !candidate.defeated && candidate.team !== actor.team);
      const target = enemies[random.integer(0, enemies.length - 1)];
      const variance = 0.78 + random.next() * 0.44;
      const critical = random.next() < Math.min(0.22, 0.05 + actor.agility / 2000);
      const raw = actor.attack * variance - target.defense * 0.24 + actor.insight * 0.08;
      const damage = Math.max(4, Math.round(raw * (critical ? 1.45 : 1)));
      target.hp = Math.max(0, target.hp - damage);
      target.defeated = target.hp === 0;
      if (target.defeated) actor.kills += 1;
      events.push({
        eventIndex: events.length,
        actorId: actor.id,
        actorName: actor.name,
        actorTeam: actor.team,
        targetId: target.id,
        targetName: target.name,
        targetTeam: target.team,
        damage,
        critical,
        targetHp: target.hp,
        defeated: target.defeated,
      });
    }
  }

  const survivors = Array.from(livingTeams(units));
  const winnerTeam = survivors.length === 1
    ? survivors[0]
    : Array.from(new Set(units.map((unit) => unit.team))).sort((left, right) => {
      const leftHp = units.filter((unit) => unit.team === left).reduce((sum, unit) => sum + unit.hp, 0);
      const rightHp = units.filter((unit) => unit.team === right).reduce((sum, unit) => sum + unit.hp, 0);
      return rightHp - leftHp || left - right;
    })[0];

  return {
    input: orderedTeamText(teams),
    seed,
    units,
    events,
    winnerTeam,
    rounds,
    teams: teams.map((_, team) => {
      const won = team === winnerTeam;
      const members = units.filter((unit) => unit.team === team);
      return { team, members, won, score: members.reduce((sum, unit) => sum + scoreUnit(unit, won), 0) };
    }),
  };
}
