/**
 * Style reference: 白色背景、圆角蓝青发光框与圆角按钮；输入框保持直角。
 * C++/WASM completes the battle and supplies values plus RenderCommand lines; React only renders those results.
 */
import { memo, useEffect, useMemo, useRef, useState, type CSSProperties } from "react";
import { requestCppBattleSimulation, type CppBattleSimulationResponse, type CppPlayerSnapshot, type CppRenderCommand } from "@/lib/cppSnapshot";

const FREEZE_ICON_URL = "data:image/svg+xml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHZpZXdCb3g9IjAgMCAyNCAyNCI+CjxkZWZzPjxsaW5lYXJHcmFkaWVudCBpZD0iaSIgeDE9IjMiIHkxPSIyIiB4Mj0iMjEiIHkyPSIyMiIgZ3JhZGllbnRVbml0cz0idXNlclNwYWNlT25Vc2UiPjxzdG9wIHN0b3AtY29sb3I9IiNlZmZjZmYiLz48c3RvcCBvZmZzZXQ9Ii40NSIgc3RvcC1jb2xvcj0iIzhmZGNmNSIvPjxzdG9wIG9mZnNldD0iMSIgc3RvcC1jb2xvcj0iIzQxOThkMCIvPjwvbGluZWFyR3JhZGllbnQ+PC9kZWZzPgo8cGF0aCBmaWxsPSJ1cmwoI2kpIiBzdHJva2U9IiM0YmE4ZDgiIHN0cm9rZS13aWR0aD0iMS4yNSIgc3Ryb2tlLWxpbmVqb2luPSJyb3VuZCIgZD0iTTcgNGgxMGw0IDUtMiAxMC03IDItNy0yTDMgOXoiLz4KPHBhdGggZmlsbD0iI2ZmZiIgZmlsbC1vcGFjaXR5PSIuNzIiIGQ9Im03IDUgNCAzLTIgNS00LTR6Ii8+CjxwYXRoIGZpbGw9IiNjOWY0ZmYiIGZpbGwtb3BhY2l0eT0iLjc2IiBkPSJtMTEgOCA2LTMgMyA0LTUgNHoiLz4KPHBhdGggZmlsbD0ibm9uZSIgc3Ryb2tlPSIjZmZmIiBzdHJva2UtbGluZWNhcD0icm91bmQiIHN0cm9rZS13aWR0aD0iMS4yNSIgb3BhY2l0eT0iLjg4IiBkPSJtOCA3IDItMW00IDExIDMtMiIvPgo8L3N2Zz4K";
const IRONWALL_ICON_URL = "data:image/svg+xml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHZpZXdCb3g9IjAgMCAyNCAyNCI+CiAgPGRlZnM+CiAgICA8bGluZWFyR3JhZGllbnQgaWQ9InN0ZWVsIiB4MT0iNSIgeTE9IjMiIHgyPSIxOSIgeTI9IjIxIiBncmFkaWVudFVuaXRzPSJ1c2VyU3BhY2VPblVzZSI+CiAgICAgIDxzdG9wIHN0b3AtY29sb3I9IiNmNGY3ZjgiLz4KICAgICAgPHN0b3Agb2Zmc2V0PSIuMzQiIHN0b3AtY29sb3I9IiNjN2QwZDYiLz4KICAgICAgPHN0b3Agb2Zmc2V0PSIuNyIgc3RvcC1jb2xvcj0iIzdkODkzIi8+CiAgICAgIDxzdG9wIG9mZnNldD0iMSIgc3RvcC1jb2xvcj0iIzRlNTk2MyIvPgogICAgPC9saW5lYXJHcmFkaWVudD4KICA8L2RlZnM+CiAgPHBhdGggZD0iTTEyIDIuMiAyMCA1LjggMTguMjUgMTUuMiAxMiAyMiA1Ljc1IDE1LjIgNCA1LjggMTIgMi4yWiIgZmlsbD0idXJsKCNzdGVlbCkiIHN0cm9rZT0iIzQwNTA1YyIgc3Ryb2tlLXdpZHRoPSIxLjI1IiBzdHJva2UtbGluZWpvaW49InJvdW5kIi8+CiAgPHBhdGggZD0iTTEyIDQuMjV2MTQuNU02LjQ1IDcuMSAxMiA1LjFsNS41NSAyIiBmaWxsPSJub25lIiBzdHJva2U9IiNmOWZiZmMiIHN0cm9rZS1vcGFjaXR5PSIuNzIiIHN0cm9rZS13aWR0aD0iMSIgc3Ryb2tlLWxpbmVjYXA9InJvdW5kIi8+CiAgPGNpcmNsZSBjeD0iMTIiIGN5PSI5LjE1IiByPSIxLjEiIGZpbGw9IiM1OTY2NzEiIHN0cm9rZT0iI2VlZjJmNCIgc3Ryb2tlLXdpZHRoPSIuNSIvPgogIDxjaXJjbGUgY3g9IjguMSIgY3k9IjcuMyIgcj0iLjYiIGZpbGw9IiNlZWYyZjQiIGZpbGwtb3BhY2l0eT0iLjg4Ii8+CiAgPGNpcmNsZSBjeD0iMTUuOSIgY3k9IjcuMyIgcj0iLjYiIGZpbGw9IiNlZWYyZjQiIGZpbGwtb3BhY2l0eT0iLjg4Ii8+Cjwvc3ZnPgo=";
const LIFESTEAL_ICON_URL = "data:image/svg+xml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHZpZXdCb3g9IjAgMCAyNCAyNCI+PHBhdGggZD0iTTEyIDJDOSA3IDUgMTAgNSAxNGE3IDcgMCAwIDAgMTQgMGMwLTQtNC03LTctMTJaIiBmaWxsPSIjYzIxZjNhIiBzdHJva2U9IiM2YjA4MWEiIHN0cm9rZS13aWR0aD0iMS4yIi8+PHBhdGggZD0iTTkgOGMtMS4yIDEuOC0yIDMuMi0yIDQuOCIgZmlsbD0ibm9uZSIgc3Ryb2tlPSIjZmZkNmRkIiBzdHJva2Utd2lkdGg9IjEuMyIgc3Ryb2tlLWxpbmVjYXA9InJvdW5kIi8+PC9zdmc+Cg==";
const FIRE_ICON_URL = "data:image/svg+xml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHZpZXdCb3g9IjAgMCAyNCAyNCI+PHBhdGggZD0iTTEzIDJjMSA1LTIgNi0yIDkgMCAxLjcgMS4yIDIuOCAyLjcgMi44IDEuOSAwIDMuMS0xLjcgMi40LTQuNCAyLjEgMS42IDMuNiA0IDMuNiA2LjhBNy43IDcuNyAwIDAgMSAxMiAyNGE3LjcgNy43IDAgMCAxLTcuNy03LjhjMC0zLjYgMi02LjcgNS4xLTguNy0uNCAzLjIuOCA0LjggMi4yIDQuOCAxLjcgMCAyLjUtMi4xIDEuNC00LjZaIiBmaWxsPSIjZTc2ODI4IiBzdHJva2U9IiNhODM5MTkiIHN0cm9rZS13aWR0aD0iMSIvPjxwYXRoIGQ9Ik0xMi4yIDEzLjFjMS41IDItMS4zIDMuMS0xLjMgNS4zIDAgMS4xLjYgMi4xIDEuNyAyLjggMi41LTEuNCAyLjctNC44LS40LTguMVoiIGZpbGw9IiNmZmQxNTgiLz48L3N2Zz4K";
const SPEED_UP_ICON_URL = "data:image/svg+xml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHZpZXdCb3g9IjAgMCAyNCAyNCI+PHBhdGggZD0iTTMgMjFDMTAgMTkgMTcgMTIgMjAgMyAxMSA1IDQgMTIgMyAyMVoiIGZpbGw9IiM3MGQ4ZTkiIHN0cm9rZT0iIzI4N2M5YSIvPjxjaXJjbGUgY3g9IjE4IiBjeT0iMTgiIHI9IjQiIGZpbGw9IiMzN2E2NmYiLz48cGF0aCBkPSJNMTggMTQuNXY3bS0zLjUtMy41aDciIHN0cm9rZT0iI2ZmZiIgc3Ryb2tlLXdpZHRoPSIxLjUiLz48L3N2Zz4K";
const SPEED_DOWN_ICON_URL = "data:image/svg+xml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHZpZXdCb3g9IjAgMCAyNCAyNCI+PHBhdGggZD0iTTMgMjFDMTAgMTkgMTcgMTIgMjAgMyAxMSA1IDQgMTIgMyAyMVoiIGZpbGw9IiNhOWM2ZDYiIHN0cm9rZT0iIzRmNzU4OSIvPjxjaXJjbGUgY3g9IjE4IiBjeT0iMTgiIHI9IjQiIGZpbGw9IiNkMTViNTQiLz48cGF0aCBkPSJNMTQuNSAxOGg3IiBzdHJva2U9IiNmZmYiIHN0cm9rZS13aWR0aD0iMS41Ii8+PC9zdmc+Cg==";
const MAGIC_VULN_ICON_URL = "data:image/svg+xml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHZpZXdCb3g9IjAgMCAyNCAyNCI+PHBhdGggZD0iTTEyIDJsNyA3LTcgMTMtNy0xM3oiIGZpbGw9IiNhYjY4ZDAiIHN0cm9rZT0iIzYyMzI5MCIgc3Ryb2tlLXdpZHRoPSIxLjIiLz48cGF0aCBkPSJtMTIgNiAyLjcgNS0yLjcgNy0yLjctN3oiIGZpbGw9IiNmN2U4ZmYiIGZpbGwtb3BhY2l0eT0iLjg1Ii8+PC9zdmc+";

