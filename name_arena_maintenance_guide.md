# `name_arena.cpp` 维护者手册

## 1. 文件职责与维护原则

`cpp/name_arena.cpp` 是名字竞技场唯一的**战斗权威**。它负责名单解析、属性派生、确定性随机、行动调度、技能数值、状态结算、死亡联动、文本事件、JSON 序列化，以及 WebAssembly 导出。React/TypeScript 不应重新计算伤害、目标、概率、技能选择或单位生死；前端只输入名单、读取快照并按 C++ `Cmd` 指令播放。

> **维护规则：**任何会改变数值、随机结果、目标选择、状态层数、存活状态或日志顺序的改动，都应优先放在本文件中；前端只新增与 `Cmd`/快照对应的显示逻辑。

当前构建入口是 `cpp/build_wasm.sh`。修改 C++ 后必须先重建 WebAssembly；本项目不保留项目内战斗核心回归源码，日常检查使用 WASM 构建、前端播放器回归、TypeScript 与生产构建。

```bash
bash cpp/build_wasm.sh
pnpm test      # 仅前端播放器/模式回归
pnpm check
pnpm build
```

## 2. 总体数据流

| 阶段 | 入口/函数 | 输入 | 输出与职责 |
|---|---|---|---|
| 名单解析 | `read_input` | 多行名字，空行分队 | 创建 `p[1..n]`，写入队伍和初始属性，初始化战斗随机种子 |
| 初始快照 | `players_json` | 当前 `p` | 输出给前端的玩家数组；此时还没有战斗中召唤的幻魔 |
| 战斗调度 | `fight` | 玩家表与种子 | 按行动槽推进时间、开始回合、回魔、选技能、回合末状态结算 |
| 数值结算 | `take_hit`/`receive_damage` | 来源、目标、伤害 | 依次处理招架、守护、扣血、死亡、被动反击、吞噬与眷属联动 |
| 事件传输 | `add` | 文字、动画、数值、状态 | 生成前端唯一可信的 `Cmd` 序列 |
| 返回浏览器 | `simulate`/WASM 导出 | 原始名单 | 返回初始/最终玩家、命令流、胜者与统计 JSON |

## 3. 代码约定

本文件使用 `#define int long long`，因此所有未显式写出类型的 `int` 实际上是 64 位有符号整数。玩家、技能槽和绝大多数循环均采用 **1-index**；`0` 通常表示“无目标”“无来源”或“未找到”。循环风格保持为 `for(int i = 1;i <= n;++ i)`。

| 约定 | 含义 | 维护注意事项 |
|---|---|---|
| `x` | 技能施放者或伤害来源座位 | 读取 `p[x]` 前先确认来源仍存活（持续伤害的 `x` 可为 0） |
| `it` | 目标座位 | 不要将 `it` 与团队编号混用 |
| `d` | 已计算完成的实际伤害/治疗量 | 进入 `receive_damage` 前应已完成倍率、防御、波动、易损计算 |
| `sid` | 技能编号 | 用于日志、前端动画归类和同次招架识别 |
| `ani` | 前端动画名称 | 前端 `Home.tsx` 按该字符串更新血蓝条、眷属、复活与文字效果 |
| `end_line`/`nl` | 是否结束日志行 | 多段技能或守护分摊需要谨慎设置，避免日志被拆错行 |

## 4. 主要数据结构与变量

### 4.1 `Player`

`Player` 保存一名单位的完整战斗状态。普通角色由 `make_player` 创建；幻魔由 `summon` 在战斗中追加，因此前端不能假设 `n` 在战斗期间固定。

| 字段 | 作用 | 维护说明 |
|---|---|---|
| `name`、`no`、`id` | 名字、座位、队伍 | `no` 是 `p` 下标，`id` 是空行解析出的队伍号 |
| `hp`、`maxhp` | 当前/最大生命 | 改动生命要走 `receive_damage` 或显式治疗；不要仅改前端 |
| `atk`、`def` | 物攻/物防基础值 | 物理伤害用 `get_atk`、`get_def`，铁壁会改变实际防御 |
| `magic`、`maxmagic`、`mreg` | 当前魔力、上限、每回合回复 | 法术消耗由 `use_skill` 统一扣除，技能函数不要重复扣蓝 |
| `spd`、`act` | 基础速度、行动槽 | `fight` 用 `get_spd` 累加；行动阈值严格大于 `20000` |
| `satk`、`sdef`、`iq` | 魔攻、魔防、智慧 | 魔法伤害优先用 `magic_damage`；智慧影响若干目标选择与特殊技能概率 |
| `alive` | 存活标记 | 任何死亡都应经 `report_death` 输出后续联动；不要只设为 `false` |
| `freeze`、`freeze_int` | 冻结层数/强度 | 由 `unfreeze` 优先处理；冻结会阻碍正常行动 |
| `def_plus_int`、`def_plus_time` | 防御强化强度/层数 | `get_def` 将强度视为百分比；铁壁使用它们 |
| `burn_int`、`burn_time` | 烧伤单次伤害/层数 | 在 `end_turn` 每回合结算一次并减一层 |
| `posion_int`、`poison_time` | 中毒单次伤害/层数 | 字段拼写为历史兼容保留；每层每回合各结算一次 |
| `parry_time` | 招架等待层数 | 直接攻击进入 `take_hit` 时可触发反击；未触发会在自身回合消耗 |
| `has_guard`、`has_last_stand`、`has_counter`、`has_devour` | 被动资格 | 不要把被动函数直接放入主动技能抽取 |
| `last_stand_used` | 垂死挣扎本局是否用过 | 创建角色时重置，避免同局重复触发 |
| `is_familiar`、`owner` | 是否幻魔、所属本体座位 | 本体死亡时 `dismiss_familiars` 立即消灭眷属 |
| `magic_vuln` | 常驻魔法易损等级 | 当前幻魔固定为 `20`，`magic_damage` 和瘟疫会令其承受 200% 魔法伤害 |
| `spd_up_*`、`spd_down_*` | 速度强化/削减强度和层数 | `get_spd` 中每点强度换算为 5% 基础速度 |
| `sn`、`skill`、`can`、`w`、`tmp` | 技能数量、函数指针、资格、权重、待执行队列 | `tmp` 优先于普通选技，适用于解除冻结等束缚技能 |

### 4.2 `Cmd`

`Cmd` 是从 C++ 到 React 的渲染协议。一行日志通常由多个 `Cmd` 拼接；前端应用每条指令自带的状态快照，而非根据文字推断状态。

| 字段 | 含义 | 前端用途 |
|---|---|---|
| `a`、`b` | 来源/目标座位 | 定位要更新的单位条、眷属和事件来源 |
| `sid` | 技能编号 | 同次多段攻击和前端事件归类 |
| `val`、`after` | 本次变化量、结算后数值 | 驱动伤害、治疗、魔力条动画 |
| `col`、`str`、`nl` | 颜色、文本片段、是否换行 | 拼接战斗日志 |
| `ani` | 动画键 | 例如 `heal`、`poison_damage`、`summon_spawn` |
| 状态快照字段 | 冻结、铁壁、速度、烧伤、中毒、招架、`alive` | Cmd 到达时立即更新左侧状态与死亡/复活外观 |

### 4.3 全局状态

| 变量 | 工作原理 |
|---|---|
| `p`、`n` | 1-index 玩家表和当前单位数量；召唤会增加 `n` |
| `e` | 所有 `Cmd` 的事件流；首个空元素使有效命令从 `e[1]` 开始 |
| `seed`、`tar_seed` | 两套确定性随机源。前者用于伤害波动、状态成功率等，后者用于目标和权重选择，避免相互改变随机序列 |
| `vis`、`tag` | 统计仍存活队伍时的去重标记，避免清空大数组 |
| `revive_used` | 每个队伍是否已成功复苏一次 |
| `win`、`tim`、`cnt` | 胜者、战斗时刻数、已执行行动数，最终随模拟结果返回 |
| `parry_source`、`parry_target`、`parry_sid` | 同一次多段攻击的招架识别标记，防止雷击每段都重复触发反击 |

## 5. 函数索引与工作原理

### 5.1 哈希、随机、JSON 与事件函数

| 函数 | 工作原理 | 维护时如何使用 |
|---|---|---|
| `get_hash(s)` | FNV-1a 计算稳定 64 位名字种子 | 新技能资格应使用独立前缀，如 `"new-skill:" + name`，避免扰动旧机制 |
| `get_rand64(x)` | SplitMix64 单步输出 | 仅供确定性属性派生工具调用 |
| `get_val(x,l,r)` | 对身份种子无偏映射闭区间整数 | 创建角色时派生属性、技能资格、初始权重 |
| `get_normal(x,l,r)` | 用 12 个均匀样本近似正态分布 | 当前用于回魔等需要中间值更常见的属性 |
| `rnd()` | Xorshift32 战斗随机 | 伤害波动、雷击段数、状态概率等战斗内随机 |
| `rnd_tar()`、`rnd_id(m)` | 独立目标随机和无偏 `1..m` 编号 | 目标选择、守护概率与加权技能抽取；`m<=0` 返回 0 |
| `esc(s)` | 转义 JSON 特殊字符 | 新增任何手工拼 JSON 字段时必须使用 |
| `add(...)` | 创建 `Cmd`，记录文字、动画、数值和目标状态快照 | 所有新日志/动画都必须走此函数 |
| `tip(str,col)` | 创建无来源无目标的独立文本行 | 用于全局提示，不可用来修改单位状态 |

