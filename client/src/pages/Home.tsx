/**
 * Style reference: 白色背景、圆角蓝青发光框与圆角按钮；输入框保持直角。
 * C++/WASM completes the battle and supplies values plus RenderCommand lines; React only renders those results.
 */
import { useEffect, useMemo, useRef, useState } from "react";
import { requestCppBattleSimulation, type CppBattleSimulationResponse, type CppPlayerSnapshot, type CppRenderCommand } from "@/lib/cppSnapshot";

type DamageVisual = {
  targetPlayerId: number;
  beforeHp: number;
  afterHp: number;
  phase: "marked" | "erasing";
};

function UnitReadout({ player, damageVisual }: { player: CppPlayerSnapshot; damageVisual: DamageVisual | null }) {
  const hpPercent = Math.max(0, Math.min(100, Math.round((player.hp / player.maxHp) * 100)));
  const isDamageTarget = damageVisual?.targetPlayerId === player.id;
  const lossStartPercent = isDamageTarget ? Math.max(0, Math.min(100, (damageVisual.beforeHp / player.maxHp) * 100)) : 0;
  const lossEndPercent = isDamageTarget ? Math.max(0, Math.min(100, (damageVisual.afterHp / player.maxHp) * 100)) : 0;
  const lossWidthPercent = Math.max(0, lossStartPercent - lossEndPercent);

  return (
    <article className={`unit-readout ${player.alive === false ? "is-defeated" : ""} ${isDamageTarget ? "is-taking-damage" : ""}`}>
      <strong title={player.name}>{player.name}</strong>
      <div className="hp-meter" aria-label={`${player.name} 生命 ${player.hp}/${player.maxHp}`}>
        <span className="hp-fill" style={{ width: `${hpPercent}%` }} />
        {isDamageTarget && lossWidthPercent > 0 && <span className={`hp-loss ${damageVisual.phase === "erasing" ? "is-erasing" : ""}`} style={{ left: `${lossEndPercent}%`, width: `${lossWidthPercent}%` }} />}
      </div>
      <div className="hp-values"><span>HP</span><span>{player.hp}/{player.maxHp}</span></div>
    </article>
  );
}