const POISON_ICON_URL = "data:image/svg+xml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHZpZXdCb3g9IjAgMCAyNCI+PHBhdGggZD0iTTkgMmg2djNsLTEgMiA1IDlhNCA0IDAgMDEtNCA2SDhhNCA0IDAgMDEtNC02bDUtOVY1SDl6IiBmaWxsPSIjMTk3NTQ1IiBzdHJva2U9IiMwOTNkMjUiLz48Y2lyY2xlIGN4PSIxNiIgY3k9IjEwIiByPSIxIiBmaWxsPSIjZGZmZmYwIi8+PC9zdmc+Cg==";

type DamageVisual = {
  animationId: number;
  targetPlayerId: number;
  beforeHp: number;
  afterHp: number;
  transitionMs: number;
  phase: "marked" | "erasing";
};

type HealVisual = {
  animationId: number;
  targetPlayerId: number;
  beforeHp: number;
  afterHp: number;
  transitionMs: number;
  phase: "marked" | "erasing";
};

const PARRY_ICON_URL = "data:image/svg+xml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHZpZXdCb3g9IjAgMCAyNCAyNCI+PHBhdGggZD0iTTMgM2w4IDgtNSA4TTIxIDNsLTggOCA1IDgiIGZpbGw9Im5vbmUiIHN0cm9rZT0iIzY4NzQ3ZCIgc3Ryb2tlLXdpZHRoPSIzIiBzdHJva2UtbGluZWNhcD0icm91bmQiLz48cGF0aCBkPSJNNiAxOGwzLTJNMTggMThsLTMtMiIgc3Ryb2tlPSIjMzk0MzRhIiBzdHJva2Utd2lkdGg9IjIuNSIvPjxwYXRoIGQ9Ik0xMiA4VjVNMTAgOSA4IDdNMTQgOWwyLTIiIHN0cm9rZT0iI2ZmZiIgc3Ryb2tlLXdpZHRoPSIxLjUiIHN0cm9rZS1saW5lY2FwPSJyb3VuZCIvPjwvc3ZnPgo=";