### 5.2 属性、创建与名单解析

| 函数 | 工作原理 | 维护提示 |
|---|---|---|
| `get_maxhp(x)` | 返回当前最大生命值 | 治疗、复活、吞噬和生命条快照必须通过它读取上限 |
| `get_atk(x)` | 返回当前物理攻击基础值 | 普攻、戳刺、会心、吸血和反击使用它计算物理倍率 |
| `get_maxmagic(x)` | 返回当前魔力上限 | `fight` 回魔和快照序列化使用它；当前通常为 200 |
| `get_mreg(x)` | 返回每回合固定回魔量 | 只在 `fight` 的正常行动分支使用 |
| `get_satk(x)` | 返回当前魔法攻击基础值 | 法术伤害、治疗与净化恢复的来源 |
| `get_sdef(x)` | 返回当前魔法防御基础值 | `magic_damage` 用它抵扣法术伤害 |
| `get_iq(x)` | 返回当前智慧基础值 | 目标策略、特殊技能概率与快照读取均使用它 |
| `get_def(x)` | 基础防御叠加 `def_plus_int` 百分比 | 新物理伤害必须使用它，才能受铁壁影响 |
| `get_spd(x)` | 基础速度按速度增减强度计算，最低为 1 | 行动调度只使用此函数 |
| `magic_damage(x,it,mul)` | 魔攻倍率减魔防、90%–110% 波动，再应用幻魔魔法易损 | 新法术伤害首选它；非伤害法术不需要调用 |
| `make_player(s,id,long_battle)` | 由名字创建角色、派生属性、登记全部技能槽与资格 | 新技能需要在此登记函数指针、资格哈希和权重 |
| `empty_line(s)` | 判断输入行是否只有空白 | 空白行用于队伍分隔 |
| `read_input(s,long_battle)` | 重置整局、解析队伍、创建玩家、初始化随机种子 | `snapshot` 和 `simulate` 都以它为唯一初始化入口 |
| `alive_team()` | 统计存活队伍并在仅剩一队时写入 `win` | 新死亡机制后应保留它的调用时机 |

### 5.3 目标与资格选择函数

| 函数 | 工作原理 | 使用示例 |
|---|---|---|
| `get_target(x)` | 等概率随机存活敌人 | 单目标攻击：`int it = get_target(x); if(it == 0) return;` |
| `get_plague_target(x)` | 智慧大于 125 时选择当前生命最高敌人，否则随机 | 瘟疫专用目标策略 |
| `get_life_wheel_target(x)` | 选择当前生命最高敌人 | 生命交换专用，不会随机 |
| `can_heal_target(x,it)` | 判断 `it` 是否为 `x` 的存活、未满血队友 | 为治疗候选的底层谓词，不直接选人 |
| `has_heal_target(x)` | 扫描是否存在任何可治疗目标 | `pickable_special` 过滤治疗术时使用 |
| `get_heal_target(x)` | 选择治疗目标；高智慧偏向最低生命友军，否则随机 | 只在 `has_heal_target(x)` 为真后调用 |
| `can_revive_target(x,it)` | 判断 `it` 是否为同队死亡目标且队伍未用复苏 | 复苏目标的底层谓词 |
| `has_revive_target(x)` | 扫描是否存在可复苏目标 | 过滤复苏术并影响其紧急权重 |
| `get_revive_target(x)` | 在合法死亡队友中选择复苏对象 | 复苏函数内调用，不能绕过队伍次数限制 |
| `can_purify_target(x,it)` | 判断 `it` 是否是带负面状态的合法友方 | 净化候选的底层谓词 |
| `has_purify_target(x)` | 扫描是否存在可净化目标 | 过滤净化术的可用性 |
| `get_purify_target(x)` | 选择净化目标；高智慧时偏向负面状态更重者 | 只在 `has_purify_target(x)` 为真后调用 |
| `get_ironwall_target(x)` | 高智慧优先低于 40% 生命的最低血量队友，否则自己 | 铁壁指定目标策略 |
| `has_familiar(x)` | 查询本体当前是否有存活幻魔 | 新增眷属召唤前必须检查，保证每本体至多一个 |
| `can_guard_target/get_guardian` | 校验守护者资格与存活余量，再逐位进行 20% 判定 | 不要在新伤害函数中自行复制守护概率；直接进入 `take_hit` |

### 5.4 统一伤害、死亡和被动管线

| 函数 | 工作原理 | 关键限制 |
|---|---|---|
| `clear_parry_action()` | 清空同次攻击的招架标记 | 多段攻击完全结束后调用；不要每一段都清空 |
| `receive_damage(it,d,type)` | 唯一扣血入口，只改 `hp/alive`，返回 0/1/2 | 不写日志、不处理死亡；调用者必须继续处理返回值 |
| `finish_hurt(x,it,type,end_line)` | 存活受伤后的垂死挣扎与被动反击 | 仅在 `receive_damage` 返回 1 后调用 |
| `counter_attack(x,it)` | 招架成功的 350% 物攻反击 | 由 `take_hit` 触发；不可手写重复招架文案 |
| `passive_counter(x,it,end_line)` | 25% 被动普攻反击，使用 `HURT_COUNTER` 防递归 | 只适用于带来源的有效受伤 |
| `devour(x,it)` | 击杀后继承技能/权重并恢复最大生命 15% | 由 `report_death` 在有来源时调用 |
| `dismiss_familiars(x)` | 本体死亡时消灭其存活幻魔并发出事件 | 幻魔死亡不能反向调用本体死亡 |
| `report_death(source,it,sid)` | 死亡日志、眷属消失、吞噬联动统一出口 | 任何新增伤害杀死目标后都应调用 |
| `take_guard_damage(...)` | 一次守护分摊及两份 40% 伤害日志 | 内部禁止递归守护；可处理同一行反击 |
| `take_hit(...)` | 直接伤害总入口：招架 → 守护 → 扣血 → 死亡/被动 | 普攻、物理技能与可招架法术都使用它 |

### 5.5 主动伤害、治疗与状态技能

| 函数 | 类型与数值 | 文本/维护要点 |
|---|---|---|
| `basic_attack` | 随机敌人，100% 物攻减防，80%–120% 波动 | 物理技能的最小模板；结束后清招架动作 |
| `stab` | 150% 物攻物理伤害；命中存活者获得 5 强度、2 层减速 | 状态静默，仅通过 Cmd 快照显示 |
| `critical_strike` | 200% 物攻物理伤害 | 与普通攻击同样经过招架和守护 |
| `parry` | 增加 2 层招架等待 | 自身回合若未触发会被 `skip_parry_turn` 消耗 |
| `guard` | 空函数，占位表示被动守护 | 被动资格由 `has_guard` 与 `get_guardian` 使用，不在自身回合调用 |
| `poison` | 50% 成功，强度累加施法者物攻 20%，层数加一 | 失败只输出深绿色“失败”；伤害在 `end_turn` 发生 |
| `lifesteal_attack` | 100% 物攻物理伤害，恢复计算伤害的 100%，最大生命封顶 | 通过改写最后伤害片段把吸血显示并入同一行 |
| `fireball` | 150% 魔攻法术伤害，命中存活后叠加 `d*5%` 烧伤和 2 层 | 新烧伤规则应在命中返回 1 后再施加 |
| `plague` | 目标当前生命 70%–100% 伤害，魔攻提高下限 | 直接 `receive_damage`，明确绕过招架与守护 |
| `life_wheel` | 与最高生命敌人直接交换当前生命 | 不属于伤害；不能调用 `take_hit` |
| `thunder` | 3–5 段魔法；普通段 30% 魔攻、终段 80% | 先发 3–5 个空行；多段结束才清招架标记 |
| `earthquake` | 无重复攻击 4–6 名敌人，每人 80% 魔攻 | 敌人不足时攻击全部；共享同次攻击招架标记 |
| `heal_magic` | 120% 魔攻、90%–110% 治疗，智慧高时优先最低血队友 | 治疗值要用生命封顶后的有效值写 Cmd |
| `revive` | 每队一次，死亡队友以 60% 最大生命复活，清负面并加 20000 行动值 | 复活 Cmd 必须携带活着的状态快照，前端才能恢复外观 |
| `purify` | 清全部负面并恢复施法者魔攻 10% 的生命 | 与治疗一样以实际有效治疗值输出 |
| `rage` | 自身加 5 强度、10 层速度强化 | 速度只应由 `get_spd` 读取 |
| `fast_action` | 自身加 20000 行动值，再随机分配一份 20000 给队友 | 当前刻意不输出额外描述 |
| `ironwall` | 目标加 100% 防御强化，覆盖 3 个自身回合 | 通过 `def_plus_time=4` 配合回合开始递减实现三回合效果 |
| `unfreeze` | 75% 成功去 1 层，失败时重新插入 `tmp` 队首 | 成功但仍有层数时同样回队首继续优先处理 |
| `ice` | 25% 魔攻法术伤害，命中存活后加 3 层冻结 | 新控制法术应复用“命中存活后加状态”的顺序 |
| `summon` | 消耗在外层扣除，创建 150 HP/80 攻/20 双防/1500 速幻魔 | 幻魔只登记普攻，常驻魔法易损，前端靠 `summon_spawn` 动态插入 |