function AttributeStrip({ players }: { players: CppPlayerSnapshot[] }) {
  const namesOnly = players.length > 10;

  return (
    <section className="attribute-strip" aria-label="参战者基础属性">
      <header><strong>基础属性</strong><span>{players.length} 人</span></header>
      <div className={namesOnly ? "attribute-strip-list names-only" : "attribute-strip-list"}>
        {players.map((player) => namesOnly ? (
          <p key={player.id}>{player.name}</p>
        ) : (
          <p key={player.id}><strong>{player.name}</strong><span>命 {player.maxHp}　攻 {player.physicalAttack}　防 {player.physicalDefense}　速 {player.speed}　魔攻 {player.magicAttack}　魔防 {player.magicDefense}　智 {player.wisdom}　魔力 {player.maxMana}</span></p>
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
  const toneMap: Record<number, string> = { 1: "tone-system", 2: "tone-skill", 3: "tone-damage", 4: "tone-heal", 5: "tone-status", 6: "tone-warning" };
  return toneMap[renderTone] ?? "tone-normal";
}

function BattlePlayback({ battle }: { battle: CppBattleSimulationResponse }) {
  const events = useMemo(() => groupCommandsIntoLines(battle.commands), [battle.commands]);
  const [eventIndex, setEventIndex] = useState(0);
  const [displayPlayers, setDisplayPlayers] = useState<CppPlayerSnapshot[]>(() => battle.initialPlayers.map((player) => ({ ...player, alive: true })));
  const [damageVisual, setDamageVisual] = useState<DamageVisual | null>(null);
  const displayPlayersRef = useRef(displayPlayers);

  useEffect(() => {
    const resetPlayers = battle.initialPlayers.map((player) => ({ ...player, alive: true }));
    setEventIndex(0);
    displayPlayersRef.current = resetPlayers;
    setDisplayPlayers(resetPlayers);
    setDamageVisual(null);
  }, [battle]);

  useEffect(() => {
    if (eventIndex >= events.length) return;

    const currentEvent = events[eventIndex];
    const damageCommand = currentEvent.find((command) => command.renderTone === 3 && (command.frontEndAnimation === "normal_attack_damage" || command.frontEndAnimation === "status_damage"));
    const healCommand = currentEvent.find((command) => command.frontEndAnimation === "heal" || command.frontEndAnimation === "status_heal");
    const deathCommand = currentEvent.find((command) => command.frontEndAnimation === "death");

    if (damageCommand && damageCommand.targetPlayerId !== 0) {
      const target = displayPlayersRef.current.find((player) => player.id === damageCommand.targetPlayerId);

      if (target) {
        setDamageVisual({ targetPlayerId: target.id, beforeHp: target.hp, afterHp: damageCommand.valueAfter, phase: "marked" });
        const eraseTimer = window.setTimeout(() => {
          setDamageVisual((visual) => visual ? { ...visual, phase: "erasing" } : null);
          setDisplayPlayers((players) => {
            const nextPlayers = players.map((player) => player.id === target.id ? { ...player, hp: damageCommand.valueAfter } : player);
            displayPlayersRef.current = nextPlayers;
            return nextPlayers;
          });
        }, 420);
        const nextTimer = window.setTimeout(() => {
          setDamageVisual(null);
          setEventIndex((index) => index + 1);
        }, 1360);
        return () => { window.clearTimeout(eraseTimer); window.clearTimeout(nextTimer); };
      }
    }

    if (healCommand && healCommand.targetPlayerId !== 0) {
      setDisplayPlayers((players) => {
        const nextPlayers = players.map((player) => player.id === healCommand.targetPlayerId ? { ...player, hp: healCommand.valueAfter } : player);
        displayPlayersRef.current = nextPlayers;
        return nextPlayers;
      });
    }

    if (deathCommand && deathCommand.targetPlayerId !== 0) {
      setDisplayPlayers((players) => {
        const nextPlayers = players.map((player) => player.id === deathCommand.targetPlayerId ? { ...player, alive: false } : player);
        displayPlayersRef.current = nextPlayers;
        return nextPlayers;
      });
    }

    const nextTimer = window.setTimeout(() => setEventIndex((index) => index + 1), 220);
    return () => window.clearTimeout(nextTimer);
  }, [battle, eventIndex, events]);

  const renderedEvents = events.slice(0, Math.min(eventIndex + 1, events.length));

  return (
    <main className="arena-screen" aria-label="名字竞技场战斗界面">
      <section className="arena-frame">
        <AttributeStrip players={battle.initialPlayers} />
        <aside className="unit-panel" aria-label="参战单位生命栏">
          <header className="panel-title">单位</header>
          <div className="unit-list">{displayPlayers.map((player) => <UnitReadout key={player.id} player={player} damageVisual={damageVisual} />)}</div>
        </aside>
        <section className="playback-panel" aria-label="战斗播放区">
          <header className="playback-header"><h1>战斗</h1><span>{Math.min(eventIndex + 1, events.length)}/{events.length}</span></header>
          <div className="playback-stream" aria-live="polite">
            {renderedEvents.map((event, index) => <p className="battle-line" key={`${index}-${event.map((command) => command.text).join("")}`}>{event.map((command, commandIndex) => <span className={toneClass(command.renderTone)} key={`${commandIndex}-${command.text}`}>{command.text}</span>)}</p>)}
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

  const startBattle = async () => {
    if (!names.trim()) { setError("请输入至少两个名字，并用空行分隔队伍。"); return; }
    setLoading(true);
    setError("");

    try { setBattle(await requestCppBattleSimulation(names)); }
    catch (reason) { setError(reason instanceof Error ? reason.message : "无法读取 C++ 完整战斗结果。"); }
    finally { setLoading(false); }
  };

  if (battle) return <BattlePlayback battle={battle} />;

  return (
    <main className="start-screen">
      <section className="start-console" aria-label="名字竞技场输入">
        <h1>名字竞技场</h1>
        <textarea value={names} onChange={(event) => { setNames(event.target.value); setError(""); }} placeholder={"每行输入一个名字\n\n空行分隔队伍"} aria-label="输入名字" aria-describedby={error ? "input-error" : undefined} />
        {error && <p id="input-error" className="input-error" role="alert">{error}</p>}
        <button className="start-button" type="button" onClick={startBattle} disabled={loading}>{loading ? "结算中" : "开始"}</button>
      </section>
    </main>
  );
}