const EMPTY_DAMAGE_VISUALS: DamageVisual[] = [];
const EMPTY_HEAL_VISUALS: HealVisual[] = [];

type ManaVisual = {
  targetPlayerId: number;
  beforeMana: number;
  afterMana: number;
  phase: "marked" | "erasing";
};

type StatusBadge = {
  key: "freeze" | "ironwall" | "speed-up" | "speed-down" | "burn" | "poison" | "parry" | "magic-vuln";
  strength: number;
  layers: number;
  priority: number;
  title: string;
  icon: string;
};

function getStatusBadges(player: CppPlayerSnapshot) {
  const statuses: StatusBadge[] = [];
  const freezeLayers = player.freezeLayers ?? 0;
  const ironwallLayers = player.ironwallLayers ?? 0;
  const speedUpLayers = player.speedUpLayers ?? 0;
  const speedDownLayers = player.speedDownLayers ?? 0;
  const burnLayers = player.burnLayers ?? 0;
  const poisonLayers = player.poisonLayers ?? 0;
  const parryLayers = player.parryLayers ?? 0;
  const magicVulnerability = player.magicVulnerability ?? 0;

  if (magicVulnerability > 0) statuses.push({ key: "magic-vuln", strength: magicVulnerability, layers: Number.MAX_SAFE_INTEGER, priority: -1, title: `魔法易损 ${magicVulnerability}/∞`, icon: MAGIC_VULN_ICON_URL });
  if (freezeLayers > 0) statuses.push({ key: "freeze", strength: player.freezeStrength ?? 0, layers: freezeLayers, priority: 0, title: `冻结 ${player.freezeStrength ?? 0}/${freezeLayers}`, icon: FREEZE_ICON_URL });
  if (burnLayers > 0) statuses.push({ key: "burn", strength: player.burnStrength ?? 0, layers: burnLayers, priority: 1, title: `烧伤 ${player.burnStrength ?? 0}/${burnLayers}`, icon: FIRE_ICON_URL });
  if (poisonLayers > 0) statuses.push({ key: "poison", strength: player.poisonStrength ?? 0, layers: poisonLayers, priority: 2, title: `中毒 ${player.poisonStrength ?? 0}/${poisonLayers}`, icon: POISON_ICON_URL });
  if (parryLayers > 0) statuses.push({ key: "parry", strength: 1, layers: parryLayers, priority: 3, title: `招架 1/${parryLayers}`, icon: PARRY_ICON_URL });
  if (speedDownLayers > 0) statuses.push({ key: "speed-down", strength: player.speedDownStrength ?? 0, layers: speedDownLayers, priority: 4, title: `速度削减 ${player.speedDownStrength ?? 0}/${speedDownLayers}`, icon: SPEED_DOWN_ICON_URL });
  if (ironwallLayers > 0) statuses.push({ key: "ironwall", strength: player.ironwallStrength ?? 0, layers: ironwallLayers, priority: 5, title: `铁壁 ${player.ironwallStrength ?? 0}/${ironwallLayers}`, icon: IRONWALL_ICON_URL });
  if (speedUpLayers > 0) statuses.push({ key: "speed-up", strength: player.speedUpStrength ?? 0, layers: speedUpLayers, priority: 6, title: `速度强化 ${player.speedUpStrength ?? 0}/${speedUpLayers}`, icon: SPEED_UP_ICON_URL });

  return statuses.sort((a, b) => b.layers - a.layers || a.priority - b.priority).slice(0, 3);
}