### 5.6 技能选择、状态时机与行动调度

| 函数 | 工作原理 | 维护提示 |
|---|---|---|
| `spell(f)` | 判断函数指针是否属于法术 | 新法术必须加入，否则不会扣蓝 |
| `cost(f)` | 返回法术魔力消耗 | 新法术在这里登记唯一消耗值 |
| `pickable_special(x,i)` | 判断技能槽是否能进入本回合特殊候选池 | 处理存活眷属、目标存在、魔力与一次性限制等前置条件 |
| `special_weight(x,i)` | 给候选技能计算实际权重 | 低血治疗、存在尸体复苏等紧急加权放在此处 |
| `special_chance(x)` | 根据智慧给出本回合尝试特殊技能的概率 | 只影响“是否尝试”，不直接选择具体技能 |
| `begin_turn(x)` | 自身回合开始时消耗强化层、解除到期状态、处理冻结队列 | 新“按自身回合倒计时”的状态应在这里维护 |
| `skip_parry_turn(x)` | 招架未被攻击触发时，消耗一层并要求跳过正常行动 | 仅影响技能/回魔分支，回合末仍会执行 |
| `end_turn(x)` | 结算烧伤、中毒、解毒与死亡 | 新 DOT 应在这里经 `receive_damage` 结算 |
| `use_skill(x)` | 先执行 `tmp`，否则按智慧概率和共享权重选择技能；统一扣蓝 | 技能函数本身不要再扣蓝；魔力不够的法术会回退普攻 |
| `ready()` | 找第一个行动槽严格大于 20000 的存活单位 | 返回座位而非队伍 |
| `next_action_time()` | 数学计算到下一名角色可行动需要跳过的时刻 | 修改行动阈值时需与 `ready` 同步 |
| `fight()` | 主战斗循环：时间跳跃、加行动槽、行动、回魔、技能、DOT、胜负 | 保持 C++ 为唯一数值权威；前端不能插入新动作 |

### 5.7 JSON、模拟与 WASM 接口

| 函数 | 作用 | 维护提示 |
|---|---|---|
| `player_json(x)` | 将一名单位完整状态手拼为 JSON | 新前端单位字段需同时扩展 `Player`、这里和 TypeScript 类型 |
| `players_json()` | 输出 `p[1..n]` 全部单位数组 | 终局会包含战斗中动态召唤的幻魔 |
| `cmds_json()` | 输出所有 Cmd 及每条状态快照 | 新 `Cmd` 字段务必同步这里和 `cppSnapshot.ts` |
| `snapshot(s)` | 只解析名单，返回初始玩家快照 | 用于开始预览，不执行战斗 |
| `simulate(s,long_battle)` | 解析、保存初始快照、执行 `fight`、保存终局快照并返回响应 | 前端的正式对局入口 |
| `name_arena_player_snapshot` | WASM C 导出，返回初始快照字符串 | JavaScript 传 UTF-8 名单；返回指针只在下一次调用前有效 |
| `name_arena_simulate_battle` | WASM C 导出，返回完整模拟响应字符串 | 第二参数非零为长对局，零为短对局 |

## 6. 常见维护示例

### 6.1 新增单目标物理技能

新技能应登记函数、资格与权重，然后复用 `take_hit`。不要自己扣血，否则会跳过招架、守护、死亡、反击与吞噬。

```cpp
void heavy_strike(int x)
{
    int it = get_target(x);
    if(it == 0) return;

    int d = max(1LL , get_atk(x) * 180 / 100 - get_def(it));
    d = max(1LL , (d * (800 + rnd() % 401) + 500) / 1000);
    add(p[x].no , p[it].no , 24 , 0 , p[it].hp , 0 , "heavy_strike" , p[x].name + "重击，" , false);
    take_hit(x , it , d , 24 , "heavy_strike" , "heavy_strike_damage");
    clear_parry_action();
}
```

接着在 `make_player` 增加技能槽，在 `spell` 中保持它为非法术，在 `pickable_special` 中确认目标存在，并在前端增加对应动画键或沿用现有物理伤害动画。

### 6.2 新增单目标法术

法术必须登记在 `spell` 与 `cost`，伤害应该通过 `magic_damage`，这样幻魔的 200% 魔法易损、魔防和随机波动都会自动生效。

```cpp
void arcane_bolt(int x)
{
    int it = get_target(x);
    if(it == 0) return;

    int d = magic_damage(x , it , 110);
    add(p[x].no , p[it].no , 25 , 0 , p[it].hp , 0 , "arcane_bolt" , p[x].name + "施放奥术箭，" , false);
    take_hit(x , it , d , 25 , "arcane_bolt" , "arcane_bolt_damage");
    clear_parry_action();
}
```

### 6.3 新增状态或持续伤害

新状态至少涉及四处：`Player` 存储字段、`Cmd` 快照字段、`add` 中的快照复制、`cmds_json/player_json` 的序列化。若前端要显示，还需扩展 `cppSnapshot.ts` 与 `Home.tsx`。

DOT 不应直接改 `hp`。应在 `end_turn` 中使用如下顺序：计算 `d` → `receive_damage` → `add` 文本与红色数值 → `report_death` 或 `finish_hurt`。这样死亡、幻魔消失和垂死挣扎路径保持一致。

### 6.4 增加文本事件

纯提示使用 `tip("文字", 色号)`。关联某个技能、单位或动画时使用 `add`，并让 `b` 指向需要同步状态的目标。状态已经改变时，**必须先改 C++ 状态再调用 `add`**，这样 Cmd 才能带上正确快照。

```cpp
p[it].freeze += 1;
add(p[x].no , p[it].no , 3 , p[it].freeze , p[it].hp , 8 , "freeze_apply" , p[it].name + "获得冻结", true);
```

## 7. 修改检查表

| 改动类型 | 必做位置 |
|---|---|
| 新技能 | 前置声明、`make_player`、`spell`/`cost`、`pickable_special`、`special_weight`、技能函数、前端动画映射 |
| 新伤害 | 物理走 `take_hit`；魔法优先 `magic_damage`；DOT 走 `receive_damage` + `report_death/finish_hurt` |
| 新状态 | `Player`、`Cmd`、`add`、序列化、TypeScript 快照、前端状态徽记与样式 |
| 新文本颜色 | C++ `col`、前端颜色映射、必要的 CSS 类 |
| 新召唤/动态单位 | C++ 创建单位、Cmd 传递动态快照、前端播放时插入 `displayPlayers`、按 owner 排序 |
| 修改概率或倍率 | 保持 C++ 唯一权威；更新前端夹具中对应的显示文本或数值 |

## 8. 当前验证边界

本项目当前不保留项目内战斗核心回归程序，也不执行独立原生 C++ 回归。维护后应至少运行 WebAssembly 构建、前端播放器回归、类型检查和生产构建；前端回归验证的是 C++ 已输出的 `Cmd` 与快照如何被播放器显示。

```bash
bash cpp/build_wasm.sh
pnpm test
pnpm check
pnpm build
git diff --check
```

## 9. 文本颜色编码、样式与动画协议

### 9.1 颜色编码的真实含义

C++ 不直接保存 CSS 颜色，而是在 `add(..., col, ...)` 的 `col` 参数中写入一个**整数色号**。`cmds_json` 将其输出为 `renderTone`；`Home.tsx` 的 `toneClass` 把色号转换成 CSS 类名；最终颜色值由 `client/src/index.css` 的 `.tone-*` 决定。也就是说，若只想改颜色视觉效果，应改 CSS 十六进制色值；若想让一种新语义拥有独立样式，才需要新增 C++ 色号、前端映射与 CSS 类三处。

| C++ `col` / `renderTone` | 前端 CSS 类 | 当前十六进制色值 | 语义与典型文本 |
|---:|---|---|---|
| `0` 或未知值 | `.tone-normal` | `#506c74` | 默认深灰蓝；角色姓名、普通叙述 |
| `1` | `.tone-system` | `#2d7f91` | 系统提示；队伍获胜、状态同步之外的通用提示 |
| `2` | `.tone-skill` | `#237fd8` | 普通技能蓝；会心一击等通用技能名 |
| `3` | `.tone-damage` | `#d15454` | 红色伤害数值和“受到”类受击信息 |
| `4` | `.tone-heal` | `#6eb783` | 绿色治疗、吸血恢复、复活恢复数值 |
| `5` | `.tone-status` | `#7d6cb0` | 紫色状态/吞噬等通用状态语义 |
| `6` | `.tone-warning` | `#c85a7b` | 警告、战斗上限或异常信息 |
| `7` | `.tone-thunder` | `#c69c2d` | 黄色雷击术文本；**仅雷击术应使用此色号** |
| `8` | `.tone-freeze` | `#73bde3` | 浅蓝冻结、解冻文字 |
| `9` | `.tone-stack` | `#e89235` | 橙色层数、获得 Buff 的数字强调 |
| `10` | `.tone-ironwall` | `#9aa5b1` | 银灰铁壁、守护、招架相关文字 |
| `11` | `.tone-earthquake` | `#9a6941` | 棕色地裂术 |
| `12` | `.tone-rage` | `#e89235` | 橙色暴走；与 `9` 共用视觉色但语义不同 |
| `13` | `.tone-fire` | `#e66928` | 橙色火球、点燃、烧伤 |
| `14` | `.tone-poison` | `#197545` | 深绿色投毒、中毒、**仅“毒素伤害”字样** |
| `15` | `.tone-gold` | `#b88a18` | 金色快速行动、疾走/速度法术 |
| `16` | `.tone-life-wheel` | `#8758bd` | 紫色生命之轮 |
| `17` | `.tone-summon` | `#21a8bd` | 青色召唤文本 |

> **颜色分段的原则：**一整句日志可以由多条 `Cmd` 拼接，因此同一行中角色名字、技能名、伤害数字可有不同色号。例如“甲使用雷击术，乙受到 32 点伤害”应至少拆成普通名字段、`col=7` 的“雷击术”、普通“乙受到”、`col=3` 的伤害数值。不要为方便而将整行设成一种颜色。

### 9.2 动画键与资源变化

`ani` 并不负责改变数值；它告诉前端以哪一种方式播放已经由 C++ 决定的 `val` 与 `after`。所有单位状态仍以 Cmd 的快照为准。

| `ani` 示例 | 前端效果 | C++ 传参约定 |
|---|---|---|
| `mana_gain`、`mana_cost` | 蓝色魔力条增加/浅蓝消耗区段 | `val` 为变动量，`after` 为结算后的魔力 |
| `basic_damage`、`thunder_damage`、`fireball_damage` | 红色生命损失动画 | `val` 为实际伤害，`after` 为目标当前生命 |
| `heal`、`lifesteal_heal`、`revive_heal` | 浅绿色待恢复区段与生命回升 | `val` 必须是上限截断后的实际治疗，不是理论治疗 |
| `freeze_apply`、`ironwall_apply`、`status_sync` | 状态徽记立即按 Cmd 快照更新 | 必须在修改 `p[b]` 的状态后调用 `add` |
| `summon_spawn`、`familiar_dismiss` | 动态加入幻魔或令其灰暗消失 | `finalPlayers`/事件目标须能定位该动态单位 |
| `battle_end`、`battle_limit` | 系统结局文本 | 通常无目标，`a=b=0` |

### 9.3 新增一种专属颜色的最小改动示例

假设新增“奥术箭”并希望使用青紫色：先在 C++ 约定未占用色号 `18`，然后在 `Home.tsx` 的 `toneMap` 增加 `18: "tone-arcane"`，最后在 `index.css` 增加 `.tone-arcane { color: #6d67c8; }`。技能发文时使用 `add(..., 18, "arcane_bolt", "奥术箭", false)`。若只希望沿用现有紫色生命之轮视觉，则直接使用 `col=16`，不应复制一套映射。

## 10. 逐函数维护示例

以下示例用于说明每个函数在维护时的典型调用、输入或验证方式。示例中的 `x` 为施放者/来源座位，`it` 为目标座位，均假定它们已经通过目标合法性检查；不是建议在前端调用这些函数。

### 10.1 基础工具、属性和初始化函数

| 函数 | 示例 | 该示例说明 |
|---|---|---|
| `get_hash(s)` | `uint64_t h = get_hash("name-arena:" + p[x].name);` | 对名字加固定前缀后派生稳定资格，避免与其他规则共用哈希流。 |
| `get_rand64(x)` | `uint64_t roll = get_rand64(h);` | 推进身份派生的局部种子；仅在角色创建阶段使用。 |
| `get_val(x,l,r)` | `int w = get_val(h , 0 , 10);` | 为某个技能登记稳定的 0–10 权重。 |
| `get_normal(x,l,r)` | `p[n].mreg = get_normal(h , 20 , 40);` | 令回魔值更常落在区间中部而仍保持同名稳定。 |
| `rnd()` | `int factor = 900 + rnd() % 201;` | 在战斗中得到 90%–110% 的伤害浮动系数。 |
| `rnd_tar()` | `uint32_t v = rnd_tar();` | 只有实现新的目标抽样算法时才直接调用；普通选择应使用 `rnd_id`。 |
| `rnd_id(m)` | `int index = rnd_id((int)candidates.size());` | 从候选列表中无偏选出 1-index 序号；空列表时返回 0。 |
| `esc(s)` | `"\"name\":\"" + esc(p[x].name) + "\""` | 手写 JSON 文本时转义名字中的引号与换行。 |
| `add(...)` | `add(x , it , 1 , d , p[it].hp , 3 , "basic_damage" , to_string(d) , true);` | 在**已经扣血后**发出红色伤害 Cmd，并把目标状态冻结为历史快照。 |
| `tip(str,col)` | `tip("战斗达到结算上限" , 6);` | 输出一条不绑定单位条的独立警告行。 |
| `get_maxhp(x)` | `int ceiling = get_maxhp(it);` | 治疗封顶与复活比例应读取该函数，而不是直接硬编码。 |
| `get_atk(x)` | `int base = get_atk(x) * 150 / 100;` | 计算戳刺等物理倍率的起点。 |
| `get_def(x)` | `int d = max(1LL , get_atk(x) - get_def(it));` | 让铁壁防御强化自动参与物理减伤。 |
| `get_maxmagic(x)` | `p[x].magic = min(get_maxmagic(x) , p[x].magic + get_mreg(x));` | 每回合回魔时保证不超过统一魔力上限。 |
| `get_mreg(x)` | `int gained = get_mreg(x);` | 读取名字派生出的当回合固定回复魔力。 |
| `get_spd(x)` | `p[x].act += get_spd(x) * step;` | 行动槽必须读实际速度，才能反映暴走与减速。 |
| `get_satk(x)` | `int heal = get_satk(x) * 120 / 100;` | 治愈魔法的未波动基础值。 |
| `get_sdef(x)` | `int raw = get_satk(x) - get_sdef(it);` | 若新法术不直接调用 `magic_damage`，至少要按此原则抵扣魔防。 |
| `magic_damage(x,it,mul)` | `int d = magic_damage(x , it , 150);` | 火球术传 150，可统一获得魔防、随机波动和幻魔易损。 |
| `get_iq(x)` | `if(get_iq(x) > 125) it = get_plague_target(x);` | 按智慧选择智能目标策略。 |
| `make_player(s,id,long_battle)` | `make_player(line , team_id , long_battle);` | 每读取一行非空名单时创建完整角色与技能资格。 |
| `empty_line(s)` | `if(empty_line(line)) ++team_id;` | 空白行仅用于切换到下一队。 |
| `read_input(s,long_battle)` | `read_input(rawRoster , true);` | 清空上一局状态并由原始名单创建新战局。 |
| `alive_team()` | `if(alive_team() == 1) return;` | 回合后判断胜负；函数同时更新全局 `win`。 |

### 10.2 目标、候选与资格函数

| 函数 | 示例 | 该示例说明 |
|---|---|---|
| `get_target(x)` | `int it = get_target(x); if(it == 0) return;` | 单目标攻击的标准敌人选择前置。 |
| `get_plague_target(x)` | `int it = get_plague_target(x);` | 瘟疫在高智慧下转为优先当前生命最高敌人。 |
| `get_life_wheel_target(x)` | `int it = get_life_wheel_target(x);` | 生命之轮锁定当前血量最高敌人。 |
| `can_heal_target(x,it)` | `if(can_heal_target(x , it)) candidates.push_back(it);` | 构造治疗候选名单时复用同队、存活、未满血条件。 |
| `has_heal_target(x)` | `if(has_heal_target(x) == false) return false;` | 过滤治疗术，防止它在全队满血时进入技能池。 |
| `get_heal_target(x)` | `int it = get_heal_target(x);` | 从合法治疗候选中按智慧策略选出目标。 |
| `can_revive_target(x,it)` | `if(can_revive_target(x , it)) return it;` | 判断同队尸体是否可被本队剩余复苏次数使用。 |
| `has_revive_target(x)` | `bool urgent = has_revive_target(x);` | 有尸体时让复苏术获得更高技能权重。 |
| `get_revive_target(x)` | `int it = get_revive_target(x);` | 实际施放复苏前取得一名死亡队友。 |
| `can_purify_target(x,it)` | `if(can_purify_target(x , it)) ++count;` | 统计有负面状态的友军。 |
| `has_purify_target(x)` | `if(!has_purify_target(x)) return false;` | 不让净化在没有负面状态时浪费行动。 |
| `get_purify_target(x)` | `int it = get_purify_target(x);` | 选出净化目标；高智慧会偏向状态更重的友军。 |
| `get_ironwall_target(x)` | `int it = get_ironwall_target(x);` | 铁壁优先保护低血友军，否则施加给自己。 |
| `has_familiar(x)` | `if(has_familiar(x)) return;` | 阻止本体仍有幻魔时重复召唤。 |
| `can_guard_target(g,t,d)` | `if(can_guard_target(g , it , d)) guardian = g;` | 验证守护者活着、未冻结/招架且分摊后不会死亡。 |
| `get_guardian(it,d)` | `int g = get_guardian(it , d);` | 由统一伤害管线完成 20% 守护抽签，而非技能函数复制概率。 |