const UnitReadout = memo(function UnitReadout({ player, damageVisuals, healVisuals, manaVisual, compactMode }: { player: CppPlayerSnapshot; damageVisuals: DamageVisual[]; healVisuals: HealVisual[]; manaVisual: ManaVisual | null; compactMode: boolean }) {
  const hpPercent = Math.max(0, Math.min(100, Math.round((player.hp / player.maxHp) * 100)));
  const playerDamageVisuals = damageVisuals.filter((visual) => visual.targetPlayerId === player.id);
  const playerHealVisuals = healVisuals.filter((visual) => visual.targetPlayerId === player.id);
  const isDamageTarget = playerDamageVisuals.length > 0;
  const isHealTarget = playerHealVisuals.length > 0;
  const hpTransitionMs = playerDamageVisuals[0]?.transitionMs ?? playerHealVisuals[0]?.transitionMs ?? 100;
  const manaPercent = player.maxMana > 0 ? Math.max(0, Math.min(100, Math.round((player.mana / player.maxMana) * 100))) : 0;
  const isManaTarget = manaVisual?.targetPlayerId === player.id;
  const manaLossStartPercent = isManaTarget && player.maxMana > 0 ? Math.max(0, Math.min(100, (manaVisual.beforeMana / player.maxMana) * 100)) : 0;
  const manaLossEndPercent = isManaTarget && player.maxMana > 0 ? Math.max(0, Math.min(100, (manaVisual.afterMana / player.maxMana) * 100)) : 0;
  const manaLossWidthPercent = Math.max(0, manaLossStartPercent - manaLossEndPercent);
  const statuses = getStatusBadges(player);
  const isPoisoned = (player.poisonLayers ?? 0) > 0;

  return (
    <article className={`unit-readout ${compactMode ? "is-compact" : ""} ${player.isFamiliar ? "is-familiar" : ""} ${player.alive === false ? "is-defeated" : ""} ${player.isRevived ? "is-revived" : ""} ${isDamageTarget ? "is-taking-damage" : ""} ${isHealTarget ? "is-being-healed" : ""} ${isManaTarget ? "is-using-mana" : ""} ${isPoisoned ? "is-poisoned" : ""}`}>
      <strong title={`${player.name} #队伍${player.teamId}`}><span className="unit-name">{player.name}</span>{compactMode === false && statuses.length > 0 && <span className="unit-statuses" aria-label={`${player.name} 当前状态`}>{statuses.map((status) => <span className={`unit-status unit-status-${status.key}`} title={status.title} key={status.key}><img src={status.icon} alt="" aria-hidden="true" /><b>{status.key === "magic-vuln" ? `${status.strength}/∞` : `${status.strength}/${status.layers}`}</b></span>)}</span>}<em>#队伍{player.teamId}</em></strong>
      <div className="meter-stack">
        <div className="hp-meter" aria-label={`${player.name} 生命 ${player.hp}/${player.maxHp}`}>
          <span className="hp-fill" style={{ transform: `scaleX(${hpPercent / 100})`, "--hp-animation-duration": `${hpTransitionMs}ms`, "--revived-hp-scale": hpPercent / 100 } as CSSProperties} />
          {playerDamageVisuals.map((visual) => {
            const lossStartPercent = Math.max(0, Math.min(100, (visual.beforeHp / player.maxHp) * 100));
            const lossEndPercent = Math.max(0, Math.min(100, (visual.afterHp / player.maxHp) * 100));
            const lossWidthPercent = Math.max(0, lossStartPercent - lossEndPercent);
            return lossWidthPercent > 0 ? <span className={`hp-loss ${visual.phase === "erasing" ? "is-erasing" : ""}`} key={visual.animationId} style={{ left: `${lossEndPercent}%`, width: `${lossWidthPercent}%`, "--hp-animation-duration": `${visual.transitionMs}ms` } as CSSProperties} /> : null;
          })}
          {playerHealVisuals.map((visual) => {
            const healStartPercent = Math.max(0, Math.min(100, (visual.beforeHp / player.maxHp) * 100));
            const healEndPercent = Math.max(0, Math.min(100, (visual.afterHp / player.maxHp) * 100));
            const healWidthPercent = Math.max(0, healEndPercent - healStartPercent);
            return healWidthPercent > 0 ? <span className={`hp-heal ${visual.phase === "erasing" ? "is-erasing" : ""}`} key={visual.animationId} style={{ left: `${healStartPercent}%`, width: `${healWidthPercent}%`, "--hp-animation-duration": `${visual.transitionMs}ms` } as CSSProperties} /> : null;
          })}
        </div>
        {compactMode === false && <div className="mana-meter" aria-label={`${player.name} 魔力 ${player.mana}/${player.maxMana}`}>
          <span className="mana-fill" style={{ transform: `scaleX(${manaPercent / 100})` }} />
          {isManaTarget && manaLossWidthPercent > 0 && <span className={`mana-loss ${manaVisual.phase === "erasing" ? "is-erasing" : ""}`} style={{ left: `${manaLossEndPercent}%`, width: `${manaLossWidthPercent}%` }} />}
        </div>}
      </div>
      <div className="hp-values"><span>HP</span><span>{player.hp}/{player.maxHp}</span></div>
    </article>
  );
});

function AttributeStrip({ players }: { players: CppPlayerSnapshot[] }) {
  const namesOnly = players.length > 10;

  return (
    <section className="attribute-strip" aria-label="参战者基础属性">
      <header><strong>基础属性</strong><span>{players.length} 人</span></header>
      <div className={namesOnly ? "attribute-strip-list names-only" : "attribute-strip-list"}>
        {players.map((player) => namesOnly ? (
          <p key={player.id}>{player.name}</p>
        ) : (
          <p key={player.id}><strong>{player.name}</strong><span>命 {player.maxHp}　攻 {player.physicalAttack}　防 {player.physicalDefense}　速 {player.speed}　魔攻 {player.magicAttack}　魔防 {player.magicDefense}　智 {player.wisdom}　回魔 {player.manaRecovery}</span></p>
        ))}
      </div>
    </section>
  );
}

function groupCommandsIntoLines(commands: CppRenderCommand[]) {
  const lines: CppRenderCommand[][] = [];
  let currentLine: CppRenderCommand[] = [];

  for (const command of commands) {
    currentLine.push(command);
    if (command.newlineAfter) {
      lines.push(currentLine);
      currentLine = [];
    }
  }

  if (currentLine.length > 0) lines.push(currentLine);
  return lines;
}

function toneClass(renderTone: number) {
  const toneMap: Record<number, string> = { 1: "tone-system", 2: "tone-skill", 3: "tone-damage", 4: "tone-heal", 5: "tone-status", 6: "tone-warning", 7: "tone-thunder", 8: "tone-freeze", 9: "tone-stack", 10: "tone-ironwall", 11: "tone-earthquake", 12: "tone-rage", 13: "tone-fire", 14: "tone-poison", 15: "tone-gold", 16: "tone-life-wheel", 17: "tone-summon" };
  return toneMap[renderTone] ?? "tone-normal";
}