### 10.3 受伤、死亡和被动函数

| 函数 | 示例 | 该示例说明 |
|---|---|---|
| `clear_parry_action()` | `clear_parry_action(); // 雷击所有段结束后` | 多段伤害完结才清除本次格挡识别状态。 |
| `receive_damage(it,d,type)` | `int result = receive_damage(it , d , HURT_POISON);` | DOT 扣血的最低层入口；返回值决定后续死亡或受伤后置处理。 |
| `finish_hurt(x,it,type,end_line)` | `if(result == 1) finish_hurt(x , it , HURT_DIRECT);` | 目标仍存活时处理垂死挣扎和被动反击。 |
| `counter_attack(x,it)` | `counter_attack(it , x);` | 招架目标 `it` 对攻击来源 `x` 发起 350% 物攻反击。 |
| `passive_counter(x,it,end_line)` | `passive_counter(it , x , true);` | 被动反击以原受击者为施放者，避免再次递归触发。 |
| `devour(x,it)` | `devour(x , it);` | 攻击者击杀目标后继承其技能资格/权重并恢复最大生命 15%。 |
| `dismiss_familiars(x)` | `dismiss_familiars(it);` | 本体死亡后清掉其所有仍存活的绑定幻魔。 |
| `report_death(source,it,sid)` | `if(result == 2) report_death(x , it , 10);` | 伤害杀死目标后输出死亡、处理眷属和吞噬。 |
| `take_guard_damage(...)` | `take_guard_damage(x , it , d , 18 , "guard" , "guard_damage" , "伤害" , HURT_DIRECT , true , true);` | 仅供 `take_hit` 内部调用，执行一次不链式的两方 40% 分摊。 |
| `take_hit(...)` | `take_hit(x , it , d , 1 , "basic" , "basic_damage");` | 新增可招架、可守护的直接伤害时使用的标准入口。 |

### 10.4 技能函数

| 函数 | 示例 | 该示例说明 |
|---|---|---|
| `basic_attack(x)` | `basic_attack(x);` | 无特殊技能或法术魔力不足时的标准回退动作。 |
| `stab(x)` | `stab(x);` | 执行 150% 物攻并在命中后叠加减速。 |
| `critical_strike(x)` | `critical_strike(x);` | 执行 200% 物攻的蓝色会心一击。 |
| `parry(x)` | `parry(x);` | 增加招架等待层，等待后续直接攻击触发反击。 |
| `guard(x)` | `guard(x);` | 不应主动调用；它是被动守护资格的函数指针占位。 |
| `poison(x)` | `poison(x);` | 执行投毒成功率判定并叠加中毒强度/层数。 |
| `lifesteal_attack(x)` | `lifesteal_attack(x);` | 造成物理伤害后按实际伤害恢复，治疗不会超过最大生命。 |
| `fireball(x)` | `fireball(x);` | 造成魔法伤害；若命中且仍存活，再加烧伤状态。 |
| `plague(x)` | `plague(x);` | 按目标当前生命比例直接伤害，特意绕过招架与守护。 |
| `life_wheel(x)` | `life_wheel(x);` | 直接交换生命，不能用作普通伤害模板。 |
| `thunder(x)` | `thunder(x);` | 输出空行和多段雷击；完成全部段后再清招架识别。 |
| `earthquake(x)` | `earthquake(x);` | 随机无重复攻击 4–6 名敌人；敌人不足则全部命中。 |
| `heal_magic(x)` | `heal_magic(x);` | 选一名合法队友，计算实际治疗并发绿色生命 Cmd。 |
| `revive(x)` | `revive(x);` | 选择尸体、扣蓝已由外层处理、复活并写入 `revive_heal`。 |
| `purify(x)` | `purify(x);` | 清负面状态并治疗目标；必须在清状态后发 Cmd 快照。 |
| `rage(x)` | `rage(x);` | 增加自身速度强化状态，实际速度由 `get_spd` 读取。 |
| `fast_action(x)` | `fast_action(x);` | 分配行动槽，但当前不额外输出日志。 |
| `ironwall(x)` | `ironwall(x);` | 对策略目标写入防御强化强度和持续层数。 |
| `unfreeze(x)` | `unfreeze(x);` | 尝试移除一层冻结；失败时重新放回 `tmp` 队首。 |
| `ice(x)` | `ice(x);` | 魔法命中仍活着的敌人后叠加三层冻结。 |
| `summon(x)` | `summon(x);` | 动态追加绑定幻魔并输出可供前端插入单位的事件。 |

### 10.5 选技、回合、序列化与 WASM 函数

| 函数 | 示例 | 该示例说明 |
|---|---|---|
| `spell(f)` | `if(spell(f)) p[x].magic -= cost(f);` | 选技执行层以此识别法术，统一处理魔力。 |
| `cost(f)` | `int c = cost(thunder); // 取得雷击术消耗` | 新法术只在这里维护唯一消耗值。 |
| `pickable_special(x,i)` | `if(pickable_special(x , i)) sum += special_weight(x , i);` | 构造本回合共享权重池时的标准过滤。 |
| `special_weight(x,i)` | `k -= special_weight(x , i);` | 加权随机抽取的每个候选扣除自身有效权重。 |
| `special_chance(x)` | `if(rnd_id(100) <= special_chance(x)) { /* 尝试特殊技能 */ }` | 决定本回合先走特殊池还是直接普攻。 |
| `begin_turn(x)` | `begin_turn(x);` | 每次行动前更新持续层数、到期解除和冻结束缚队列。 |
| `skip_parry_turn(x)` | `if(skip_parry_turn(x) == false) use_skill(x);` | 招架等待尚未触发时决定是否跳过本次正常行动。 |
| `end_turn(x)` | `end_turn(x);` | 每次行动后结算烧伤、中毒与解毒，并可能触发死亡。 |
| `use_skill(x)` | `use_skill(x);` | 先执行 `tmp`，否则按智慧概率与权重选技，并在内部统一扣蓝。 |
| `ready()` | `for(int x = ready(); x != 0; x = ready()) { /* 行动 */ }` | 逐个取出行动槽超过阈值的存活单位。 |
| `next_action_time()` | `int step = next_action_time();` | 在没有行动时一次跨过多个空战斗时刻。 |
| `fight()` | `fight();` | 只在 `simulate` 中对已经初始化的名单运行完整对局。 |
| `player_json(x)` | `string one = player_json(x);` | 调试单个单位时生成其当前快照 JSON。 |
| `players_json()` | `string initial = players_json();` | 在 `fight` 前、后各调用一次形成初始/终局快照。 |
| `cmds_json()` | `string stream = cmds_json();` | 把 `e[1..]` 序列化为前端播放的命令数组。 |
| `snapshot(s)` | `string preview = snapshot(rawRoster);` | 仅查看初始属性，不执行战斗。 |
| `simulate(s,long_battle)` | `string response = simulate(rawRoster , true);` | 产生完整 API 响应；正式前端对局应调用它。 |

## 11. 模拟一场前台战斗时，后台正在发生什么

下面以两队名单“甲、乙”对“丙、丁”为例。数值、出手者和技能由名字哈希与 C++ 随机序列决定，因此这是**流程模拟**，不承诺该名字组合必然出现雷击术；目的是展示一个“甲施放雷击术命中丙并附带状态更新”的前后台协作边界。

| 前台可见步骤 | React/播放器做的事 | 同一时刻 C++/WASM 已完成的事 |
|---|---|---|
| 1. 用户填写名单并点击开始 | 调用 `name_arena_player_snapshot` 显示初始属性预览，随后调用 `name_arena_simulate_battle` | `snapshot`/`simulate` 调用 `read_input`；空行划分队伍，`make_player` 对每个名字派生属性、技能资格、权重和被动资格 |
| 2. 画面显示四名角色、魔力均为 0 | 以 `initialPlayers` 创建 `displayPlayers`；只渲染 C++ 给出的 HP、属性与 `alive=true` | `simulate` 已先保存初始 `players_json`，随后进入 `fight`；前端此时没有决定出手顺序 |
| 3. 还没有日志，但随后甲先行动 | 播放器等待第一批 `Cmd`；不会自己累加速度或产生随机数 | `next_action_time` 数学求出到阈值的最短步数，所有存活者以 `get_spd(i)*step` 增加 `act`，`ready` 找到按座位顺序第一个 `act>20000` 的单位 |
| 4. 若甲有回魔，蓝条增长 | 接收到 `mana_gain` 指令后，把 `after` 写入甲的 `mana`，绘制蓝条变化 | `fight` 先执行 `begin_turn(甲)`，再将 `magic` 加上 `get_mreg(甲)` 并由 `add` 记录增量与结算后魔力 |
| 5. 日志出现“甲使用雷击术” | `groupCommandsIntoLines` 将 `nl=false` 的片段拼为同一行，黄色片段采用 `renderTone=7` | `use_skill(甲)` 已按 `special_chance`、`pickable_special`、`special_weight` 抽中 `thunder`，统一扣除 `cost(thunder)` 魔力，然后 `thunder(甲)` 写入发招 Cmd |
| 6. 中间出现数个空白间隔 | 播放器按 C++ 的空 Cmd 与既定节奏推进，不生成额外伤害 | `thunder` 先由 `add` 写入 3–5 个空片段，精确规定多段雷击之间的日志空间 |
| 7. 丙生命条逐段变红并下降 | 每个 `thunder_damage` Cmd 用 `value` 创建红色覆盖动画，并以 `valueAfter` 设置正确生命；文字数值为红色 `renderTone=3` | 每一段先用 `magic_damage(甲,丙,30/80)` 算出含魔防、浮动、易损的实际 `d`，再走 `take_hit`。招架或守护若触发，会替换为其对应命令与分摊结果 |
| 8. 丙状态图标在该日志行首段就更新 | 播放器读取 Cmd 的冻结/烧伤/招架等快照，第一片段便更新 `displayPlayers`；不从文字猜层数 | `add` 在每次写 Cmd 时复制目标当前状态。若雷击未施加状态，快照保持原值；若后续技能加状态，C++ 必须先改 `p[丙]` 再 `add` |
| 9. 若本段打死丙，丙立刻灰暗；若甲吞噬则甲短暂绿色回血 | React 以 Cmd 的 `alive` 为准切换 `.is-defeated`，并按吞噬治疗命令播放绿色区域 | `receive_damage` 返回死亡结果后，`report_death` 输出死亡、处理丙的幻魔；若甲有吞噬则 `devour` 恢复最大生命 15% 并继承资格/权重 |
| 10. 甲行动结束，轮到下一个达到阈值的人 | 日志继续消费已有命令；播放器不再询问后端也不重新计算 | `end_turn(甲)` 结算甲的烧伤/中毒；`fight` 接着处理同一时刻其他 `ready` 单位，或再次用 `next_action_time` 跳跃 |
| 11. 屏幕最终显示胜者 | 播放器消费 `battle_end` 并显示“队伍X获胜” | 任何行动后的 `alive_team()==1` 令 `win` 固定，`fight` 写入 `battle_end`，`simulate` 返回 `winnerTeamId`、终局 `players_json`、`cmds_json` |

> **关键结论：**浏览器拿到 `simulate` 的 JSON 时，整场战斗在 C++ 中已经完全结算完毕。所谓“实时播放”只是 React 按 `commands` 的顺序和节奏展示历史结果；它不调用随机函数，不临时决定目标，也不会在动画结束时再向 C++ 请求下一段伤害。

### 11.1 一个最小 Cmd 片段如何被前端消费

```json
{
  "sourcePlayerId": 1,
  "targetPlayerId": 3,
  "skillId": 2,
  "value": 32,
  "valueAfter": 168,
  "renderTone": 3,
  "frontEndAnimation": "thunder_damage",
  "text": "32",
  "freezeLayers": 0,
  "alive": true,
  "newlineAfter": true
}
```

该片段表示 C++ 已经确认：1 号对 3 号的雷击本段造成 32 点实际伤害，3 号结算后生命为 168，尚存活且冻结为 0。前端只需将“32”渲染成 `.tone-damage`，将 3 号生命条动画从上一个生命值推到 168，并根据 `alive=true` 保持正常外观。它不能再次计算雷击公式，也不能把数字改成别的值。



## 12. 真实战局逐行对照：1、2、3 队对 4、5、6 队

本节使用真实输入 `1\n2\n3\n\n4\n5\n6`，调用当前 WASM 的长对局模式生成。由于随机源由名单稳定派生，本表记录的是本次当前核心的实际输出；若以后修改属性、技能资格或随机调用顺序，数值与行数可能变化，但每行的映射方法不变。空的 `status_sync`、`mana_gain`、`mana_cost` Cmd 不产生可见文字，却属于同一行动的后台步骤，因此命令范围也一并列出。