export function BattlePlayback({ battle, onRestart, compactMode = false }: { battle: CppBattleSimulationResponse; onRestart: () => void; compactMode?: boolean }) {
  const events = useMemo(() => groupCommandsIntoLines(battle.commands), [battle.commands]);
  const [eventIndex, setEventIndex] = useState(0);
  const [segmentIndex, setSegmentIndex] = useState(0);
  const [displayPlayers, setDisplayPlayers] = useState<CppPlayerSnapshot[]>(() => battle.initialPlayers.map((player) => ({ ...player, alive: true })));
  const [damageVisuals, setDamageVisuals] = useState<DamageVisual[]>([]);
  const [healVisuals, setHealVisuals] = useState<HealVisual[]>([]);
  const [manaVisual, setManaVisual] = useState<ManaVisual | null>(null);
  const [copyState, setCopyState] = useState<"idle" | "copied" | "failed">("idle");
  const displayPlayersRef = useRef(displayPlayers);
  const playbackStreamRef = useRef<HTMLDivElement | null>(null);
  const copyResetRef = useRef<number | null>(null);
  const damageAnimationIdRef = useRef(0);
  const healAnimationIdRef = useRef(0);

  const copyRoster = async () => {
    try {
      if (navigator.clipboard?.writeText) {
        await navigator.clipboard.writeText(battle.rawText);
      } else {
        const fallback = document.createElement("textarea");
        fallback.value = battle.rawText;
        fallback.setAttribute("readonly", "");
        fallback.style.position = "fixed";
        fallback.style.opacity = "0";
        document.body.appendChild(fallback);
        fallback.select();
        const copied = document.execCommand("copy");
        fallback.remove();
        if (!copied) throw new Error("Clipboard copy failed");
      }
      setCopyState("copied");
    } catch {
      setCopyState("failed");
    }

    if (copyResetRef.current !== null) window.clearTimeout(copyResetRef.current);
    copyResetRef.current = window.setTimeout(() => setCopyState("idle"), 1800);
  };

  useEffect(() => () => {
    if (copyResetRef.current !== null) window.clearTimeout(copyResetRef.current);
  }, []);

  useEffect(() => {
    const resetPlayers = battle.initialPlayers.map((player) => ({ ...player, alive: true }));
    setEventIndex(0);
    setSegmentIndex(0);
    displayPlayersRef.current = resetPlayers;
    setDisplayPlayers(resetPlayers);
    setDamageVisuals([]);
    setHealVisuals([]);
    setManaVisual(null);
  }, [battle]);

  useEffect(() => {
    if (eventIndex >= events.length) return;

    const currentEvent = events[eventIndex];
    const isLongBattle = battle.longBattle !== false;
    const hasSegmentPause = isLongBattle === false && currentEvent.length > 1;
    const isFinalSegment = hasSegmentPause === false || segmentIndex >= currentEvent.length - 1;
    const statusCommands = new Map<number, CppRenderCommand>();
    for (const command of currentEvent) {
      const hasStatusSnapshot = command.frontEndAnimation === "status_sync" || [command.freezeStrength, command.freezeLayers, command.ironwallStrength, command.ironwallLayers, command.speedUpStrength, command.speedUpLayers, command.speedDownStrength, command.speedDownLayers, command.burnStrength, command.burnLayers, command.poisonStrength, command.poisonLayers, command.parryLayers].some((value) => value !== undefined);
      if (command.targetPlayerId !== 0 && hasStatusSnapshot) statusCommands.set(command.targetPlayerId, command);
    }

    if (segmentIndex === 0 && statusCommands.size > 0) {
      const nextPlayers = displayPlayersRef.current.map((player) => {
        const statusCommand = statusCommands.get(player.id);
        if (!statusCommand) return player;
        return {
          ...player,
          freezeStrength: statusCommand.freezeStrength ?? player.freezeStrength,
          freezeLayers: statusCommand.freezeLayers ?? player.freezeLayers,
          ironwallStrength: statusCommand.ironwallStrength ?? player.ironwallStrength,
          ironwallLayers: statusCommand.ironwallLayers ?? player.ironwallLayers,
          speedUpStrength: statusCommand.speedUpStrength ?? player.speedUpStrength,
          speedUpLayers: statusCommand.speedUpLayers ?? player.speedUpLayers,
          speedDownStrength: statusCommand.speedDownStrength ?? player.speedDownStrength,
          speedDownLayers: statusCommand.speedDownLayers ?? player.speedDownLayers,
          burnStrength: statusCommand.burnStrength ?? player.burnStrength,
          burnLayers: statusCommand.burnLayers ?? player.burnLayers,
          poisonStrength: statusCommand.poisonStrength ?? player.poisonStrength,
          poisonLayers: statusCommand.poisonLayers ?? player.poisonLayers,
          parryLayers: statusCommand.parryLayers ?? player.parryLayers,
          alive: statusCommand.alive ?? player.alive,
          isRevived: statusCommand.frontEndAnimation === "revive_heal" ? true : statusCommand.alive === false ? false : player.isRevived,
        };
      });
      displayPlayersRef.current = nextPlayers;
      setDisplayPlayers(nextPlayers);
    }

    if (isFinalSegment === false) {
      const segmentTimer = window.setTimeout(() => setSegmentIndex((index) => index + 1), 60);
      return () => window.clearTimeout(segmentTimer);
    }

    const damageCommands = currentEvent.filter((command) => command.renderTone === 3 && (command.frontEndAnimation === "normal_attack_damage" || command.frontEndAnimation === "stab_damage" || command.frontEndAnimation === "critical_strike_damage" || command.frontEndAnimation === "lifesteal_damage" || command.frontEndAnimation === "parry_counter_damage" || command.frontEndAnimation === "counter_damage" || command.frontEndAnimation === "fireball_damage" || command.frontEndAnimation === "plague_damage" || command.frontEndAnimation === "burn_damage" || command.frontEndAnimation === "poison_damage" || command.frontEndAnimation === "status_damage" || command.frontEndAnimation === "thunder_damage" || command.frontEndAnimation === "thunder_damage_last" || command.frontEndAnimation === "earthquake_damage" || command.frontEndAnimation === "ice_damage" || command.frontEndAnimation === "life_wheel_damage") && command.targetPlayerId !== 0);
    const healCommand = currentEvent.find((command) => command.frontEndAnimation === "heal" || command.frontEndAnimation === "lifesteal_heal" || command.frontEndAnimation === "status_heal" || command.frontEndAnimation === "revive_heal" || command.frontEndAnimation === "life_wheel_heal" || command.frontEndAnimation === "devour_heal");
    const reviveCommand = currentEvent.find((command) => command.frontEndAnimation === "revive_heal");
    const deathCommand = currentEvent.find((command) => command.frontEndAnimation === "death" || command.frontEndAnimation === "familiar_depart");
    const familiarSpawnCommand = currentEvent.find((command) => command.frontEndAnimation === "summon_spawn" && command.targetPlayerId !== 0);
    const manaGainCommand = currentEvent.find((command) => command.frontEndAnimation === "mana_gain" && command.sourcePlayerId !== 0);
    const manaCostCommand = currentEvent.find((command) => command.frontEndAnimation === "mana_cost" && command.sourcePlayerId !== 0);
    if (familiarSpawnCommand) {
      const finalFamiliar = battle.finalPlayers.find((player) => player.id === familiarSpawnCommand.targetPlayerId && player.isFamiliar);
      if (finalFamiliar) {
        const currentPlayers = displayPlayersRef.current;
        if (currentPlayers.some((player) => player.id === finalFamiliar.id) === false) {
          const spawnedFamiliar: CppPlayerSnapshot = {
            ...finalFamiliar,
            hp: familiarSpawnCommand.valueAfter,
            mana: 0,
            freezeStrength: 0,
            freezeLayers: 0,
            ironwallStrength: 0,
            ironwallLayers: 0,
            speedUpStrength: 0,
            speedUpLayers: 0,
            speedDownStrength: 0,
            speedDownLayers: 0,
            burnStrength: 0,
            burnLayers: 0,
            poisonStrength: 0,
            poisonLayers: 0,
            parryLayers: 0,
            alive: true,
          };
          const nextPlayers = [...currentPlayers, spawnedFamiliar];
          displayPlayersRef.current = nextPlayers;
          setDisplayPlayers(nextPlayers);
        }
      }
    }

    if (manaCostCommand) {
      const target = displayPlayersRef.current.find((player) => player.id === manaCostCommand.sourcePlayerId);

      if (target) {
        const beforeMana = Math.min(target.maxMana, manaCostCommand.valueAfter + manaCostCommand.value);
        const nextMarkedPlayers = displayPlayersRef.current.map((player) => player.id === target.id ? { ...player, mana: beforeMana } : player);
        displayPlayersRef.current = nextMarkedPlayers;
        setDisplayPlayers(nextMarkedPlayers);
        setManaVisual({ targetPlayerId: target.id, beforeMana, afterMana: manaCostCommand.valueAfter, phase: "marked" });
        const eraseTimer = window.setTimeout(() => {
          setManaVisual((visual) => visual ? { ...visual, phase: "erasing" } : null);
          setDisplayPlayers((players) => {
            const nextPlayers = players.map((player) => player.id === target.id ? { ...player, mana: manaCostCommand.valueAfter } : player);
            displayPlayersRef.current = nextPlayers;
            return nextPlayers;
          });
        }, 8);
        const nextTimer = window.setTimeout(() => {
          setManaVisual(null);
          setSegmentIndex(0);
          setEventIndex((index) => index + 1);
        }, 80);
        return () => { window.clearTimeout(eraseTimer); window.clearTimeout(nextTimer); };
      }
    }

    if (manaGainCommand) {
      setDisplayPlayers((players) => {
        const nextPlayers = players.map((player) => player.id === manaGainCommand.sourcePlayerId ? { ...player, mana: manaGainCommand.valueAfter } : player);
        displayPlayersRef.current = nextPlayers;
        return nextPlayers;
      });
    }

    const isBlankLine = currentEvent.every((command) => command.text.length === 0);
    const isNextFinalThunderEvent = events[eventIndex + 1]?.some((command) => command.frontEndAnimation === "thunder_damage_last") ?? false;
    const isEarthquakeDamage = currentEvent.some((command) => command.frontEndAnimation === "earthquake_damage");
    const eventDelay = isBlankLine ? (isLongBattle ? 16 : 32) : isNextFinalThunderEvent ? (isLongBattle ? 110 : 180) : isEarthquakeDamage ? (isLongBattle ? 42 : 82) : (isLongBattle ? 48 : 96);
    const lifeEraseDelay = Math.max(8, Math.floor(eventDelay * 0.1));
    const lifeTransitionMs = Math.max(18, eventDelay - lifeEraseDelay);
    const timers: number[] = [];

    if (damageCommands.length > 0) {
      const nextVisuals = damageCommands.flatMap((damageCommand) => {
        const target = displayPlayersRef.current.find((player) => player.id === damageCommand.targetPlayerId);
        if (!target) return [];
        return [{ animationId: ++damageAnimationIdRef.current, targetPlayerId: target.id, beforeHp: Math.min(target.maxHp, damageCommand.valueAfter + damageCommand.value), afterHp: damageCommand.valueAfter, transitionMs: lifeTransitionMs, phase: "marked" as const }];
      });

      if (nextVisuals.length > 0) {
        setDisplayPlayers((players) => {
          const nextPlayers = players.map((player) => {
            const visual = nextVisuals.filter((item) => item.targetPlayerId === player.id).at(-1);
            return visual ? { ...player, hp: visual.afterHp } : player;
          });
          displayPlayersRef.current = nextPlayers;
          return nextPlayers;
        });
        setDamageVisuals(nextVisuals);
        timers.push(window.setTimeout(() => setDamageVisuals((visuals) => visuals.map((visual) => ({ ...visual, phase: "erasing" }))), lifeEraseDelay));
      }
    }

    if (healCommand && healCommand.targetPlayerId !== 0) {
      const target = displayPlayersRef.current.find((player) => player.id === healCommand.targetPlayerId);

      if (target) {
        const animationId = ++healAnimationIdRef.current;
        const beforeHp = reviveCommand ? 0 : Math.max(0, healCommand.valueAfter - healCommand.value);

        setDisplayPlayers((players) => {
          const nextPlayers = players.map((player) => player.id === target.id ? {
            ...player,
            hp: healCommand.valueAfter,
            alive: reviveCommand ? true : player.alive,
            isRevived: reviveCommand ? true : player.isRevived,
            freezeStrength: reviveCommand ? 0 : player.freezeStrength,
            freezeLayers: reviveCommand ? 0 : player.freezeLayers,
            speedDownStrength: reviveCommand ? 0 : player.speedDownStrength,
            speedDownLayers: reviveCommand ? 0 : player.speedDownLayers,
            burnStrength: reviveCommand ? 0 : player.burnStrength,
            burnLayers: reviveCommand ? 0 : player.burnLayers,
            poisonStrength: reviveCommand ? 0 : player.poisonStrength,
            poisonLayers: reviveCommand ? 0 : player.poisonLayers,
          } : player);
          displayPlayersRef.current = nextPlayers;
          return nextPlayers;
        });
        if (reviveCommand) setDamageVisuals((visuals) => visuals.filter((visual) => visual.targetPlayerId !== target.id));
        setHealVisuals([{ animationId, targetPlayerId: target.id, beforeHp, afterHp: healCommand.valueAfter, transitionMs: lifeTransitionMs, phase: "marked" }]);
        timers.push(window.setTimeout(() => setHealVisuals((visuals) => visuals.map((visual) => visual.animationId === animationId ? { ...visual, phase: "erasing" } : visual)), lifeEraseDelay));
      }
    }

    if (deathCommand && deathCommand.targetPlayerId !== 0) {
      setDisplayPlayers((players) => {
        const nextPlayers = players.map((player) => player.id === deathCommand.targetPlayerId ? { ...player, alive: false, isRevived: false } : player);
        displayPlayersRef.current = nextPlayers;
        return nextPlayers;
      });
    }

    const nextTimer = window.setTimeout(() => {
      setDamageVisuals([]);
      setHealVisuals([]);
      setSegmentIndex(0);
      setEventIndex((index) => index + 1);
    }, eventDelay);
    return () => {
      for (const timer of timers) window.clearTimeout(timer);
      window.clearTimeout(nextTimer);
    };
  }, [battle, eventIndex, events, segmentIndex]);

  useEffect(() => {
    const stream = playbackStreamRef.current;
    if (!stream) return;
    if (stream.scrollHeight <= stream.clientHeight) return;

    const nextTop = stream.scrollHeight - stream.clientHeight;
    if (stream.scrollTop >= nextTop) return;
    if (typeof stream.scrollTo === "function") stream.scrollTo({ top: nextTop, behavior: "smooth" });
    else stream.scrollTop = nextTop;
  }, [eventIndex]);

  const renderedEvents = events.slice(0, Math.min(eventIndex + 1, events.length));
  const teamGroups = useMemo(() => {
    const groups = new Map<number, CppPlayerSnapshot[]>();
    for (const player of displayPlayers) {
      const members = groups.get(player.teamId) ?? [];
      members.push(player);
      groups.set(player.teamId, members);
    }
    const familiarOwnerIds = new Set(battle.finalPlayers.filter((player) => player.isFamiliar && player.ownerPlayerId).map((player) => player.ownerPlayerId));
    return Array.from(groups.entries()).map(([teamId, members]) => ({
      teamId,
      owners: members.filter((player) => player.isFamiliar !== true).map((owner) => ({
        owner,
        familiars: members.filter((player) => player.isFamiliar && player.ownerPlayerId === owner.id),
        reservesFamiliarSlot: familiarOwnerIds.has(owner.id),
      })),
    }));
  }, [battle.finalPlayers, displayPlayers]);
  const damageVisualsByPlayer = useMemo(() => {
    const grouped = new Map<number, DamageVisual[]>();
    for (const visual of damageVisuals) grouped.set(visual.targetPlayerId, [...(grouped.get(visual.targetPlayerId) ?? []), visual]);
    return grouped;
  }, [damageVisuals]);
  const healVisualsByPlayer = useMemo(() => {
    const grouped = new Map<number, HealVisual[]>();
    for (const visual of healVisuals) grouped.set(visual.targetPlayerId, [...(grouped.get(visual.targetPlayerId) ?? []), visual]);
    return grouped;
  }, [healVisuals]);

  return (
    <main className="arena-screen" aria-label="名字竞技场战斗界面">
      <section className="arena-frame">
        <AttributeStrip players={battle.initialPlayers} />
        <aside className={`unit-panel ${compactMode ? "is-compact" : ""}`} aria-label="参战单位生命栏">
          <header className="panel-title">单位</header>
          <div className={`unit-list ${compactMode ? "is-compact" : ""}`}>
            {teamGroups.map(({ teamId, owners }) => <section className="team-unit-group" key={teamId} aria-label={`队伍 ${teamId}`}>{owners.map(({ owner, familiars, reservesFamiliarSlot }) => <div className={`unit-owner-slot ${reservesFamiliarSlot ? "has-reserved-familiar" : ""}`} key={owner.id}><UnitReadout key={`${owner.id}-${owner.alive ? "alive" : "dead"}`} player={owner} damageVisuals={damageVisualsByPlayer.get(owner.id) ?? EMPTY_DAMAGE_VISUALS} healVisuals={healVisualsByPlayer.get(owner.id) ?? EMPTY_HEAL_VISUALS} manaVisual={manaVisual?.targetPlayerId === owner.id ? manaVisual : null} compactMode={compactMode} />{familiars.map((familiar) => <UnitReadout key={`${familiar.id}-${familiar.alive ? "alive" : "dead"}`} player={familiar} damageVisuals={damageVisualsByPlayer.get(familiar.id) ?? EMPTY_DAMAGE_VISUALS} healVisuals={healVisualsByPlayer.get(familiar.id) ?? EMPTY_HEAL_VISUALS} manaVisual={manaVisual?.targetPlayerId === familiar.id ? manaVisual : null} compactMode={compactMode} />)}</div>)}</section>)}
          </div>
        </aside>
        <section className="playback-panel" aria-label="战斗播放区">
          <header className="playback-header">
            <h1>战斗</h1>
            <div className="playback-header-actions">
              <span>{Math.min(eventIndex + 1, events.length)}/{events.length}</span>
              <button className={`restart-button copy-roster-button ${copyState !== "idle" ? `is-${copyState}` : ""}`} type="button" onClick={copyRoster} aria-live="polite">
                {copyState === "copied" ? "名单已复制" : copyState === "failed" ? "复制失败" : "复制上局名单"}
              </button>
              <button className="restart-button" type="button" onClick={onRestart}>重新开始</button>
            </div>
          </header>
          <div className="playback-stream" ref={playbackStreamRef} aria-live="polite">
            {renderedEvents.map((event, index) => {
              const shownCommands = battle.longBattle === false && index === eventIndex && event.length > 1 ? event.slice(0, segmentIndex + 1) : event;
              const visibleCommands = shownCommands.filter((command) => command.frontEndAnimation !== "status_sync");
              const guardNames = new Set(event.filter((command) => command.frontEndAnimation === "guard" && command.text !== "守护").map((command) => command.text));
              return visibleCommands.length > 0 ? <p className="battle-line" key={`${index}-${visibleCommands.map((command) => command.text).join("")}`}>{visibleCommands.map((command, commandIndex) => {
                const isParryCounterNumber = command.frontEndAnimation === "parry_counter_damage" && command.renderTone === 3;
                const isGuardCommand = command.frontEndAnimation === "guard";
                const isGuardLabel = isGuardCommand && command.text === "守护";
                const isGuardName = guardNames.has(command.text);
                const poisonFailureText = command.frontEndAnimation === "poison_fail" ? command.text.trim() || "失败" : command.text;
                const commandTone = isGuardLabel ? "tone-ironwall" : isGuardName ? "tone-normal" : command.frontEndAnimation === "poison_fail" ? "tone-poison" : toneClass(command.renderTone);
                const commandClassName = `${commandTone}${isParryCounterNumber ? " parry-counter-number" : ""}`;
                return <span className={commandClassName} key={`${commandIndex}-${command.text}`}>{isParryCounterNumber && <i className="parry-counter-flash-ring" aria-hidden="true" />}{command.frontEndAnimation === "freeze_apply" && command.text === "冻结" && <img className="freeze-icon" src={FREEZE_ICON_URL} alt="" aria-hidden="true" />}{command.frontEndAnimation === "ironwall" && command.text === "铁壁" && <img className="ironwall-icon" src={IRONWALL_ICON_URL} alt="" aria-hidden="true" />}{command.frontEndAnimation === "lifesteal_attack" && command.text === "吸血攻击" && <img className="lifesteal-icon" src={LIFESTEAL_ICON_URL} alt="" aria-hidden="true" />}{command.frontEndAnimation === "fireball" && command.text === "火球术" && <img className="fire-icon" src={FIRE_ICON_URL} alt="" aria-hidden="true" />}{command.frontEndAnimation === "poison" && command.text === "投毒" && <img className="poison-icon" src={POISON_ICON_URL} alt="" aria-hidden="true" />}{poisonFailureText}</span>;
              })}</p> : null;
            })}
            {eventIndex + 1 < events.length && <p className="playback-wait">战斗播放中…</p>}
          </div>
        </section>
      </section>
    </main>
  );
}