| 行 | 前端最终显示文字 | Cmd 范围 | 后台实际函数链（可见事件的主链） |
|---:|---|---:|---|
| 1 | 5使用投毒失败 | 1–5 | `fight` → `begin_turn` → `use_skill` → `poison` → `rnd_id` → 状态写入 `add` |
| 2 | 3 发起攻击，4受到31伤害 | 6–14 | `fight` → `begin_turn` → `use_skill` → `basic_attack` → `get_target` → `take_hit` |
| 3 | 2 发起攻击，4受到35伤害 | 15–23 | `fight` → `begin_turn` → `use_skill` → `basic_attack` → `get_target` → `take_hit` |
| 4 | 1 发起攻击，4受到45伤害 | 24–32 | `fight` → `begin_turn` → `use_skill` → `basic_attack` → `get_target` → `take_hit` |
| 5 | 4吸血攻击，1受到31伤害 | 33–41 | `fight` → `begin_turn` → `use_skill` → `lifesteal_attack` → `get_target` → `take_hit` |
| 6 | 1反击! 4受到46伤害，吸取31血量 | 42–49 | `passive_counter` → `basic_attack` → `take_hit` |
| 7 | 6开始招架 | 50–53 | `fight` → `begin_turn` → `use_skill` → `parry` → `add` |
| 8 | 5 发起攻击，3受到61伤害 | 54–62 | `fight` → `begin_turn` → `use_skill` → `basic_attack` → `get_target` → `take_hit` |
| 9 | 3反击! 5受到24点伤害 | 63–68 | `passive_counter` → `basic_attack` → `take_hit` |
| 10 | 3 发起攻击，4守护5 | 69–76 | `fight` → `begin_turn` → `use_skill` → `basic_attack` → `get_target` → `take_hit` |
| 11 | 5受到7伤害 4受到7伤害 | 77–86 | `fight` → `begin_turn` → `use_skill` → `basic_attack` → `get_target` → `take_hit` |
| 12 | 2 发起攻击，4受到47伤害 | 87–95 | `fight` → `begin_turn` → `use_skill` → `basic_attack` → `get_target` → `take_hit` |
| 13 | 1 发起攻击，4受到46伤害 | 96–104 | `fight` → `begin_turn` → `use_skill` → `basic_attack` → `get_target` → `take_hit` |
| 14 | 4 发起攻击，2受到44伤害 | 105–113 | `fight` → `begin_turn` → `use_skill` → `basic_attack` → `get_target` → `take_hit` |
| 15 | 5使用投毒失败 | 114–119 | `fight` → `begin_turn` → `use_skill` → `poison` → `rnd_id` → 状态写入 `add` |
| 16 | 3 发起攻击，6触发招架反击，3受到213点伤害 | 120–132 | `fight` → `begin_turn` → `use_skill` → `basic_attack` → `get_target` → `take_hit` |
| 17 | 2 召唤 出了 幻魔 | 133–140 | `fight` → `begin_turn` → `use_skill` → `summon` → `add(summon_spawn)` |
| 18 | 1 发起攻击，6受到41伤害 | 141–149 | `fight` → `begin_turn` → `use_skill` → `basic_attack` → `get_target` → `take_hit` |
| 19 | 4发动疾走术 | 150–154 | `fight` → `begin_turn` → `use_skill` → `rage` → `add` |
| 20 | 6 发起攻击，幻魔受到67伤害 | 155–163 | `fight` → `begin_turn` → `use_skill` → `basic_attack` → `get_target` → `take_hit` |
| 21 | 5戳刺，3守护1 | 164–171 | `fight` → `begin_turn` → `use_skill` → `stab` → `get_target` → `take_hit` |
| 22 | 1受到38伤害 3受到38伤害3反击! 5受到25点伤害 | 172–187 | `fight` → `begin_turn` → `use_skill` → `stab` → `get_target` → `take_hit` |
| 23 | 幻魔 发起攻击，4受到37伤害 | 188–195 | `fight` → `begin_turn` → `use_skill` → `basic_attack` → `get_target` → `take_hit` |
| 24 | 3 发起攻击，4受到33伤害 | 196–204 | `fight` → `begin_turn` → `use_skill` → `basic_attack` → `get_target` → `take_hit` |
| 25 | 2 发起攻击，4守护6 | 205–212 | `fight` → `begin_turn` → `use_skill` → `basic_attack` → `get_target` → `take_hit` |
| 26 | 6受到15伤害 4受到15伤害 | 213–222 | `fight` → `begin_turn` → `use_skill` → `basic_attack` → `get_target` → `take_hit` |
| 27 | 4 发起攻击，2受到58伤害 | 223–231 | `fight` → `begin_turn` → `use_skill` → `basic_attack` → `get_target` → `take_hit` |
| 28 | 5 发起攻击，2受到85伤害 | 232–240 | `fight` → `begin_turn` → `use_skill` → `basic_attack` → `get_target` → `take_hit` |
| 29 | 幻魔 发起攻击，6受到29伤害 | 241–248 | `fight` → `begin_turn` → `use_skill` → `basic_attack` → `get_target` → `take_hit` |
| 30 | 6开始招架 | 249–252 | `fight` → `begin_turn` → `use_skill` → `parry` → `add` |
| 31 | 1 发起攻击，5受到35伤害 | 253–261 | `fight` → `begin_turn` → `use_skill` → `basic_attack` → `get_target` → `take_hit` |
| 32 | 3 发起攻击，5受到26伤害 | 262–270 | `fight` → `begin_turn` → `use_skill` → `basic_attack` → `get_target` → `take_hit` |
| 33 | 4 发起攻击，幻魔受到49伤害 | 271–279 | `fight` → `begin_turn` → `use_skill` → `basic_attack` → `get_target` → `take_hit` |
| 34 | 幻魔 发起攻击，6触发招架反击，幻魔受到313点伤害 | 280–291 | `fight` → `begin_turn` → `use_skill` → `basic_attack` → `get_target` → `take_hit` |
| 35 | 幻魔消失了 | 292–292 | `report_death` → `dismiss_familiars`（若有） |
| 36 | 6吞噬幻魔 6 恢复15%生命 | 293–297 | `report_death` → `devour` → `add` |
| 37 | 2发动疾走术 | 298–302 | `fight` → `begin_turn` → `use_skill` → `rage` → `add` |
| 38 | 5 发起攻击，3受到73伤害 | 303–311 | `fight` → `begin_turn` → `use_skill` → `basic_attack` → `get_target` → `take_hit` |
| 39 | 6 发起攻击，2受到55伤害 | 312–320 | `fight` → `begin_turn` → `use_skill` → `basic_attack` → `get_target` → `take_hit` |
| 40 | 2反击! 6受到40点伤害 | 321–326 | `passive_counter` → `basic_attack` → `take_hit` |
| 41 | 3 发起攻击，6守护5 | 327–334 | `fight` → `begin_turn` → `use_skill` → `basic_attack` → `get_target` → `take_hit` |
| 42 | 5受到9伤害 6受到9伤害 | 335–344 | `fight` → `begin_turn` → `use_skill` → `basic_attack` → `get_target` → `take_hit` |
| 43 | 4 发起攻击，2受到46伤害 | 345–353 | `fight` → `begin_turn` → `use_skill` → `basic_attack` → `get_target` → `take_hit` |
| 44 | 2 发起攻击，4受到43伤害 | 354–362 | `fight` → `begin_turn` → `use_skill` → `basic_attack` → `get_target` → `take_hit` |
| 45 | 1 发起攻击，4守护6 | 363–370 | `fight` → `begin_turn` → `use_skill` → `basic_attack` → `get_target` → `take_hit` |
| 46 | 6受到16伤害2守护1 | 371–377 | `fight` → `begin_turn` → `use_skill` → `basic_attack` → `get_target` → `take_hit` |
| 47 | 6反击! 1受到14点伤害 2受到14点伤害 4受到16伤害 | 378–395 | `passive_counter` → `basic_attack` → `take_hit` |
| 48 | 5 发起攻击，2受到75伤害 | 396–404 | `fight` → `begin_turn` → `use_skill` → `basic_attack` → `get_target` → `take_hit` |
| 49 | 6守护5 | 405–407 | `get_guardian` → `take_guard_damage` → `add` |
| 50 | 2反击! 5受到12点伤害 6受到12点伤害 | 408–419 | `passive_counter` → `basic_attack` → `take_hit` |
| 51 | 3 发起攻击，4守护6 | 420–427 | `fight` → `begin_turn` → `use_skill` → `basic_attack` → `get_target` → `take_hit` |
| 52 | 6受到9伤害6反击! 3受到46点伤害 4受到9伤害 | 428–443 | `fight` → `begin_turn` → `use_skill` → `basic_attack` → `get_target` → `take_hit` |
| 53 | 4 发起攻击，3守护2 | 444–451 | `fight` → `begin_turn` → `use_skill` → `basic_attack` → `get_target` → `take_hit` |
| 54 | 2受到16伤害6守护4 | 452–458 | `fight` → `begin_turn` → `use_skill` → `basic_attack` → `get_target` → `take_hit` |
| 55 | 2反击! 4受到14点伤害 6受到14点伤害 3受到16伤害3反击! 4受到31点伤害 | 459–482 | `passive_counter` → `basic_attack` → `take_hit` |
| 56 | 2 发起攻击，6受到34伤害 | 483–491 | `fight` → `begin_turn` → `use_skill` → `basic_attack` → `get_target` → `take_hit` |
| 57 | 3守护2 | 492–494 | `get_guardian` → `take_guard_damage` → `add` |
| 58 | 6反击! 2受到17点伤害2发动垂死挣扎，属性大幅上升!!! | 495–503 | `passive_counter` → `basic_attack` → `take_hit` |
| 59 |  3受到17点伤害 | 504–509 | `add`（该行为无可见文字） |
| 60 | 2 发起攻击，6守护5 | 510–517 | `fight` → `begin_turn` → `use_skill` → `basic_attack` → `get_target` → `take_hit` |
| 61 | 5受到13伤害3守护2 | 518–524 | `fight` → `begin_turn` → `use_skill` → `basic_attack` → `get_target` → `take_hit` |
| 62 | 5反击! 2受到30点伤害 3受到30点伤害 6受到13伤害 | 525–542 | `passive_counter` → `basic_attack` → `take_hit` |
| 63 | 6 发起攻击，2受到51伤害 | 543–551 | `fight` → `begin_turn` → `use_skill` → `basic_attack` → `get_target` → `take_hit` |
| 64 | 2消失了 | 552–552 | `report_death` → `dismiss_familiars`（若有） |
| 65 | 6吞噬2 6 恢复15%生命 | 553–557 | `report_death` → `devour` → `add` |
| 66 | 5发动铁壁，4防御力大幅提升!!! | 558–563 | `fight` → `begin_turn` → `use_skill` → `ironwall` → `get_ironwall_target` → `add` |
| 67 | 1 发起攻击，6守护5 | 564–571 | `fight` → `begin_turn` → `use_skill` → `basic_attack` → `get_target` → `take_hit` |
| 68 | 5受到12伤害 6受到12伤害6反击! 1受到35点伤害 | 572–587 | `fight` → `begin_turn` → `use_skill` → `basic_attack` → `get_target` → `take_hit` |
| 69 | 4 发起攻击，3受到36伤害 | 588–596 | `fight` → `begin_turn` → `use_skill` → `basic_attack` → `get_target` → `take_hit` |
| 70 | 3 发起攻击，6守护5 | 597–603 | `fight` → `begin_turn` → `use_skill` → `basic_attack` → `get_target` → `take_hit` |
| 71 | 5受到8伤害5反击! 3受到69点伤害 6受到8伤害 | 604–619 | `fight` → `begin_turn` → `use_skill` → `basic_attack` → `get_target` → `take_hit` |
| 72 | 5戳刺，3受到133伤害 | 620–628 | `fight` → `begin_turn` → `use_skill` → `stab` → `get_target` → `take_hit` |
| 73 | 6使用火球术1受到124点伤害，1被点燃了，获得6烧伤强度，层数为2 | 629–642 | `fight` → `begin_turn` → `use_skill` → `fireball` → `magic_damage` → `take_hit` |
| 74 | 1使用地裂术 | 643–647 | `fight` → `use_skill` → `earthquake` → `magic_damage` → `take_hit` |
| 75 | 6受到143伤害 | 648–651 | `fight` → `use_skill` → `earthquake` → `magic_damage` → `take_hit` |
| 76 | 4受到106伤害 | 652–655 | `fight` → `use_skill` → `earthquake` → `magic_damage` → `take_hit` |
| 77 | 5受到103伤害 | 656–659 | `fight` → `use_skill` → `earthquake` → `magic_damage` → `take_hit` |
| 78 | 1受到6点烧伤伤害 | 660–664 | `end_turn` → `receive_damage(HURT_BURN)` → `report_death`/`finish_hurt` |
| 79 | 4 发起攻击，3受到40伤害 | 665–673 | `fight` → `begin_turn` → `use_skill` → `basic_attack` → `get_target` → `take_hit` |
| 80 | 5 发起攻击，1受到64伤害 | 674–682 | `fight` → `begin_turn` → `use_skill` → `basic_attack` → `get_target` → `take_hit` |
| 81 | 3 发起攻击，4受到1伤害 | 683–690 | `fight` → `begin_turn` → `use_skill` → `basic_attack` → `get_target` → `take_hit` |
| 82 | 6 发起攻击，1受到44伤害 | 691–699 | `fight` → `begin_turn` → `use_skill` → `basic_attack` → `get_target` → `take_hit` |
| 83 | 1 发起攻击，6守护5 | 700–707 | `fight` → `begin_turn` → `use_skill` → `basic_attack` → `get_target` → `take_hit` |
| 84 | 5受到11伤害 6受到11伤害 | 708–717 | `fight` → `begin_turn` → `use_skill` → `basic_attack` → `get_target` → `take_hit` |
| 85 | 1受到6点烧伤伤害，从烧伤中解除 | 718–724 | `end_turn` → `receive_damage(HURT_BURN)` → `report_death`/`finish_hurt` |
| 86 | 4 发起攻击，3受到48伤害 | 725–733 | `fight` → `begin_turn` → `use_skill` → `basic_attack` → `get_target` → `take_hit` |
| 87 | 3消失了 | 734–734 | `report_death` → `dismiss_familiars`（若有） |
| 88 | 5 发起攻击，1受到67伤害 | 735–743 | `fight` → `begin_turn` → `use_skill` → `basic_attack` → `get_target` → `take_hit` |
| 89 | 4从铁壁中解除 | 744–746 | `begin_turn` → 强化层数递减 → `add` |
| 90 | 4 发起攻击，1受到34伤害 | 747–754 | `fight` → `begin_turn` → `use_skill` → `basic_attack` → `get_target` → `take_hit` |
| 91 | 6 发起攻击，1受到43伤害 | 755–763 | `fight` → `begin_turn` → `use_skill` → `basic_attack` → `get_target` → `take_hit` |
| 92 | 1发动铁壁，1防御力大幅提升!!! | 764–769 | `fight` → `begin_turn` → `use_skill` → `ironwall` → `get_ironwall_target` → `add` |
| 93 | 5 发起攻击，1受到21伤害 | 770–777 | `fight` → `begin_turn` → `use_skill` → `basic_attack` → `get_target` → `take_hit` |
| 94 | 4 发起攻击，1受到1伤害 | 778–785 | `fight` → `begin_turn` → `use_skill` → `basic_attack` → `get_target` → `take_hit` |
| 95 | 5使用治愈魔法，为4恢复164生命 | 786–792 | `fight` → `begin_turn` → `use_skill` → `heal_magic` → `get_heal_target` → `add` |
| 96 | 6使用火球术1受到127伤害 | 793–801 | `fight` → `begin_turn` → `use_skill` → `fireball` → `magic_damage` → `take_hit` |
| 97 | 1反击! 6受到34点伤害，1被点燃了，获得6烧伤强度，层数为2 | 802–812 | `passive_counter` → `basic_attack` → `take_hit` |
| 98 | 1 发起攻击，6守护4 | 813–820 | `fight` → `begin_turn` → `use_skill` → `basic_attack` → `get_target` → `take_hit` |
| 99 | 4受到16伤害 6受到16伤害 | 821–830 | `fight` → `begin_turn` → `use_skill` → `basic_attack` → `get_target` → `take_hit` |
| 100 | 1受到6点烧伤伤害 | 831–835 | `end_turn` → `receive_damage(HURT_BURN)` → `report_death`/`finish_hurt` |
| 101 | 4从疾走术中解除 | 836–838 | `begin_turn` → 强化层数递减 → `add` |
| 102 | 4 发起攻击，1受到1伤害 | 839–846 | `fight` → `begin_turn` → `use_skill` → `basic_attack` → `get_target` → `take_hit` |
| 103 | 5 发起攻击，1受到17伤害 | 847–855 | `fight` → `begin_turn` → `use_skill` → `basic_attack` → `get_target` → `take_hit` |
| 104 | 6 发起攻击，1受到1伤害 | 856–864 | `fight` → `begin_turn` → `use_skill` → `basic_attack` → `get_target` → `take_hit` |
| 105 | 1 发起攻击，5受到28伤害 | 865–873 | `fight` → `begin_turn` → `use_skill` → `basic_attack` → `get_target` → `take_hit` |
| 106 | 5反击! 1受到20点伤害 | 874–879 | `passive_counter` → `basic_attack` → `take_hit` |
| 107 | 1受到6点烧伤伤害，从烧伤中解除 | 880–886 | `end_turn` → `receive_damage(HURT_BURN)` → `report_death`/`finish_hurt` |
| 108 | 4 发起攻击，1受到1伤害 | 887–894 | `fight` → `begin_turn` → `use_skill` → `basic_attack` → `get_target` → `take_hit` |
| 109 | 1反击! 4受到40点伤害 | 895–900 | `passive_counter` → `basic_attack` → `take_hit` |
| 110 | 5 召唤 出了 幻魔 | 901–908 | `fight` → `begin_turn` → `use_skill` → `summon` → `add(summon_spawn)` |
| 111 | 1 发起攻击，4受到35伤害 | 909–917 | `fight` → `begin_turn` → `use_skill` → `basic_attack` → `get_target` → `take_hit` |
| 112 | 6 发起攻击，1受到1伤害 | 918–926 | `fight` → `begin_turn` → `use_skill` → `basic_attack` → `get_target` → `take_hit` |
| 113 | 幻魔 发起攻击，1受到1伤害 | 927–934 | `fight` → `begin_turn` → `use_skill` → `basic_attack` → `get_target` → `take_hit` |
| 114 | 6守护幻魔 | 935–937 | `get_guardian` → `take_guard_damage` → `add` |
| 115 | 1反击! 幻魔受到20点伤害 6受到20点伤害 | 938–949 | `passive_counter` → `basic_attack` → `take_hit` |
| 116 | 5 发起攻击，1受到15伤害 | 950–958 | `fight` → `begin_turn` → `use_skill` → `basic_attack` → `get_target` → `take_hit` |
| 117 | 1反击! 5受到32点伤害 | 959–964 | `passive_counter` → `basic_attack` → `take_hit` |
| 118 | 4 发起攻击，1受到1伤害 | 965–972 | `fight` → `begin_turn` → `use_skill` → `basic_attack` → `get_target` → `take_hit` |
| 119 | 幻魔 发起攻击，1受到1伤害 | 973–980 | `fight` → `begin_turn` → `use_skill` → `basic_attack` → `get_target` → `take_hit` |
| 120 | 1从铁壁中解除 | 981–983 | `begin_turn` → 强化层数递减 → `add` |
| 121 | 1 发起攻击，幻魔受到63伤害 | 984–992 | `fight` → `begin_turn` → `use_skill` → `basic_attack` → `get_target` → `take_hit` |
| 122 | 5 发起攻击，1受到60伤害 | 993–1001 | `fight` → `begin_turn` → `use_skill` → `basic_attack` → `get_target` → `take_hit` |
| 123 | 1消失了 | 1002–1002 | `report_death` → `dismiss_familiars`（若有） |
| 124 | 队伍2获胜 | 1003–1003 | `fight` → `alive_team` → `add(battle_end)` |

### 12.1 如何阅读这张表

例如第 1 行虽然前端只看到“4 发起攻击，1受到伤害”，但后台并不是一个函数完成：`fight` 找到行动者，`begin_turn` 处理回合开始状态，`use_skill` 决定普通攻击，`basic_attack` 选取目标并计算物理伤害，`take_hit` 统一检查招架/守护并扣血，最后由 `add` 把文字片段和 `alive`/状态快照送进命令流。前端把这一行动内的多个 Cmd 按 `newlineAfter=true` 拼成一行。

第 11、12 行展示了没有可见文字的后台 Cmd 如何归属于下一条战斗动作：第 11 行是投毒的回合开始、回魔和扣蓝，随后 `poison` 才产生可见文本；第 12 行是召唤的扣蓝、技能文本与 `summon_spawn`，前端据此把动态幻魔插入本体下方。第 23 行的“消失了”来自 `report_death`，第 24 行的胜负文本来自 `fight` 在 `alive_team()` 只剩一个队伍后写入的 `battle_end`。

### 12.2 本次真实输出的后台统计

这次运行的 `winnerTeamId`、`momentCount`、`executedActionCount` 和 Cmd 总数均以当前 WASM 结果为准。文档中的行号是前端 `groupCommandsIntoLines` 按 `newlineAfter=true` 分组后的编号，不是 C++ 的 `e` 下标；因此维护时应同时看“行号”和“Cmd 范围”。

## 13. 与当前版本同步的维护补充

本手册此前侧重于普通技能和前后台命令流。后续维护时，还必须遵守当前 Boss 机制的以下边界：`mili@!` 仅是输入识别标记，前端单位名、动态快照和日志均显示为 `mili`；K-2 的额外烧伤是一次真正的烧伤结算，会同时造成伤害并消耗目标一层；`world.execute(me);` 是结束技，其循环结束后 `world_execute_finished` 会阻止其他角色行动、回蓝和回合末 DOT。

张洋“偷”的复制结果必须在入队和执行时同时维持技能约束。若未来某个路径可复制 `world.execute`，必须转换为 `world.search(you);`；月之子、K-2、科学性实验魔女等偷取召唤以张洋为施放者时，仍必须经过 `stolen_skill_within_summon_cap` 与各召唤函数的自身上限检查。

对于新 Buff/Debuff、Base64 SVG 图标、前端命令动画、WASM 重建、GitHub 镜像同步、完整归档和故障排查，请以新增的 [`name_arena_update_manual.md`](./name_arena_update_manual.md) 为实际操作手册。本文件仍是 `name_arena.cpp` 变量、函数和调用顺序的逐函数参考。