export default function Home() {
  const [names, setNames] = useState("");
  const [battle, setBattle] = useState<CppBattleSimulationResponse | null>(null);
  const [error, setError] = useState("");
  const [loading, setLoading] = useState(false);
  const [longBattle, setLongBattle] = useState(true);
  const [compactMode, setCompactMode] = useState(false);

  const startBattle = async () => {
    if (!names.trim()) { setError("请输入至少两个名字，并用空行分隔队伍。"); return; }
    setLoading(true);
    setError("");

    try { setBattle(await requestCppBattleSimulation(names, longBattle)); }
    catch (reason) { setError(reason instanceof Error ? reason.message : "无法读取 C++ 完整战斗结果。"); }
    finally { setLoading(false); }
  };

  if (battle) return <BattlePlayback battle={battle} compactMode={compactMode} onRestart={() => { setBattle(null); setError(""); }} />;

  return (
    <main className="start-screen">
      <section className="start-console" aria-label="名字竞技场输入">
        <h1><span>名字竞技场</span></h1>
        <p className="start-intro">选手入场：连续名字同队，空行分隔队伍</p>
        <div className="roster-heading"><strong>参赛名单</strong><span>空行换队</span></div>
        <textarea value={names} onChange={(event) => { setNames(event.target.value); setError(""); }} placeholder={"每行一名选手\n\n一个或多个空行分隔队伍"} aria-label="输入名字" aria-describedby={error ? "input-error" : undefined} />
        <button className={`battle-mode-toggle ${longBattle ? "is-active" : ""}`} type="button" aria-pressed={longBattle} onClick={() => setLongBattle((value) => !value)}>
          <span className="battle-mode-indicator" aria-hidden="true" />
          <span>长对局</span>
          <small>{longBattle ? "开启：完整生命与当前节奏" : "关闭：生命减半，事件放慢"}</small>
        </button>
        <button className={`battle-mode-toggle ${compactMode ? "is-active" : ""}`} type="button" aria-pressed={compactMode} onClick={() => setCompactMode((value) => !value)}>
          <span className="battle-mode-indicator" aria-hidden="true" />
          <span>简略模式</span>
          <small>{compactMode ? "开启：隐藏魔力与状态，血条密集" : "关闭：完整显示"}</small>
        </button>
        {error && <p id="input-error" className="input-error" role="alert">{error}</p>}
        <p className="start-cta-hint">名单就绪后开战</p>
        <button className="start-button" type="button" onClick={startBattle} disabled={loading}>{loading ? "结算中" : "开始对战"}</button>
      </section>
    </main>
  );
}
