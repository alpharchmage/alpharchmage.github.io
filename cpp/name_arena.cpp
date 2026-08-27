/*
    名字竞技场 C++ 战斗核心。
    本文件负责：输入解析、确定性属性派生、战斗随机、技能结算、状态处理、渲染指令生成、JSON 序列化和 WASM 导出。
    React 只传入原始名单并按 Cmd 指令播放，所有数值变化均以本文件为唯一权威。
*/

#ifdef __EMSCRIPTEN__
#include<algorithm>
#include<array>
#include<cstdint>
#include<deque>
#include<sstream>
#include<string>
#include<vector>
#else
#include<bits/stdc++.h>
#endif

using namespace std;

// 统一使用 long long，避免生命、权重、哈希映射等数值在后续扩展时溢出。
#define int long long

const int MAXN = 100100; // 最大玩家数量，数组采用 1-index。
const int MAXS = 32; // 单名玩家最多可登记的技能槽数量，数组采用 1-index。

// 各技能函数的前置声明；技能函数指针将被保存进 Player::skill 与 Player::tmp。
void basic_attack(int x);
void stab(int x);
void critical_strike(int x);
void parry(int x);
void poison(int x);
void lifesteal_attack(int x);
void fireball(int x);
void thunder(int x);
void earthquake(int x);
void rage(int x);
void fast_action(int x);
void plague(int x);
void revive(int x);
void life_wheel(int x);
void ice(int x);
void heal_magic(int x);
void purify(int x);
void ironwall(int x);
void unfreeze(int x);
void guard(int x);
void summon(int x);
void string_skill(int x);
void magic_square(int x);
void double_island_milk(int x);
void summon_moon_child(int x);
void moon_chant(int x);
void summon_k2(int x);
void iron_blood_lotus(int x);
void summon_scientific_witch(int x);
void summon_lancelot_zero(int x);
void idle_skill(int x);
void lancelot_zero_attack(int x);
void summon_broken_lancelot_zero(int x);
void brew_rebirth_potion(int x);
void convert_broken_to_galahad(int x);
void galahad_laser_attack(int x);
void lament(int x);
void world_search(int x);
void world_execute(int x);
void witch_galahad_never_fall(int x);
void witch_talila_tulila(int x);
void witch_multiple_path(int x);
void check_scientific_witch_phases(int x);
bool spell(void (*f)(int));

// Player 保存一个名字在整场战斗中的全部数值、状态和技能队列。
struct Player
{
    string name; // 输入的角色名字，也是身份哈希和属性派生的来源。
    int no; // 玩家在本局数组 p 中的 1-index 座位编号。
    int id; // 所属队伍编号；连续名字同队，空行后队伍编号递增。
    int hp; // 当前生命值。
    int maxhp; // 最大生命值。
    int atk; // 物理攻击力，供普通攻击使用。
    int def; // 物理防御力，抵扣普通攻击基础伤害。
    int magic; // 当前魔力值，施法时消耗、每次行动前恢复。
    int maxmagic; // 当前版本统一为 200 的魔力上限。
    int mreg; // 每次行动前直接恢复的魔力数值，名字哈希派生于 20～40。
    int spd; // 速度，每个战斗时刻累加到 act。
    int satk; // 魔法攻击力，供雷击术、冰冻术、治愈魔法使用，并提高瘟疫的当前生命伤害倍率下限。
    int sdef; // 魔法防御力，抵扣敌方法术伤害。
    int iq; // 智慧属性，当前保留给后续技能扩展。
    int act; // 当前行动槽；超过 20000 时获得一次行动。
    bool alive; // 是否存活；死亡后不再累加行动槽、不能被选为技能执行者。
    int freeze; // 冻结层数；每层需要一次成功的解除冻结尝试消去。
    int freeze_int; // 冻结强度；每次冰冻术施加 1 强度，连续命中时与层数一同累加。
    int def_plus_int; // 防御强化强度，单位为普通防御力百分比；100 表示额外增加 100%。
    int def_plus_time; // 防御强化剩余层数；该角色每个自身回合开始时减 1，归零时解除。
    int def_down_int; // 防御削弱强度；每级令基础物防额外降低 5%，可与防御强化共同结算。
    int def_down_time; // 防御削弱剩余层数；目标每个自身回合开始时减 1，归零后清空削弱强度。
    int damage_down_int; // 伤害削弱强度；每级令该角色造成的最终伤害额外降低 5%。
    int damage_down_time; // 伤害削弱剩余层数；目标每个自身回合开始时减 1，归零后清空削弱强度。
    int square_int; // 方状态强度；当前固定为 1，存在期间令该角色的实际回魔量直接为 0。
    int square_time; // 方状态剩余层数；在目标每个自身回合结束时静默减 1，归零后清空强度。
    int blood_int; // 嗜血强度；吸血攻击会恢复伤害值的 5×嗜血强度%。
    int burn_int; // 烧伤强度；烧伤角色每次行动结束后直接失去该数值生命。
    int burn_time; // 烧伤剩余层数；每次烧伤角色行动结束后减 1，归零时解除。
    int posion_int; // 中毒强度；中毒角色每次毒素结算都会直接失去该数值生命。
    int poison_time; // 中毒层数；每次行动结束后按当前层数结算等次数的毒素伤害。
    int parry_time; // 招架剩余静默跳过回合数；存在时等待直接攻击触发反击，若连续两个自身回合未触发则归零。
    int lament_time; // 哀悼剩余层数；每次自身行动被无差别普通攻击替换后减 1，可能攻击任意存活单位且可命中自己。
    bool has_guard; // 是否拥有被动非法术守护；队友受到可转移伤害时可参与固定 30% 分摊判定。
    bool has_last_stand; // 是否拥有被动垂死挣扎；非生命之轮伤害后仍存活且生命低于 10% 时可触发一次。
    bool last_stand_used; // 本局是否已经发动过垂死挣扎；新对局创建角色时重置，避免重复触发。
    bool has_counter; // 是否拥有被动反击；受到带来源的伤害后有 25% 概率立即向来源发起一次普攻。
    bool has_devour; // 是否拥有被动吞噬；初始禁用法术，击杀敌人后继承其技能资格和权重并恢复最大生命的 50%。
    bool is_boss; // 是否为 Boss 单位；Boss 使用固定属性与专属技能池，不参与普通名字随机技能分配。
    bool is_mili; // 是否为 mili@! Boss；输入标记只用于识别，前端与战斗文本均显示 mili。
    int mili_skill_turn; // 保留 mili 的专属行动计数，供未来扩展轮换或阶段性技能时使用。
    int mili_milk_count; // mili 本局已饮用双岛牛奶数量，达到五瓶后立刻进入 world.execute 终局。
    bool mili_witch_summoned; // 科学性实验魔女的单局召唤资格；成功召唤一次后即永久关闭，即使魔女死亡也不再补召。
    int boss_string_count; // 张洋“偷”的充能计数；每成功释放一次串加一，到 3 时由下一次 Boss 行动消耗。
    bool boss_magic_ready; // 张洋完成一次偷后标记下一次空待命行动施放第三技能“魔”，施放后自动复位。
    bool is_familiar; // 是否为召唤出的幻魔眷属；眷属不参与初始名单，且只拥有普通攻击。
    bool is_moon_child; // 是否为 mili 召唤的月之子；其行动固定吟唱，死亡或随本体消失时会额外吟唱一次。
    bool is_k2; // 是否为 mili 召唤的 K-2；其行动固定铁血莲华，且承受带烧伤来源的伤害时降低 80%。
    bool is_scientific_witch; // 是否为科学性实验魔女；被召唤时立即生成兰斯洛特0号，后续当前版本只执行空技能。
    bool is_lancelot_zero; // 是否为兰斯洛特0号；每次行动走两种随机特殊普攻之一，文本仍显示为普通攻击。
    bool is_broken_lancelot_zero; // 是否为兰斯洛特0号死亡后生成的破烂随从；永久没有行动值且不会行动。
    bool has_rebirth_potion; // 科学性实验魔女是否已因兰斯洛特0号死亡获得“调制重生药水”文本技能。
    int rebirth_potion_count; // 科学性实验魔女已完成的调制次数，达到三次后触发加拉哈德1号复活。
    bool is_inert; // 是否为永久不参与行动值累加的静止单位；破烂的兰斯洛特0号使用此标记。
    bool is_galahad_one; // 是否为重生后的加拉哈德1号；由破烂随从原地转换而来。
    bool galahad_laser_mode; // 加拉哈德是否已被魔女强化为蓝色激光枪。
    bool galahad_multi_target; // 加拉哈德激光枪是否已获得三目标模式。
    bool witch_phase_50_used; // 魔女首次低于 50% 最大生命后是否已触发阶段技能。
    bool witch_phase_30_used; // 魔女首次低于 30% 最大生命后是否已触发阶段技能。
    bool witch_phase_10_used; // 魔女首次低于 10% 最大生命后是否已触发阶段技能。
    int owner; // 幻魔所属本体的座位编号；本体死亡后会立即令对应存活眷属死亡。
    int magic_vuln; // 常驻魔法易损等级；幻魔固定为 20，使所有魔法伤害按 200% 结算且不递减。
    int spd_up_int; // 速度强化强度，每级使基础速度额外增加 5%。
    int spd_up_time; // 速度强化剩余层数；该角色每个自身回合开始时减 1，归零时解除。
    int spd_down_int; // 速度削减强度，每级使基础速度额外减少 5%。
    int spd_down_time; // 速度削减剩余层数；该角色每个自身回合开始时减 1，归零时静默解除。
    int sn; // skill 中已登记技能的数量。
    array<void (*)(int) , MAXS> skill; // 可用技能槽；元素为“仅接收施法者编号”的函数指针。
    array<bool , MAXS> can; // 各技能槽是否具有释放资格的显式 bool 标记。
    array<int , MAXS> w; // 各特殊技能槽的固定共享抽取权重，法术与非法术均使用，范围通常为 0～10。
    deque<void (*)(int)> tmp; // 当前待执行技能队列；束缚技能会从队首优先插入。
    deque<bool> tmp_free; // 与 tmp 一一对应；true 表示张洋偷取的技能，执行时不检查也不扣除魔力。
};

// Cmd 是 C++ 发送给前端的最小渲染指令；一条战斗日志可由多条 Cmd 拼接而成。
struct Cmd
{
    int a; // 指令来源玩家的座位编号；纯文本系统事件为 0。
    int b; // 指令目标玩家的座位编号；纯文本系统事件为 0。
    int sid; // 技能编号：1 普攻、2 雷击术、3 冰冻术、4 状态/治愈、5 铁壁、6 地裂术、7 疾走术、8 戳刺、9 吸血攻击、10 火球术/烧伤、11 净化、12 投毒/中毒、13 会心一击、14 招架/反击、15 快速行动、16 瘟疫、17 复苏术、18 守护、19 生命之轮、20 垂死挣扎、21 反击、22 吞噬、23 召唤、24 张洋的串、25 张洋的偷、26 张洋的魔、34 加拉哈德1号的斩击。
    int val; // 本次变化量，例如伤害值、治疗值、消耗魔力或冻结层数。
    int after; // 本次结算后的关键数值，前端据此更新血量或魔力。
    int col; // 前端文字颜色编号。
    string ani; // 前端动画类型编号，例如 thunder_damage、heal、freeze_apply。
    string str; // 当前 Cmd 输出的文本片段。
    bool nl; // 当前文本片段后是否换行，false 时继续和下一条 Cmd 拼接。
    int freeze; // 当前目标的冻结层数快照。
    int freeze_int; // 当前目标的冻结强度快照。
    int ironwall; // 当前目标的铁壁剩余层数快照。
    int ironwall_int; // 当前目标的铁壁强化强度快照。
    int def_down; // 当前目标的防御削弱剩余层数快照。
    int def_down_int; // 当前目标的防御削弱强度快照。
    int damage_down; // 当前目标的伤害削弱剩余层数快照。
    int damage_down_int; // 当前目标的伤害削弱强度快照。
    int square; // 当前目标的方状态剩余层数快照。
    int square_int; // 当前目标的方状态强度快照。
    int spd_up_int; // 当前目标的速度强化强度快照。
    int spd_up_time; // 当前目标的速度强化剩余层数快照。
    int spd_down_int; // 当前目标的速度削减强度快照。
    int spd_down_time; // 当前目标的速度削减剩余层数快照。
    int burn_int; // 当前目标的烧伤强度快照。
    int burn_time; // 当前目标的烧伤剩余层数快照。
    int posion_int; // 当前目标的中毒强度快照。
    int poison_time; // 当前目标的中毒剩余层数快照。
    int parry; // 当前目标的招架层数快照。
    int lament; // 当前目标的哀悼剩余层数快照。
    bool alive; // 当前目标在该命令结算后的存活状态快照。
    string player_name; // 当前目标的动态名称快照；用于破烂随从转换为新单位时即时改名。
    int player_maxhp; // 当前目标的动态最大生命快照。
    int player_atk; // 当前目标的动态物攻快照。
    int player_def; // 当前目标的动态物防快照。
    int player_satk; // 当前目标的动态魔攻快照。
    int player_sdef; // 当前目标的动态魔防快照。
    int player_spd; // 当前目标的动态速度快照。
    bool has_player_snapshot; // 是否携带动态名称与属性快照。
};

array<Player , MAXN> p; // 本局玩家表，p[1] 到 p[n] 有效。
array<int , MAXN> vis; // 队伍去重访问标记，用于统计存活队伍数量。
array<bool , MAXN> revive_used; // 各队本局是否已成功施放过复苏术；队伍编号作为下标。
enum HurtType { HURT_DIRECT , HURT_PLAGUE , HURT_BURN , HURT_POISON , HURT_LIFE_WHEEL , HURT_COUNTER }; // 统一伤害管线的来源类型；生命之轮交换和反击伤害均排除被动反击递归。
vector<Cmd> e = vector<Cmd>(1); // 全部渲染指令；首个空元素保证有效指令从下标 1 开始。
int n = 0; // 本局玩家数量。
int tag = 0; // vis 的访问轮次标签，避免每次统计时清空整张 vis。
uint32_t seed = 0; // 技能伤害、解冻成功率、雷击段数等战斗随机源。
uint32_t tar_seed = 0; // 随机目标和法术加权抽取使用的独立随机源。
int win = 0; // 获胜队伍编号；未决出时为 0。
int tim = 0; // 已执行的战斗时刻数量。
int cnt = 0; // 已执行的角色行动次数。
int parry_source = 0; // 当前直接攻击动作的来源座位；雷击多段格挡期间用于识别同一次攻击。
int parry_target = 0; // 当前被招架连续格挡的目标座位。
int parry_sid = 0; // 当前直接攻击动作的技能编号。
bool world_execute_finished = false; // world.execute(me); 是结束技；完成后立即锁定战斗主循环，禁止任何后续行动或回合末结算。
int get_maxhp(int x); // 返回角色当前最大生命，包含所有属性修正。
int get_atk(int x); // 返回角色当前有效物攻。
int get_def(int x); // 返回角色当前有效物防。
int get_spd(int x); // 返回角色当前有效速度。
int get_satk(int x); // 返回角色当前有效魔攻。
int get_sdef(int x); // 返回角色当前有效魔防。
int take_hit(int x , int it , int d , int sid , const string& ani , const string& damage_ani , const string& damage_suffix = "伤害" , HurtType type = HURT_DIRECT , bool passive_counter_attack = false , bool end_line = true , int* dealt = nullptr); // 所有直接攻击统一使用的受击前判定入口；dealt 可取得本次实际造成的总伤害。
void passive_counter(int x , int it , bool end_line = true); // 被动反击：x 受伤后对来源 it 进行一次不可递归的普攻。
void report_death(int source , int it , int sid); // 统一输出死亡日志、处理绑定眷属死亡，并在存在攻击来源时触发吞噬。
void steal_skill(int x); // 张洋在累计三次串后偷取敌方待命/即时可释放技能，并夺取行动值。
int special_weight(int x , int i); // 技能即时生成与常规行动共用的特殊技能权重函数。
int special_chance(int x); // 技能即时生成与常规行动共用的特殊技能概率函数。
void galahad_one_attack(int x); // 加拉哈德1号的唯一行动：红色斩击并等额治疗所属魔女。

// 对字符串计算稳定的 FNV-1a 64 位哈希；同名永远得到相同身份种子。
uint64_t get_hash(const string& s)
{
    uint64_t h = 1469598103934665603ULL; // FNV-1a 初始常量。

    for(unsigned char c : s) // c 为名字 UTF-8 字节，逐字节参与哈希。
    {
        h ^= (uint64_t)c;
        h *= 1099511628211ULL;
    }

    return h;
}

// 基于传入状态 x 的 SplitMix64 单步输出；返回值用于确定性属性派生。
uint64_t get_rand64(uint64_t& x)
{
    x += 0x9e3779b97f4a7c15ULL; // 推进调用者持有的 64 位状态。
    uint64_t z = x; // z 为当前轮混洗中的局部工作值。
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

// 在闭区间 [l,r] 内从身份种子 x 无偏映射出一个均匀整数。
int get_val(uint64_t& x , int l , int r)
{
    uint64_t m = (uint64_t)(r - l + 1); // 目标区间长度。
    uint64_t lim = (uint64_t)(0 - m) % m; // 低端拒绝阈值，用于消除取模偏差。
    uint64_t v = 0; // 本轮候选随机数。

    do
    {
        v = get_rand64(x);
    }
    while(v < lim);

    return l + (int)(v % m);
}

// 以 12 个均匀样本求和近似正态分布，再裁剪到闭区间 [l,r]。
int get_normal(uint64_t& x , int l , int r)
{
    int sum = 0; // 12 个 0～1000 均匀样本的总和，中心期望为 6000。

    for(int i = 1;i <= 12;++ i) sum += get_val(x , 0 , 1000);

    int mid = (l + r) / 2; // 目标范围的中心值。
    int div = max(1LL , 5000 / (r - l)); // 将偏离中心的样本和缩放至目标范围。
    return min(r , max(l , mid + (sum - 6000) / div));
}

// Xorshift32 战斗随机；同一完整名单和顺序会得到可复现的结算过程。
int rnd()
{
    seed ^= seed << 13;
    seed ^= seed >> 17;
    seed ^= seed << 5;

    if(seed == 0) seed = 123456789U;

    return (int)seed;
}

// 独立的 32 位混洗随机，用于目标选择和加权法术抽取，避免同伤害随机耦合。
uint32_t rnd_tar()
{
    tar_seed += 0x9e3779b9U;
    uint32_t z = tar_seed;
    z = (z ^ (z >> 16)) * 0x85ebca6bU;
    z = (z ^ (z >> 13)) * 0xc2b2ae35U;
    return z ^ (z >> 16);
}

// 在 1～m 内无偏返回一个随机编号；m 非正时返回 0。
int rnd_id(int m)
{
    if(m <= 0) return 0;

    const uint64_t lim = (1ULL << 32) - ((1ULL << 32) % (uint64_t)m); // 可整除 m 的最大 32 位前缀。
    uint64_t v = 0; // 本轮从 rnd_tar 取得的候选值。

    do
    {
        v = (uint64_t)rnd_tar();
    }
    while(v >= lim);

    return (int)(v % (uint64_t)m) + 1;
}

// 将字符串转义为可安全嵌入 JSON 字符串字段的文本。
string esc(const string& s)
{
    string t; // 转义后的 JSON 片段。

    for(unsigned char c : s)
    {
        if(c == '"') t += "\\\"";
        else if(c == '\\') t += "\\\\";
        else if(c == '\n') t += "\\n";
        else if(c == '\r') t += "\\r";
        else if(c == '\t') t += "\\t";
        else if(c < 32) t += " ";
        else t += (char)c;
    }

    return t;
}

// 文本/动画事件统一出口：组装一条 Cmd 并压入事件流，React 只读取此事件流播放，不自行计算战斗数值。
// a 为来源座位（0 表示无来源），b 为目标座位（0 表示无目标）；sid 是技能编号，供前端识别本次事件所属技能。
// val 是本片段的实际扣血、回血或魔力数值；after 是命令执行后的目标生命/资源读数，前端以它终结条形动画。
// col 是文本色号；ani 是前端动画键；str 是当前文本片段；nl 为真时结束当前日志行，否则与下一 Cmd 拼接。
// 写入前会把目标 b 此刻的全部可显示状态（冻结、强化、持续伤害、招架、存活）快照到 Cmd，避免前端因后续命令而猜错历史状态。
void add(int a , int b , int sid , int val , int after , int col , const string& ani , const string& str , bool nl)
{
    Cmd x; // 待写入事件流的局部渲染指令。
    x.a = a;
    x.b = b;
    x.sid = sid;
    x.val = val;
    x.after = after;
    x.col = col;
    x.ani = ani;
    x.str = str;
    x.nl = nl;
    x.freeze = 0;
    x.freeze_int = 0;
    x.ironwall = 0;
    x.ironwall_int = 0;
    x.def_down = 0;
    x.def_down_int = 0;
    x.damage_down = 0;
    x.damage_down_int = 0;
    x.square = 0;
    x.square_int = 0;
    x.spd_up_int = 0;
    x.spd_up_time = 0;
    x.spd_down_int = 0;
    x.spd_down_time = 0;
    x.burn_int = 0;
    x.burn_time = 0;
    x.posion_int = 0;
    x.poison_time = 0;
    x.parry = 0;
    x.lament = 0;
    x.alive = false;
    x.player_name = "";
    x.player_maxhp = 0;
    x.player_atk = 0;
    x.player_def = 0;
    x.player_satk = 0;
    x.player_sdef = 0;
    x.player_spd = 0;
    x.has_player_snapshot = false;

    if(1 <= b && b <= n)
    {
        x.freeze = p[b].freeze;
        x.freeze_int = p[b].freeze_int;
        x.ironwall = p[b].def_plus_time;
        x.ironwall_int = p[b].def_plus_int;
        x.def_down = p[b].def_down_time;
        x.def_down_int = p[b].def_down_int;
        x.damage_down = p[b].damage_down_time;
        x.damage_down_int = p[b].damage_down_int;
        x.square = p[b].square_time;
        x.square_int = p[b].square_int;
        x.spd_up_int = p[b].spd_up_int;
        x.spd_up_time = p[b].spd_up_time;
        x.spd_down_int = p[b].spd_down_int;
        x.spd_down_time = p[b].spd_down_time;
        x.burn_int = p[b].burn_int;
    x.burn_time = p[b].burn_time;
    x.posion_int = p[b].posion_int;
    x.poison_time = p[b].poison_time;
    x.parry = p[b].parry_time;
    x.lament = p[b].lament_time;
    x.alive = p[b].alive;
        x.player_name = p[b].name;
        x.player_maxhp = get_maxhp(b);
        x.player_atk = get_atk(b);
        x.player_def = get_def(b);
        x.player_satk = get_satk(b);
        x.player_sdef = get_sdef(b);
        x.player_spd = get_spd(b);
        x.has_player_snapshot = true;
    }

    e.push_back(x);
}

// 生成一条无来源、无目标的独立纯文本提示。
// str 是完整提示文字，col 是其颜色；该函数固定换行，因此适合回合开始、状态解除等不涉及单位条形动画的消息。
void tip(const string& str , int col)
{
    add(0 , 0 , 0 , 0 , 0 , col , "text" , str , true);
}

// 以下函数统一返回结算时应使用的实际属性；基础字段保持名字派生原值，状态只在此处叠加。
int get_maxhp(int x)
{
    return p[x].maxhp;
}

int get_atk(int x)
{
    return p[x].atk;
}

int get_def(int x)
{
    int rate = max(0LL , 100 + p[x].def_plus_int - p[x].def_down_int * 5);
    return p[x].def * rate / 100;
}

// 统一应用“造成伤害削弱”：每级减少来源 5% 最终伤害；16 级时只保留 20%。
// 该函数只由有来源的直接攻击与反击在最终伤害完成后调用；瘟疫、烧伤和中毒均不调用它。
int reduce_outgoing_damage(int x , int d)
{
    int rate = max(0LL , 100 - p[x].damage_down_int * 5);
    return d * rate / 100;
}

int get_maxmagic(int x)
{
    return p[x].maxmagic;
}

int get_mreg(int x)
{
    if(p[x].square_time > 0) return 0;
    return p[x].mreg;
}

int get_spd(int x)
{
    if(p[x].is_inert) return 0;
    return max(1LL , p[x].spd * (100 + p[x].spd_up_int * 5 - p[x].spd_down_int * 5) / 100);
}

int get_satk(int x)
{
    return p[x].satk;
}

int get_sdef(int x)
{
    return p[x].sdef;
}

// 法术伤害统一计算器：x 为施法者，it 为受法术伤害的目标，mul 为魔攻百分比倍率。
// 先按 max(0, 魔攻 * 倍率 - 魔防) 得到基础值，再施加 90%～110% 的整数波动；结果允许为 0，保留低魔攻对高魔防无伤害的规则。
// 幻魔的 magic_vuln 为常驻易损标记，任何经此函数产生的魔法伤害最终翻倍；物理攻击和持续物理伤害不会经过这里。
int magic_damage(int x , int it , int mul)
{
    int d = max(0LL , get_satk(x) * mul / 100 - get_sdef(it));
    int f = 900 + rnd() % 201;
    d = max(0LL , (d * f + 500) / 1000);
    if(p[it].magic_vuln > 0) d *= 2;
    return d;
}

int get_iq(int x)
{
    return p[x].iq;
}

// 根据名字 s 创建一名玩家；id 是输入解析得到的队伍编号；long_battle 决定是否保留完整生命。
void make_player(const string& s , int id , bool long_battle)
{
    n++; // 分配新的 1-index 玩家座位。
    uint64_t h = get_hash("name-arena:" + s); // 身份属性和固定技能权重的确定性随机状态。
    uint64_t fh = get_hash("spell-arena:" + s); // 法术技能资格的独立名字哈希状态；每项法术直接进行 50% 判定。
    uint64_t qh = get_hash("fast-action-arena:" + s); // 快速行动的独立法术资格哈希，不扰动既有法术资格。
    uint64_t ph = get_hash("plague-arena:" + s); // 瘟疫的独立法术资格哈希，不扰动既有法术资格。
    uint64_t rh = get_hash("revive-arena:" + s); // 复苏术的独立法术资格哈希，不扰动既有法术资格。
    uint64_t lh = get_hash("life-wheel-arena:" + s); // 生命之轮的独立法术资格哈希，不扰动既有法术资格。
    uint64_t uh = get_hash("summon-arena:" + s); // 召唤术的独立法术资格哈希，不扰动既有技能资格。
    uint64_t dh = get_hash("last-stand-arena:" + s); // 垂死抵抗的独立被动资格哈希，不扰动其他技能资格。
    uint64_t ch = get_hash("counter-arena:" + s); // 反击的独立被动资格哈希，不扰动其他技能资格。
    uint64_t eh = get_hash("devour-arena:" + s); // 吞噬的独立被动资格哈希，不扰动其他技能资格。
    uint64_t nh = get_hash("nonspell-arena:" + s); // 非法术技能资格的独立名字哈希状态，不扰动既有属性、法术资格和权重。

    p[n] = Player();
    p[n].skill.fill(nullptr);
    p[n].can.fill(false);
    p[n].w.fill(0);
    p[n].tmp.clear();
    p[n].tmp_free.clear();
    bool is_zhang_yang = s == "tommy@!"; // 输入标记只用于识别张洋 Boss，前端与日志统一显示中文名“张洋”。
    bool is_mili = s == "mili@!"; // 输入标记只用于识别 mili Boss；前端与日志均显示 mili。
    p[n].name = is_zhang_yang ? "张洋" : is_mili ? "mili" : s;
    p[n].no = n;
    p[n].id = id;
    p[n].maxhp = is_mili ? 3000 : is_zhang_yang ? 2500 : get_val(h , 400 , 800);
    if(long_battle == false) p[n].maxhp /= 2; // 短对局将普通角色派生生命、张洋固定 2500 生命一并减半。
    p[n].hp = p[n].maxhp;
    p[n].atk = is_mili ? 200 : is_zhang_yang ? 100 : get_val(h , 70 , 130);
    p[n].def = is_mili ? 45 : is_zhang_yang ? 60 : get_val(h , 30 , 60);
    p[n].spd = is_mili ? 2000 : is_zhang_yang ? 2000 : get_val(h , 800 , 1200);
    p[n].satk = is_mili ? 200 : is_zhang_yang ? 180 : get_val(h , 100 , 200);
    p[n].sdef = is_mili ? 50 : is_zhang_yang ? 50 : get_val(h , 0 , 100);
    p[n].iq = is_mili ? 0 : is_zhang_yang ? 200 : get_val(h , 100 , 200);
    uint64_t mh = get_hash("mana-arena:" + s); // 独立于基础属性的回魔正态分布种子。
    p[n].mreg = is_mili ? 0 : is_zhang_yang ? 100 : get_normal(mh , 20 , 40);
    p[n].maxmagic = 200;
    p[n].magic = 0;
    p[n].freeze = 0;
    p[n].freeze_int = 0;
    p[n].def_plus_int = 0;
    p[n].def_plus_time = 0;
    p[n].def_down_int = 0;
    p[n].def_down_time = 0;
    p[n].damage_down_int = 0;
    p[n].damage_down_time = 0;
    p[n].square_int = 0;
    p[n].square_time = 0;
    p[n].blood_int = 10;
    p[n].burn_int = 0;
    p[n].burn_time = 0;
    p[n].posion_int = 0;
    p[n].poison_time = 0;
    p[n].parry_time = 0;
    p[n].lament_time = 0;
    p[n].has_guard = false;
    p[n].has_last_stand = get_val(dh , 0 , 99) < 15;
    p[n].last_stand_used = false;
    p[n].has_counter = get_val(ch , 0 , 99) < 40;
    p[n].has_devour = get_val(eh , 0 , 99) < 10;
    p[n].is_boss = is_zhang_yang || is_mili;
    p[n].is_mili = is_mili;
    p[n].mili_skill_turn = 0;
    p[n].mili_milk_count = 0;
    p[n].mili_witch_summoned = false;
    p[n].boss_string_count = 0;
    p[n].boss_magic_ready = false;
    p[n].is_familiar = false;
    p[n].is_moon_child = false;
    p[n].is_k2 = false;
    p[n].is_scientific_witch = false;
    p[n].is_lancelot_zero = false;
    p[n].is_broken_lancelot_zero = false;
    p[n].has_rebirth_potion = false;
    p[n].is_inert = false;
    p[n].owner = 0;
    p[n].magic_vuln = 0;
    p[n].spd_up_int = 0;
    p[n].spd_up_time = 0;
    p[n].spd_down_int = 0;
    p[n].spd_down_time = 0;
    p[n].act = 0;
    p[n].alive = true;

    if(p[n].is_boss)
    {
        // Boss 不继承普通角色的随机技能、被动或吞噬规则；已公布的“串”会由 use_skill 强制执行。
        p[n].has_guard = false;
        p[n].has_last_stand = false;
        p[n].has_counter = false;
        p[n].has_devour = false;
        p[n].sn = is_mili ? 4 : 1;
        p[n].skill[1] = is_mili ? double_island_milk : string_skill;
        p[n].can[1] = true;
        p[n].w[1] = 1;
        if(is_mili)
        {
            p[n].skill[2] = summon_moon_child;
            p[n].can[2] = true;
            p[n].w[2] = 15;
            p[n].skill[3] = summon_scientific_witch;
            p[n].can[3] = true;
            p[n].w[3] = 10;
            p[n].skill[4] = lament;
            p[n].can[4] = true;
            p[n].w[4] = 10;
        }
        return;
    }
    array<int , 7> nonspell_slot; // 当前已登记的非法术技能槽编号；六种非法术候选保持 1-index。
    int nonspell_num = 0; // 已登记的非法术技能种类数量。
    p[n].sn = 1;
    p[n].skill[1] = basic_attack;
    p[n].can[1] = true;
    p[n].sn++;
    p[n].skill[p[n].sn] = thunder;
    p[n].can[p[n].sn] = get_satk(n) >= 150 && get_val(fh , 0 , 1) == 1;
    p[n].w[p[n].sn] = get_val(h , 0 , 10);
    p[n].sn++;
    p[n].skill[p[n].sn] = earthquake;
    p[n].can[p[n].sn] = get_val(fh , 0 , 1) == 1;
    p[n].w[p[n].sn] = get_val(h , 0 , 10);
    p[n].sn++;
    p[n].skill[p[n].sn] = rage;
    p[n].can[p[n].sn] = get_val(fh , 0 , 1) == 1;
    p[n].w[p[n].sn] = get_val(h , 0 , 10);
    p[n].sn++;
    p[n].skill[p[n].sn] = ice;
    p[n].can[p[n].sn] = get_satk(n) >= 150 && get_val(fh , 0 , 1) == 1;
    p[n].w[p[n].sn] = get_val(h , 0 , 10);
    p[n].sn++;
    p[n].skill[p[n].sn] = heal_magic;
    p[n].can[p[n].sn] = get_val(fh , 0 , 1) == 1;
    p[n].w[p[n].sn] = get_val(h , 0 , 10);
    p[n].sn++;
    p[n].skill[p[n].sn] = purify;
    p[n].can[p[n].sn] = get_val(fh , 0 , 1) == 1;
    p[n].w[p[n].sn] = get_val(h , 0 , 10);
    p[n].sn++;
    p[n].skill[p[n].sn] = ironwall;
    p[n].can[p[n].sn] = get_val(fh , 0 , 1) == 1;
    p[n].w[p[n].sn] = get_val(h , 0 , 10);
    p[n].sn++;
    p[n].skill[p[n].sn] = stab;
    nonspell_num++;
    nonspell_slot[nonspell_num] = p[n].sn;
    p[n].can[p[n].sn] = false;
    p[n].w[p[n].sn] = get_val(h , 0 , 10);
    p[n].sn++;
    p[n].skill[p[n].sn] = lifesteal_attack;
    nonspell_num++;
    nonspell_slot[nonspell_num] = p[n].sn;
    p[n].can[p[n].sn] = false;
    p[n].w[p[n].sn] = get_val(h , 0 , 10);
    p[n].sn++;
    p[n].skill[p[n].sn] = poison;
    nonspell_num++;
    nonspell_slot[nonspell_num] = p[n].sn;
    p[n].can[p[n].sn] = false;
    p[n].w[p[n].sn] = get_val(h , 0 , 10);
    p[n].sn++;
    p[n].skill[p[n].sn] = critical_strike;
    nonspell_num++;
    nonspell_slot[nonspell_num] = p[n].sn;
    p[n].can[p[n].sn] = false;
    p[n].w[p[n].sn] = get_val(h , 0 , 10);
    p[n].sn++;
    p[n].skill[p[n].sn] = parry;
    nonspell_num++;
    nonspell_slot[nonspell_num] = p[n].sn;
    p[n].can[p[n].sn] = false;
    p[n].w[p[n].sn] = get_val(h , 0 , 10);
    p[n].sn++;
    p[n].skill[p[n].sn] = guard;
    nonspell_num++;
    nonspell_slot[nonspell_num] = p[n].sn;
    int guard_slot = p[n].sn; // 守护仅用于受击时的被动判定，不进入主动特殊技能池。
    p[n].can[p[n].sn] = false;
    p[n].w[p[n].sn] = 0;
    int nonspell_have = 0; // 当前角色已获得的非法术数量。
    int nonspell_chance = 4000; // 首项非法术的 40.00% 获取概率，单位为万分之一。

    for(int i = 1;i <= nonspell_num && nonspell_have < 5;++ i)
    {
        if(get_val(nh , 0 , 9999) >= nonspell_chance) continue;
        p[n].can[nonspell_slot[i]] = true;
        nonspell_have++;
        nonspell_chance = nonspell_chance * 30 / 100; // 每获得一项，后续非法术概率乘以 30%。
    }

    while(nonspell_have < 2 && nonspell_have < nonspell_num)
    {
        vector<int> candidate;

        for(int i = 1;i <= nonspell_num;++ i) if(p[n].can[nonspell_slot[i]] == false) candidate.push_back(nonspell_slot[i]);
        if(candidate.empty()) break;
        int slot = candidate[get_val(nh , 0 , (int)candidate.size() - 1)]; // 保底从尚未获得的非法术中随机选择，确保每名角色至少拥有两项。
        p[n].can[slot] = true;
        nonspell_have++;
    }
    p[n].has_guard = p[n].can[guard_slot];
    p[n].sn++;
    p[n].skill[p[n].sn] = fireball;
    p[n].can[p[n].sn] = get_val(fh , 0 , 1) == 1;
    p[n].w[p[n].sn] = get_val(h , 0 , 10);
    p[n].sn++;
    p[n].skill[p[n].sn] = plague;
    p[n].can[p[n].sn] = get_val(ph , 0 , 1) == 1;
    p[n].w[p[n].sn] = get_val(h , 0 , 10);
    p[n].sn++;
    p[n].skill[p[n].sn] = fast_action;
    p[n].can[p[n].sn] = get_val(qh , 0 , 1) == 1;
    p[n].w[p[n].sn] = get_val(h , 0 , 10);
    p[n].sn++;
    p[n].skill[p[n].sn] = revive;
    p[n].can[p[n].sn] = get_val(rh , 0 , 1) == 1;
    p[n].w[p[n].sn] = get_val(h , 0 , 10);
    p[n].sn++;
    p[n].skill[p[n].sn] = life_wheel;
    p[n].can[p[n].sn] = get_val(lh , 0 , 1) == 1;
    p[n].w[p[n].sn] = get_val(h , 0 , 10);
    p[n].sn++;
    p[n].skill[p[n].sn] = summon;
    p[n].can[p[n].sn] = get_val(uh , 0 , 1) == 1;
    p[n].w[p[n].sn] = get_val(h , 0 , 10);

    if(p[n].has_devour)
    {
        for(int i = 1;i <= p[n].sn;++ i) if(spell(p[n].skill[i])) p[n].can[i] = false;
    }
}

// 判断一行是否只包含空格或制表符；这类行用于分隔队伍。
bool empty_line(const string& s)
{
    for(char c : s)
    {
        if(c != ' ' && c != '\t') return false;
    }

    return true;
}

// 重置本局状态并解析原始名单；连续非空行同队，至少一个空行后进入下一队。
void read_input(const string& s , bool long_battle)
{
    for(int i = 1;i <= n;++ i)
    {
        p[i].tmp.clear();
        p[i].tmp_free.clear();
    }

    n = 0;
    e = vector<Cmd>(1);
    win = 0;
    tim = 0;
    cnt = 0;
    world_execute_finished = false;
    revive_used.fill(false);

    string t; // 去除 Windows 回车符后的输入文本。

    for(char c : s)
    {
        if(c != '\r') t += c;
    }

    istringstream in(t); // 按行读取处理后的名单文本。
    string str; // 当前读出的单行名字。
    int id = 1; // 正在写入的队伍编号。
    bool has = false; // 当前队伍是否已写入至少一名成员。

    while(std::getline(in , str))
    {
        if(empty_line(str))
        {
            if(has)
            {
                id++;
                has = false;
            }

            continue;
        }

        has = true;
        make_player(str , id , long_battle);
    }

    uint64_t h = get_hash("battle:" + s); // 完整名单顺序种子，只影响本局发展而非名字自身属性。
    seed = (uint32_t)(h ^ (h >> 32));

    if(seed == 0) seed = 123456789U;

    tar_seed = seed ^ 0x51ed270bU;

    if(tar_seed == 0) tar_seed = 998244353U;
}

// 统计仍有存活成员的队伍数；仅剩一队时同步写入 win。
int alive_team()
{
    tag++; // 开启一轮新的 vis 标记，避免清空 vis 数组。
    int ans = 0; // 当前发现的存活队伍数量。
    int last = 0; // 最近发现的存活队伍编号；唯一存活时即为胜者。

    for(int i = 1;i <= n;++ i)
    {
        if(p[i].alive == false) continue;

        if(vis[p[i].id] != tag)
        {
            vis[p[i].id] = tag;
            ans++;
            last = p[i].id;
        }
    }

    if(ans == 1) win = last;

    return ans;
}

// 为施法者 x 从所有存活敌方成员中等概率随机选择一个目标座位。
int get_target(int x)
{
    int sum = 0; // 可被选中的存活敌人数量。

    for(int it = 1;it <= n;++ it)
    {
        if(p[it].alive && p[it].id != p[x].id) sum++;
    }

    if(sum == 0) return 0;

    int k = rnd_id(sum); // 目标在“存活敌人列表”中的随机序号。

    for(int it = 1;it <= n;++ it)
    {
        if(p[it].alive && p[it].id != p[x].id)
        {
            k--;

            if(k == 0) return it;
        }
    }

    return 0;
}

// 哀悼造成的无差别攻击目标：从全部存活单位中均匀抽取，施法者本人和同队成员也都是合法目标。
int get_lament_target(int x)
{
    int sum = 0;
    for(int it = 1;it <= n;++ it) if(p[it].alive) sum++;
    if(sum == 0) return 0;

    int k = rnd_id(sum);
    for(int it = 1;it <= n;++ it)
    {
        if(p[it].alive == false) continue;
        if(--k == 0) return it;
    }
    return 0;
}

// 瘟疫目标：智慧超过 125 时锁定当前生命值最高的存活敌人；其余情况沿用均匀随机敌人选择。
int get_plague_target(int x)
{
    if(get_iq(x) <= 125) return get_target(x);

    int ans = 0;

    for(int it = 1;it <= n;++ it)
    {
        if(p[it].alive == false || p[it].id == p[x].id) continue;
        if(ans == 0 || p[it].hp > p[ans].hp) ans = it;
    }

    return ans;
}

// 生命之轮固定选择当前生命值最高的存活敌方；生命相同时保留座位编号更小者。
int get_life_wheel_target(int x)
{
    int ans = 0;

    for(int it = 1;it <= n;++ it)
    {
        if(p[it].alive == false || p[it].id == p[x].id) continue;
        if(ans == 0 || p[it].hp > p[ans].hp) ans = it;
    }

    return ans;
}

// 判断 it 是否为施法者 x 同队、存活且尚未满血的可受治疗目标。
bool can_heal_target(int x , int it)
{
    return p[it].alive && p[it].id == p[x].id && p[it].hp < get_maxhp(it);
}

// 判断施法者 x 的队伍中是否至少存在一名可受治疗成员；资格检查不消耗随机数。
bool has_heal_target(int x)
{
    for(int it = 1;it <= n;++ it)
    {
        if(can_heal_target(x , it)) return true;
    }

    return false;
}

// 判断 it 是否为施法者 x 的同队死亡成员；复苏术只对这类目标有效。
bool can_revive_target(int x , int it)
{
    return p[it].alive == false && p[it].id == p[x].id;
}

// 本队尚未用过复苏术且存在死亡队友时，复苏术才可以进入本次特殊技能候选池。
bool has_revive_target(int x)
{
    if(revive_used[p[x].id]) return false;

    for(int it = 1;it <= n;++ it)
    {
        if(can_revive_target(x , it)) return true;
    }

    return false;
}

// 从同队死亡成员中等概率选择一名复活；不存在时返回 0。
int get_revive_target(int x)
{
    int sum = 0;

    for(int it = 1;it <= n;++ it) if(can_revive_target(x , it)) sum++;
    if(sum == 0) return 0;

    int k = rnd_id(sum);

    for(int it = 1;it <= n;++ it)
    {
        if(can_revive_target(x , it) == false) continue;
        k--;
        if(k == 0) return it;
    }

    return 0;
}

// 选择治愈目标：智力高于 100 时选取同队生命最低者，否则等概率随机选择未满血友方。
int get_heal_target(int x)
{
    if(get_iq(x) > 100)
    {
        int ans = 0; // 当前生命最低的可受治疗友方座位。

        for(int it = 1;it <= n;++ it)
        {
            if(can_heal_target(x , it) == false) continue;
            if(ans == 0 || p[it].hp < p[ans].hp) ans = it;
        }

        return ans;
    }

    int sum = 0; // 可被随机选中的同队未满血成员数量。

    for(int it = 1;it <= n;++ it)
    {
        if(can_heal_target(x , it)) sum++;
    }

    if(sum == 0) return 0;

    int k = rnd_id(sum);

    for(int it = 1;it <= n;++ it)
    {
        if(can_heal_target(x , it) == false) continue;
        k--;
        if(k == 0) return it;
    }

    return 0;
}

// 判断 it 是否为施法者 x 同队、存活且具有至少一种可净化负面状态的友方目标。
bool can_purify_target(int x , int it)
{
    if(p[it].alive == false || p[it].id != p[x].id) return false;
    return p[it].freeze > 0 || p[it].spd_down_time > 0 || p[it].burn_time > 0 || p[it].poison_time > 0;
}

// 判断队伍中是否存在可被净化的友方；净化无目标时不会参加特殊技能抽取。
bool has_purify_target(int x)
{
    for(int it = 1;it <= n;++ it)
    {
        if(can_purify_target(x , it)) return true;
    }

    return false;
}

// 选择净化目标：高智慧优先处理负面层数总和最高的友方，否则等概率随机选择。
int get_purify_target(int x)
{
    if(get_iq(x) > 100)
    {
        int ans = 0;

        for(int it = 1;it <= n;++ it)
        {
            if(can_purify_target(x , it) == false) continue;
            int now = p[it].freeze + p[it].spd_down_time + p[it].burn_time + p[it].poison_time;
            int before = ans == 0 ? -1 : p[ans].freeze + p[ans].spd_down_time + p[ans].burn_time + p[ans].poison_time;
            if(now > before) ans = it;
        }

        return ans;
    }

    int sum = 0;

    for(int it = 1;it <= n;++ it)
    {
        if(can_purify_target(x , it)) sum++;
    }

    if(sum == 0) return 0;

    int k = rnd_id(sum);

    for(int it = 1;it <= n;++ it)
    {
        if(can_purify_target(x , it) == false) continue;
        k--;
        if(k == 0) return it;
    }

    return 0;
}

// 清空当前攻击动作的多段招架标记；普通攻击和地裂单段伤害在结算后调用，雷击在所有段结束后调用。
void clear_parry_action()
{
    parry_source = 0;
    parry_target = 0;
    parry_sid = 0;
}

// 受伤后被动结算器：只在 receive_damage 已确认目标仍存活后调用。
// x 是本次有效伤害来源，it 是受伤者，type 区分伤害类型，end_line 决定被动文本是否与当前伤害文本留在同一日志行。
// 第一阶段处理一次性的垂死挣扎：非生命之轮伤害后、生命严格低于最大生命 10% 时，清除冻结/中毒并追加行动、防御和速度强化。
// 第二阶段处理被动反击：只有带来源的直接伤害和瘟疫可触发；HURT_COUNTER 不会再次触发反击，避免两个反击被动递归循环。
void finish_hurt(int x , int it , HurtType type , bool end_line = true)
{
    if(p[it].alive && p[it].is_scientific_witch) check_scientific_witch_phases(it);

    if(type != HURT_LIFE_WHEEL && p[it].alive && p[it].has_last_stand && p[it].last_stand_used == false && p[it].hp * 100 < get_maxhp(it) * 10)
    {
        p[it].last_stand_used = true;
        p[it].freeze = 0;
        p[it].freeze_int = 0;
        p[it].posion_int = 0;
        p[it].poison_time = 0;
        p[it].tmp.clear();
        p[it].tmp_free.clear();
        p[it].act += 20000;
        p[it].def_plus_int += 15;
        p[it].def_plus_time += 5;
        p[it].spd_up_int += 15;
        p[it].spd_up_time += 5;
        add(p[it].no , p[it].no , 20 , 0 , p[it].hp , 0 , "last_stand" , p[it].name + "发动" , false);
        add(p[it].no , p[it].no , 20 , 0 , p[it].hp , 10 , "last_stand" , "垂死挣扎" , false);
        add(p[it].no , p[it].no , 20 , 0 , p[it].hp , 0 , "last_stand" , "，属性大幅上升!!!" , true);
    }

    if(p[it].alive == false || p[x].alive == false || p[it].has_counter == false) return;
    if(type != HURT_DIRECT && type != HURT_PLAGUE) return;
    if(rnd_id(100) <= 25) passive_counter(it , x , end_line);
}

// K-2 被动：伤害来源自身带有烧伤时，K-2 承受该来源造成伤害的 20%，即减少 80%。
// 无来源的烧伤/中毒持续伤害不触发此被动，避免 K-2 用自身烧伤状态减免环境结算。
int reduce_k2_burn_source_damage(int source , int target , int d)
{
    if(source > 0 && p[target].is_k2 && p[source].burn_time > 0) return d * 20 / 100;
    return d;
}

// 所有扣血的唯一权威入口：这里只修改 hp/alive，不输出文本、不触发招架/守护/反击，也不处理死亡日志。
// it 为受伤者，d 为已完成属性、波动和易损计算后的实际伤害，type 由调用方携带以供上层决定后续被动逻辑。
// 返回 0 表示伤害非正或目标已死而未改状态，1 表示扣血后仍存活，2 表示本次扣血令目标死亡；调用者必须据此调用 finish_hurt 或 report_death。
int receive_damage(int it , int d , HurtType type)
{
    if(d <= 0 || p[it].alive == false) return 0;

    p[it].hp = max(0LL , p[it].hp - d);
    if(p[it].hp > 0) return 1;
    p[it].alive = false;
    return 2;
}

// 招架反击伤害：x 为招架成功者，it 为原攻击者。伤害为 350% 物攻减物防、再施加 80%～120% 波动。
// 触发“招架反击”的文字前缀由 take_hit 预先写入；本函数只计算反击伤害并再次进入 take_hit，使目标仍可被守护或进入死亡结算。
void counter_attack(int x , int it)
{
    if(p[x].alive == false || p[it].alive == false) return;

    int d = max(1LL , get_atk(x) * 350 / 100 - get_def(it));
    int f = 800 + rnd() % 401;
    d = max(1LL , (d * f + 500) / 1000);
    d = reduce_outgoing_damage(x , d);
    take_hit(x , it , d , 14 , "parry_counter_damage" , "parry_counter_damage" , "点伤害");
}

// 被动反击伤害：x 为受伤者，it 为本次伤害来源，造成一次 100% 物攻的普通物理伤害。
// HURT_COUNTER 会让 finish_hurt 跳过再次反击；end_line 传入当前行结束位置，保证守护分摊与反击日志不会互相抢行。
void passive_counter(int x , int it , bool end_line)
{
    if(p[x].alive == false || p[it].alive == false) return;

    int d = max(1LL , get_atk(x) - get_def(it));
    int f = 800 + rnd() % 401;
    d = max(1LL , (d * f + 500) / 1000);
    d = reduce_outgoing_damage(x , d);
    take_hit(x , it , d , 21 , "counter" , "counter_damage" , "点伤害" , HURT_COUNTER , true , end_line);
}

// 吞噬：拥有被动的攻击者击杀敌人后，继承其全部技能资格及权重；已拥有的同名技能直接累加权重，并恢复最大生命的 15%。
void devour(int x , int it)
{
    if(p[x].alive == false || p[x].has_devour == false || p[x].id == p[it].id) return;

    for(int i = 1;i <= p[it].sn;++ i)
    {
        if(p[it].can[i] == false || p[it].skill[i] == nullptr) continue;

        for(int j = 1;j <= p[x].sn;++ j)
        {
            if(p[x].skill[j] != p[it].skill[i]) continue;
            if(p[x].can[j]) p[x].w[j] += p[it].w[i];
            else p[x].w[j] = p[it].w[i];
            p[x].can[j] = true;
            if(p[x].skill[j] == guard) p[x].has_guard = true;
            break;
        }
    }

    int heal = min(get_maxhp(x) * 15 / 100 , get_maxhp(x) - p[x].hp);
    p[x].hp += heal;
    add(p[x].no , p[it].no , 22 , 0 , p[x].hp , 0 , "devour" , p[x].name , false);
    add(p[x].no , p[it].no , 22 , 0 , p[x].hp , 16 , "devour" , "吞噬" , false);
    add(p[x].no , p[it].no , 22 , 0 , p[x].hp , 0 , "devour" , p[it].name + " " , false);
    add(p[x].no , p[it].no , 22 , 0 , p[x].hp , 0 , "devour" , p[x].name + " " , false);
    add(p[x].no , p[x].no , 22 , heal , p[x].hp , 4 , "devour_heal" , "恢复15%生命" , true);
}

// 判断 owner 是否已有存活的指定绑定眷属；月之子与 K-2 都各自限制为同时最多一名。
bool has_living_moon_child(int owner)
{
    for(int i = 1;i <= n;++ i)
    {
        if(p[i].alive && p[i].is_moon_child && p[i].owner == owner) return true;
    }
    return false;
}

bool has_living_k2(int owner)
{
    for(int i = 1;i <= n;++ i)
    {
        if(p[i].alive && p[i].is_k2 && p[i].owner == owner) return true;
    }
    return false;
}

// 科学性实验魔女与月之子、K-2 一样受本体绑定上限约束：同一名 mili 最多保留一名存活魔女。
bool has_living_scientific_witch(int owner)
{
    for(int i = 1;i <= n;++ i)
    {
        if(p[i].alive && p[i].is_scientific_witch && p[i].owner == owner) return true;
    }
    return false;
}

// 本体死亡时撤销其存活幻魔；幻魔死亡不会反向影响本体。
void dismiss_familiars(int x)
{
    for(int i = 1;i <= n;++ i)
    {
        if(p[i].alive == false || p[i].is_familiar == false || p[i].owner != x) continue;
        if(p[i].is_moon_child) moon_chant(i); // 月之子即使因本体死亡而消失，也会先完成一次遗言吟唱。
        dismiss_familiars(i); // 递归撤销二级眷属，保证实验魔女离场时其兰斯洛特0号也同步消失。
        p[i].alive = false;
        p[i].hp = 0;
        p[i].tmp.clear();
        p[i].tmp_free.clear();
        add(x , i , 23 , 0 , 0 , 1 , "familiar_depart" , p[i].name + "随本体消失了" , true);
    }
}

// 统一死亡收尾：source 为击杀来源（0 表示无来源持续伤害），it 为死亡者，sid 为导致死亡的技能编号。
// 先输出死亡 Cmd，再撤销死亡本体绑定的存活幻魔，最后仅在存在来源时触发吞噬；集中在此处可保证任何伤害路径的死亡联动一致。
void report_death(int source , int it , int sid)
{
    add(source , it , sid , 0 , 0 , 1 , "death" , p[it].name + "消失了" , true);
    if(p[it].is_moon_child) moon_chant(it); // 月之子死亡后额外吟唱一次；自身仍作为文本来源，但不再是治疗目标。
    if(p[it].is_lancelot_zero && p[it].owner > 0 && p[p[it].owner].alive && p[p[it].owner].is_scientific_witch)
    {
        int witch = p[it].owner;
        summon_broken_lancelot_zero(witch);
        p[witch].has_rebirth_potion = true;
        p[witch].rebirth_potion_count = 0;
        p[witch].sn++;
        p[witch].skill[p[witch].sn] = brew_rebirth_potion;
        p[witch].can[p[witch].sn] = true;
        p[witch].w[p[witch].sn] = 1;
    }
    dismiss_familiars(it);
    if(source > 0) devour(source , it);
}

// 召唤：每次施放都创建一名只会普攻的绑定眷属；不设数量上限，魔力消耗由 use_skill 统一扣除。
void summon(int x)
{
    n++;
    p[n] = Player();
    p[n].skill.fill(nullptr);
    p[n].can.fill(false);
    p[n].w.fill(0);
    p[n].tmp.clear();
    p[n].tmp_free.clear();
    p[n].name = "幻魔";
    p[n].no = n;
    p[n].id = p[x].id;
    p[n].hp = 150;
    p[n].maxhp = 150;
    p[n].atk = 80;
    p[n].def = 20;
    p[n].magic = 0;
    p[n].maxmagic = 0;
    p[n].mreg = 0;
    p[n].spd = 1500;
    p[n].satk = 0;
    p[n].sdef = 20;
    p[n].iq = 0;
    p[n].act = 0;
    p[n].alive = true;
    p[n].freeze = 0;
    p[n].freeze_int = 0;
    p[n].def_plus_int = 0;
    p[n].def_plus_time = 0;
    p[n].def_down_int = 0;
    p[n].def_down_time = 0;
    p[n].damage_down_int = 0;
    p[n].damage_down_time = 0;
    p[n].blood_int = 0;
    p[n].burn_int = 0;
    p[n].burn_time = 0;
    p[n].posion_int = 0;
    p[n].poison_time = 0;
    p[n].parry_time = 0;
    p[n].has_guard = false;
    p[n].has_last_stand = false;
    p[n].last_stand_used = false;
    p[n].has_counter = false;
    p[n].has_devour = false;
    p[n].is_boss = false;
    p[n].is_mili = false;
    p[n].mili_skill_turn = 0;
    p[n].boss_string_count = 0;
    p[n].is_familiar = true;
    p[n].is_moon_child = false;
    p[n].is_k2 = false;
    p[n].owner = x;
    p[n].magic_vuln = 20;
    p[n].spd_up_int = 0;
    p[n].spd_up_time = 0;
    p[n].spd_down_int = 0;
    p[n].spd_down_time = 0;
    p[n].sn = 1;
    p[n].skill[1] = basic_attack;
    p[n].can[1] = true;

    add(p[x].no , p[n].no , 23 , 0 , p[n].hp , 0 , "summon" , p[x].name + " " , false);
    add(p[x].no , p[n].no , 23 , 0 , p[n].hp , 17 , "summon" , "召唤" , false);
    add(p[x].no , p[n].no , 23 , 0 , p[n].hp , 0 , "summon" , " 出了 " , false);
    add(p[x].no , p[n].no , 23 , 0 , p[n].hp , 16 , "summon" , "幻魔" , false);
    add(p[x].no , p[n].no , 23 , 0 , p[n].hp , 0 , "summon_spawn" , "" , true);
}

// 月之子的吟唱：为同队全体存活成员恢复各自最大生命的 2%；文本仅显示一次“全体恢复”，逐人结算以静默快照同步。
// 死亡遗言和主动回合均复用本函数，因此月之子已死亡时仍能完成最后一次阵营治疗。
void moon_chant(int x)
{
    int team = p[x].id;
    bool has_target = false;

    for(int it = 1;it <= n;++ it) if(p[it].alive && p[it].id == team) { has_target = true; break; }
    if(has_target)
    {
        add(p[x].no , p[x].no , 28 , 0 , p[x].hp , 0 , "moon_chant" , "月之子", false);
        add(p[x].no , p[x].no , 28 , 0 , p[x].hp , 16 , "moon_chant" , "吟唱，" , false);
        add(p[x].no , p[x].no , 28 , 0 , p[x].hp , 0 , "moon_chant" , "为全体恢复2%血量", true);
    }

    for(int it = 1;it <= n;++ it)
    {
        if(p[it].alive == false || p[it].id != team) continue;
        int d = get_maxhp(it) * 2 / 100;
        p[it].hp = min(get_maxhp(it) , p[it].hp + d);
        add(p[x].no , p[it].no , 28 , 0 , p[it].hp , 0 , "status_sync" , "", true);
    }
}

// mili 的月之子召唤：每次施放创建一名绑定眷属。月之子只会吟唱且不带普通幻魔的魔法易损。
void summon_moon_child(int x)
{
    if(has_living_moon_child(x)) return;
    n++;
    p[n] = Player();
    p[n].skill.fill(nullptr);
    p[n].can.fill(false);
    p[n].w.fill(0);
    p[n].tmp.clear();
    p[n].tmp_free.clear();
    p[n].name = "月之子";
    p[n].no = n;
    p[n].id = p[x].id;
    p[n].hp = p[n].maxhp = 50;
    p[n].atk = 100;
    p[n].def = -50;
    p[n].magic = p[n].maxmagic = p[n].mreg = 0;
    p[n].spd = 2500;
    p[n].satk = p[n].sdef = p[n].iq = 0;
    p[n].act = 0;
    p[n].alive = true;
    p[n].freeze = p[n].freeze_int = 0;
    p[n].def_plus_int = p[n].def_plus_time = 0;
    p[n].def_down_int = p[n].def_down_time = 0;
    p[n].damage_down_int = p[n].damage_down_time = 0;
    p[n].square_int = p[n].square_time = 0;
    p[n].blood_int = p[n].burn_int = p[n].burn_time = 0;
    p[n].posion_int = p[n].poison_time = p[n].parry_time = 0;
    p[n].has_guard = p[n].has_last_stand = p[n].last_stand_used = p[n].has_counter = p[n].has_devour = false;
    p[n].is_boss = p[n].is_mili = false;
    p[n].mili_skill_turn = p[n].boss_string_count = 0;
    p[n].boss_magic_ready = false;
    p[n].is_familiar = p[n].is_moon_child = true;
    p[n].is_k2 = false;
    p[n].owner = x;
    p[n].magic_vuln = 0;
    p[n].spd_up_int = p[n].spd_up_time = p[n].spd_down_int = p[n].spd_down_time = 0;
    p[n].sn = 1;
    p[n].skill[1] = moon_chant;
    p[n].can[1] = true;
    p[n].w[1] = 1;
    add(p[x].no , p[n].no , 28 , 0 , p[n].hp , 0 , "summon_spawn" , "", true);
    add(p[x].no , p[n].no , 28 , 0 , p[n].hp , 0 , "moon_summon" , "mili召唤出了", false);
    add(p[x].no , p[n].no , 28 , 0 , p[n].hp , 16 , "moon_summon" , "月之子" , true);
}

// Iron Lotus：K-2 为全部存活角色覆盖 1 强度、5 层烧伤；施放文本汇总为“全体施加”，逐人状态变化仅静默同步。
void iron_blood_lotus(int x)
{
    add(p[x].no , p[x].no , 29 , 0 , p[x].hp , 0 , "iron_blood_lotus" , "K-2使用", false);
    add(p[x].no , p[x].no , 29 , 0 , p[x].hp , 3 , "iron_blood_lotus" , "Iron Lotus", false);
    add(p[x].no , p[x].no , 29 , 0 , p[x].hp , 0 , "iron_blood_lotus" , "，对全体施加1级5层烧伤", true);
    for(int it = 1;it <= n;++ it)
    {
        if(p[it].alive == false) continue;
        p[it].burn_int += 1;
        p[it].burn_time += 5;
        add(p[x].no , p[it].no , 29 , 0 , p[it].hp , 0 , "status_sync" , "", true);
    }
}

// mili 的 K-2 召唤：每位 mili 同时至多一名。K-2 只会铁血莲华，并保留烧伤来源减伤被动。
void summon_k2(int x)
{
    if(has_living_k2(x)) return;
    n++;
    p[n] = Player();
    p[n].skill.fill(nullptr);
    p[n].can.fill(false);
    p[n].w.fill(0);
    p[n].tmp.clear();
    p[n].tmp_free.clear();
    p[n].name = "K-2";
    p[n].no = n;
    p[n].id = p[x].id;
    p[n].hp = p[n].maxhp = 500;
    p[n].atk = 150;
    p[n].def = 25;
    p[n].magic = p[n].maxmagic = p[n].mreg = 0;
    p[n].spd = 1500;
    p[n].satk = p[n].sdef = p[n].iq = 0;
    p[n].act = 0;
    p[n].alive = true;
    p[n].freeze = p[n].freeze_int = 0;
    p[n].def_plus_int = p[n].def_plus_time = 0;
    p[n].def_down_int = p[n].def_down_time = 0;
    p[n].damage_down_int = p[n].damage_down_time = 0;
    p[n].square_int = p[n].square_time = 0;
    p[n].blood_int = p[n].burn_int = p[n].burn_time = 0;
    p[n].posion_int = p[n].poison_time = p[n].parry_time = 0;
    p[n].has_guard = p[n].has_last_stand = p[n].last_stand_used = p[n].has_counter = p[n].has_devour = false;
    p[n].is_boss = p[n].is_mili = false;
    p[n].mili_skill_turn = p[n].boss_string_count = 0;
    p[n].boss_magic_ready = false;
    p[n].is_familiar = p[n].is_k2 = true;
    p[n].is_moon_child = false;
    p[n].owner = x;
    p[n].magic_vuln = 0;
    p[n].spd_up_int = p[n].spd_up_time = p[n].spd_down_int = p[n].spd_down_time = 0;
    p[n].sn = 1;
    p[n].skill[1] = iron_blood_lotus;
    p[n].can[1] = true;
    p[n].w[1] = 1;
    add(p[x].no , p[n].no , 29 , 0 , p[n].hp , 0 , "summon_spawn" , "", true);
    add(p[x].no , p[n].no , 29 , 0 , p[n].hp , 0 , "k2_summon" , "mili召唤出了", false);
    add(p[x].no , p[n].no , 29 , 0 , p[n].hp , 10 , "k2_summon" , "K-2", true);
}

// K-2 回合末额外烧伤：所有仍存活、处于敌对阵营且带烧伤的角色额外承受一次 burn_int。
// 日志仅显示一次“全体再次烧伤”；逐个扣血以静默状态同步完成，不递减层数，也不触发守护或反击。
void trigger_k2_extra_burn(int x)
{
    bool has_target = false;
    for(int it = 1;it <= n;++ it) if(p[it].alive && p[it].id != p[x].id && p[it].burn_time > 0) { has_target = true; break; }
    if(has_target) add(p[x].no , p[x].no , 29 , 0 , p[x].hp , 13 , "k2_burn" , "K-2令全体再次烧伤", true);
    for(int it = 1;it <= n;++ it)
    {
        if(p[it].alive == false || p[it].id == p[x].id || p[it].burn_time <= 0) continue;
        int d = p[it].burn_int;
        int res = receive_damage(it , d , HURT_BURN);
        p[it].burn_time--; // K-2 的额外烧伤同样是一次实际烧伤结算，必须消耗目标一层持续层数。
        if(p[it].burn_time == 0) p[it].burn_int = 0;
        add(p[x].no , p[it].no , 29 , 0 , p[it].hp , 0 , "status_sync" , "", true);
        if(res == 2) report_death(0 , it , 29);
        else if(res == 1) finish_hurt(0 , it , HURT_BURN);
    }
}

// 空技能：科学性实验魔女当前阶段没有自主攻击或施法，轮到自身时无文字地结束行动。
void idle_skill(int x)
{
    (void)x;
}

// 兰斯洛特0号的特殊普攻。冻结模式完全沿用普通攻击文字和伤害管线，命中存活目标后静默附加 1 层冻结。
// 烧伤模式同样只显示“发起攻击”，但不扣血；将本次本可造成的普通攻击伤害作为烧伤强度，附加 1 层持续时间。
void lancelot_zero_attack(int x)
{
    int it = get_target(x);
    if(it == 0) return;
    int d = max(1LL , get_atk(x) - get_def(it));
    int f = 800 + rnd() % 401;
    d = max(1LL , (d * f + 500) / 1000);
    d = reduce_outgoing_damage(x , d);

    if(rnd_id(2) == 1)
    {
        add(p[x].no , p[it].no , 1 , 0 , p[it].hp , 0 , "normal_attack" , p[x].name + " " , false);
        add(p[x].no , p[it].no , 1 , 0 , p[it].hp , 0 , "normal_attack" , "发起攻击" , false);
        add(p[x].no , p[it].no , 1 , 0 , p[it].hp , 0 , "normal_attack" , "，" , false);
        int res = take_hit(x , it , d , 1 , "normal_attack" , "normal_attack_damage");
        clear_parry_action();
        if(res != 1) return;
        p[it].freeze++;
        p[it].freeze_int++;
        add(p[x].no , p[it].no , 1 , 0 , p[it].hp , 0 , "status_sync" , "" , true);
        return;
    }

    p[it].burn_int += d;
    p[it].burn_time++;
    add(p[x].no , p[it].no , 1 , 0 , p[it].hp , 0 , "normal_attack" , p[x].name + " " , false);
    add(p[x].no , p[it].no , 1 , 0 , p[it].hp , 0 , "normal_attack" , "发起攻击" , true);
    clear_parry_action();
}

// 科学性实验魔女的二级眷属：固定物理属性，只拥有兰斯洛特0号的两种普通攻击模式。
void summon_lancelot_zero(int x)
{
    n++;
    p[n] = Player();
    p[n].skill.fill(nullptr);
    p[n].can.fill(false);
    p[n].w.fill(0);
    p[n].tmp.clear();
    p[n].tmp_free.clear();
    p[n].name = "兰斯洛特0号";
    p[n].no = n;
    p[n].id = p[x].id;
    p[n].hp = p[n].maxhp = 800;
    p[n].atk = 150;
    p[n].def = 100;
    p[n].magic = p[n].maxmagic = p[n].mreg = 0;
    p[n].spd = 2000;
    p[n].satk = p[n].sdef = p[n].iq = 0;
    p[n].act = 0;
    p[n].alive = true;
    p[n].freeze = p[n].freeze_int = 0;
    p[n].def_plus_int = p[n].def_plus_time = 0;
    p[n].def_down_int = p[n].def_down_time = 0;
    p[n].damage_down_int = p[n].damage_down_time = 0;
    p[n].square_int = p[n].square_time = 0;
    p[n].blood_int = p[n].burn_int = p[n].burn_time = 0;
    p[n].posion_int = p[n].poison_time = p[n].parry_time = 0;
    p[n].has_guard = p[n].has_last_stand = p[n].last_stand_used = p[n].has_counter = p[n].has_devour = false;
    p[n].is_boss = p[n].is_mili = false;
    p[n].mili_skill_turn = p[n].boss_string_count = 0;
    p[n].boss_magic_ready = false;
    p[n].is_familiar = p[n].is_lancelot_zero = true;
    p[n].is_moon_child = p[n].is_k2 = p[n].is_scientific_witch = false;
    p[n].has_rebirth_potion = false;
    p[n].rebirth_potion_count = 0;
    p[n].is_galahad_one = false;
    p[n].is_inert = false;
    p[n].owner = x;
    p[n].magic_vuln = 0;
    p[n].spd_up_int = p[n].spd_up_time = p[n].spd_down_int = p[n].spd_down_time = 0;
    p[n].sn = 1;
    p[n].skill[1] = lancelot_zero_attack;
    p[n].can[1] = true;
    p[n].w[1] = 1;
    add(p[x].no , p[n].no , 31 , 0 , p[n].hp , 0 , "lancelot_summon" , p[x].name + "召唤出了", false);
    add(p[x].no , p[n].no , 31 , 0 , p[n].hp , 16 , "lancelot_summon" , "兰斯洛特0号", false);
    add(p[x].no , p[n].no , 31 , 0 , p[n].hp , 0 , "summon_spawn" , "", true);
}

// mili 的科学性实验魔女：本体召唤完成后立即再由魔女召唤兰斯洛特0号；魔女后续只使用空技能跳过行动。
void summon_scientific_witch(int x)
{
    if(p[x].mili_witch_summoned || has_living_scientific_witch(x)) return;
    p[x].mili_witch_summoned = true;
    n++;
    p[n] = Player();
    p[n].skill.fill(nullptr);
    p[n].can.fill(false);
    p[n].w.fill(0);
    p[n].tmp.clear();
    p[n].tmp_free.clear();
    p[n].name = "科学性实验魔女";
    p[n].no = n;
    p[n].id = p[x].id;
    p[n].hp = p[n].maxhp = 1000;
    p[n].atk = p[n].def = 0;
    p[n].magic = p[n].maxmagic = p[n].mreg = 0;
    p[n].spd = 3000;
    p[n].satk = 250;
    p[n].sdef = 100;
    p[n].iq = 0;
    p[n].act = 0;
    p[n].alive = true;
    p[n].freeze = p[n].freeze_int = 0;
    p[n].def_plus_int = p[n].def_plus_time = 0;
    p[n].def_down_int = p[n].def_down_time = 0;
    p[n].damage_down_int = p[n].damage_down_time = 0;
    p[n].square_int = p[n].square_time = 0;
    p[n].blood_int = p[n].burn_int = p[n].burn_time = 0;
    p[n].posion_int = p[n].poison_time = p[n].parry_time = 0;
    p[n].has_guard = p[n].has_last_stand = p[n].last_stand_used = p[n].has_counter = p[n].has_devour = false;
    p[n].is_boss = p[n].is_mili = false;
    p[n].mili_skill_turn = p[n].boss_string_count = 0;
    p[n].boss_magic_ready = false;
    p[n].is_familiar = p[n].is_scientific_witch = true;
    p[n].is_moon_child = p[n].is_k2 = p[n].is_lancelot_zero = false;
    p[n].galahad_laser_mode = p[n].galahad_multi_target = false;
    p[n].witch_phase_50_used = p[n].witch_phase_30_used = p[n].witch_phase_10_used = false;
    p[n].owner = x;
    p[n].magic_vuln = 0;
    p[n].spd_up_int = p[n].spd_up_time = p[n].spd_down_int = p[n].spd_down_time = 0;
    p[n].sn = 1;
    p[n].skill[1] = idle_skill;
    p[n].can[1] = true;
    p[n].w[1] = 1;
    add(p[x].no , p[n].no , 30 , 0 , p[n].hp , 0 , "witch_summon" , "mili召唤出了", false);
    add(p[x].no , p[n].no , 30 , 0 , p[n].hp , 10 , "witch_summon" , "科学性实验魔女", false);
    add(p[x].no , p[n].no , 30 , 0 , p[n].hp , 0 , "summon_spawn" , "", true);
    summon_lancelot_zero(n);
}

// 破烂的兰斯洛特0号：由兰斯洛特0号死亡时的科学性实验魔女立即召唤；永久不累计行动值，也没有技能。
void summon_broken_lancelot_zero(int x)
{
    n++;
    p[n] = Player();
    p[n].skill.fill(nullptr);
    p[n].can.fill(false);
    p[n].w.fill(0);
    p[n].tmp.clear();
    p[n].tmp_free.clear();
    p[n].name = "破烂的兰斯洛特0号";
    p[n].no = n;
    p[n].id = p[x].id;
    p[n].hp = p[n].maxhp = 800;
    p[n].atk = 0;
    p[n].def = -20;
    p[n].magic = p[n].maxmagic = p[n].mreg = 0;
    p[n].spd = 0;
    p[n].satk = p[n].sdef = p[n].iq = 0;
    p[n].act = 0;
    p[n].alive = true;
    p[n].freeze = p[n].freeze_int = 0;
    p[n].def_plus_int = p[n].def_plus_time = 0;
    p[n].def_down_int = p[n].def_down_time = 0;
    p[n].damage_down_int = p[n].damage_down_time = 0;
    p[n].square_int = p[n].square_time = 0;
    p[n].blood_int = p[n].burn_int = p[n].burn_time = 0;
    p[n].posion_int = p[n].poison_time = p[n].parry_time = 0;
    p[n].has_guard = p[n].has_last_stand = p[n].last_stand_used = p[n].has_counter = p[n].has_devour = false;
    p[n].is_boss = p[n].is_mili = false;
    p[n].mili_skill_turn = p[n].boss_string_count = 0;
    p[n].boss_magic_ready = false;
    p[n].is_familiar = p[n].is_broken_lancelot_zero = true;
    p[n].is_moon_child = p[n].is_k2 = p[n].is_scientific_witch = p[n].is_lancelot_zero = false;
    p[n].galahad_laser_mode = p[n].galahad_multi_target = false;
    p[n].witch_phase_50_used = p[n].witch_phase_30_used = p[n].witch_phase_10_used = false;
    p[n].has_rebirth_potion = false;
    p[n].rebirth_potion_count = 0;
    p[n].is_galahad_one = false;
    p[n].is_inert = true;
    p[n].owner = x;
    p[n].magic_vuln = 0;
    p[n].spd_up_int = p[n].spd_up_time = p[n].spd_down_int = p[n].spd_down_time = 0;
    p[n].sn = 0;
    add(p[x].no , p[n].no , 32 , 0 , p[n].hp , 0 , "broken_lancelot_summon" , "兰斯洛特0号变成了废铁!", false);
    add(p[x].no , p[n].no , 32 , 0 , p[n].hp , 0 , "summon_spawn" , "", true);
}

// 将仍存活的破烂随从原地转换为加拉哈德1号；转换保留其眷属位置和魔女归属，但重置所有战斗状态。
void convert_broken_to_galahad(int x)
{
    if(p[x].alive == false || p[x].is_broken_lancelot_zero == false) return;

    int witch = p[x].owner;
    p[x].name = "加拉哈德1号";
    p[x].hp = p[x].maxhp = 1200;
    p[x].atk = 160;
    p[x].def = 0;
    p[x].magic = p[x].maxmagic = p[x].mreg = 0;
    p[x].spd = 2500;
    p[x].satk = 180;
    p[x].sdef = 0;
    p[x].iq = 0;
    p[x].act = 0;
    p[x].alive = true;
    p[x].freeze = p[x].freeze_int = 0;
    p[x].def_plus_int = p[x].def_plus_time = 0;
    p[x].def_down_int = p[x].def_down_time = 0;
    p[x].damage_down_int = p[x].damage_down_time = 0;
    p[x].square_int = p[x].square_time = 0;
    p[x].blood_int = p[x].burn_int = p[x].burn_time = 0;
    p[x].posion_int = p[x].poison_time = p[x].parry_time = 0;
    p[x].spd_up_int = p[x].spd_up_time = p[x].spd_down_int = p[x].spd_down_time = 0;
    p[x].has_guard = p[x].has_last_stand = p[x].last_stand_used = p[x].has_counter = p[x].has_devour = false;
    p[x].is_broken_lancelot_zero = false;
    p[x].is_galahad_one = true;
    p[x].galahad_laser_mode = p[x].galahad_multi_target = false;
    p[x].witch_phase_50_used = p[x].witch_phase_30_used = p[x].witch_phase_10_used = false;
    p[x].is_inert = false;
    p[x].skill.fill(nullptr);
    p[x].can.fill(false);
    p[x].w.fill(0);
    p[x].tmp.clear();
    p[x].tmp_free.clear();
    p[x].sn = 0;
    add(witch , p[x].no , 33 , 0 , p[x].hp , 4 , "rebirth_potion" , "加拉哈德1号复活了", true);
}

// 调制重生药水：前两次仅播放原调制文本；第三次完成后播放四行英文仪式，随后把破烂随从转换为加拉哈德1号。
void brew_rebirth_potion(int x)
{
    int scrap = 0;
    for(int it = 1;it <= n;++ it)
    {
        if(p[it].alive && p[it].owner == x && p[it].is_broken_lancelot_zero)
        {
            scrap = it;
            break;
        }
    }

    if(scrap == 0) return;

    p[x].rebirth_potion_count++;
    if(p[x].rebirth_potion_count < 3)
    {
        add(p[x].no , p[x].no , 33 , 0 , p[x].hp , 0 , "rebirth_potion" , "科学性实验魔女开始调制", false);
        add(p[x].no , p[x].no , 33 , 0 , p[x].hp , 4 , "rebirth_potion" , "重生药水", false);
        add(p[x].no , p[x].no , 33 , 0 , p[x].hp , 0 , "rebirth_potion" , "，Lulila talila~~~", true);
        return;
    }

    add(p[x].no , p[x].no , 33 , 0 , p[x].hp , 4 , "rebirth_potion" , "the magic potion of reanimation~~~", true);
    add(p[x].no , p[x].no , 33 , 0 , p[x].hp , 4 , "rebirth_potion" , "rise from bed my darling~~~", true);
    add(p[x].no , p[x].no , 33 , 0 , p[x].hp , 4 , "rebirth_potion" , "so I can see you again ~~~", true);
    add(p[x].no , p[x].no , 33 , 0 , p[x].hp , 4 , "rebirth_potion" , "so I can kill you again", true);
    p[x].has_rebirth_potion = false;
    for(int i = 1;i <= p[x].sn;++ i)
    {
        if(p[x].skill[i] == brew_rebirth_potion) p[x].can[i] = false;
    }
    convert_broken_to_galahad(scrap);
}

// 判断 guardian 是否可为 target 的本次伤害承担守护：必须同队存活、有守护、未被冻结/招架，且承担 40% 后仍存活。
bool can_guard_target(int guardian , int target , int d)
{
    if(guardian == target || p[guardian].alive == false || p[guardian].has_guard == false) return false;
    if(p[guardian].id != p[target].id || p[guardian].freeze > 0 || p[guardian].parry_time > 0) return false;

    int part = d * 40 / 100;
    return part > 0 && p[guardian].hp > part;
}

// 在 target 的同队守护者中依次进行固定 20% 判定；首个成功者承担本次唯一的守护，守护伤害不会再触发守护。
int get_guardian(int target , int d)
{
    for(int guardian = 1;guardian <= n;++ guardian)
    {
        if(can_guard_target(guardian , target , d) == false) continue;
        if(rnd_id(100) <= 20) return guardian;
    }

    return 0;
}

// 守护分摊伤害入口：x 为原攻击来源，it 为原目标，d 为完整原伤害；sid/ani/damage_ani/damage_suffix 控制日志与前端动画。
// 函数先由 get_guardian 决定是否存在唯一守护者。成功时原目标和守护者各承受 floor(d * 40%)，两份伤害均禁止再次进入守护，避免链式分摊。
// 内部 apply_damage 负责每一份伤害的文本、receive_damage、死亡报告与受伤后被动；first_target 用于让反击文案只在首份伤害前出现。
// end_line 由多段技能传入，确保地裂、雷击、守护和反击能够按既定位置连接或结束日志行。
int take_guard_damage(int x , int it , int d , int sid , const string& ani , const string& damage_ani , const string& damage_suffix , HurtType type , bool passive_counter_attack , bool end_line)
{
    int total_dealt = 0; // 同一次攻击可能由原目标与一名守护者分摊，累计两者实际承受的伤害后供斩击回血使用。
    auto apply_damage = [&](int target , int val , bool first_target , bool this_end_line)
    {
        if(passive_counter_attack && first_target)
        {
            add(p[x].no , p[target].no , sid , 0 , p[target].hp , 0 , ani , p[x].name , false);
            add(p[x].no , p[target].no , sid , 0 , p[target].hp , 10 , ani , "反击! " , false);
            add(p[x].no , p[target].no , sid , 0 , p[target].hp , 0 , ani , p[target].name , false);
            add(p[x].no , p[target].no , sid , 0 , p[target].hp , 0 , ani , "受到" , false);
        }
        else
        {
            add(p[x].no , p[target].no , sid , 0 , p[target].hp , 0 , ani , p[target].name , false);
            add(p[x].no , p[target].no , sid , 0 , p[target].hp , 0 , ani , "受到" , false);
        }
        int actual = reduce_k2_burn_source_damage(x , target , val);
        int res = receive_damage(target , actual , type);
        total_dealt += actual;
        add(p[x].no , p[target].no , sid , actual , p[target].hp , 3 , damage_ani , to_string(actual) , false);
        add(p[x].no , p[target].no , sid , actual , p[target].hp , 0 , damage_ani , damage_suffix , this_end_line);

        if(res == 2)
        {
            report_death(x , target , sid);
        }
        else if(res == 1) finish_hurt(x , target , type , this_end_line);
    };
    int guardian = get_guardian(it , d);

    if(guardian != 0)
    {
        int part = d * 40 / 100;
        add(guardian , it , 18 , 0 , p[it].hp , 0 , "guard" , p[guardian].name , false);
        add(guardian , it , 18 , 0 , p[it].hp , 10 , "guard" , "守护" , false);
        add(guardian , it , 18 , 0 , p[it].hp , 0 , "guard" , p[it].name , true);
        apply_damage(it , part , true , false);
        add(p[x].no , p[guardian].no , sid , 0 , p[guardian].hp , 0 , damage_ani , " " , false);
        apply_damage(guardian , part , false , false);
        add(p[x].no , p[guardian].no , sid , 0 , p[guardian].hp , 0 , damage_ani , "" , end_line);
        return total_dealt;
    }

    apply_damage(it , d , true , end_line);
    return total_dealt;
}

// 直接命中总入口：负责先处理招架，再进入守护分摊与真实扣血；持续伤害、生命之轮不经过这里。
// x 为来源、it 为目标、d 为已计算伤害；sid/ani/damage_ani/damage_suffix 只描述前端文本和动画，不参与数值计算。
// type 会沿途传给 receive_damage/finish_hurt；passive_counter_attack 标记本次文本需要“反击!”前缀；end_line 控制多段事件最后一片是否换行。
// 返回 0 表示被招架拦截，1 表示命中后目标存活，2 表示目标死亡。招架成功会清空招架层并立刻调用 counter_attack，随后同一技能的其他段只显示格挡。
int take_hit(int x , int it , int d , int sid , const string& ani , const string& damage_ani , const string& damage_suffix , HurtType type , bool passive_counter_attack , bool end_line , int* dealt)
{
    if(dealt != nullptr) *dealt = 0;
    bool blocked = p[it].parry_time > 0 || (parry_source == x && parry_target == it && parry_sid == sid);

    if(blocked)
    {
        bool trigger = p[it].parry_time > 0;

        if(trigger)
        {
            p[it].parry_time = 0;
            parry_source = x;
            parry_target = it;
            parry_sid = sid;
        }

        if(trigger)
        {
            add(p[x].no , p[it].no , sid , 0 , p[it].hp , 0 , "parry_counter_start" , p[it].name , false);
            add(p[x].no , p[it].no , sid , 0 , p[it].hp , 0 , "parry_counter_start" , "触发" , false);
            add(p[x].no , p[it].no , sid , 0 , p[it].hp , 10 , "parry_counter_start" , "招架反击" , false);
            add(p[x].no , p[it].no , sid , 0 , p[it].hp , 0 , "parry_counter_start" , "，" , false);
            counter_attack(it , x);
        }
        else
        {
            add(p[x].no , p[it].no , sid , 0 , p[it].hp , 0 , "parry_block" , p[it].name + "成功" , false);
            add(p[x].no , p[it].no , sid , 0 , p[it].hp , 10 , "parry_block" , "格挡" , true);
        }

        return 0;
    }

    int actual_dealt = take_guard_damage(x , it , d , sid , ani , damage_ani , damage_suffix , type , passive_counter_attack , end_line);
    if(dealt != nullptr) *dealt = actual_dealt;
    return p[it].alive ? 1 : 2;
}

// 普通攻击伤害函数：x 从存活敌人中均匀选择 it，先按 max(1, 物攻 - 物防) 计算基础物理伤害，再施加 80%～120% 波动。
// 三条 add 依次拼出“施法者 发起攻击，”前缀；take_hit 负责该攻击后续的招架、守护、扣血、死亡和受伤被动。
// 本次攻击结束后必须 clear_parry_action，避免同一普通攻击触发的招架标记污染下一次独立攻击。
void basic_attack(int x)
{
    int it = p[x].lament_time > 0 ? get_lament_target(x) : get_target(x); // 哀悼存在时改为可命中自己、队友或敌人的无差别目标。

    if(it == 0) return;

    int d = max(1LL , get_atk(x) - get_def(it)); // 未波动的物理基础伤害。
    int f = 800 + rnd() % 401; // 80%～120% 的整数伤害波动系数。
    d = max(1LL , (d * f + 500) / 1000);
    d = reduce_outgoing_damage(x , d);

    add(p[x].no , p[it].no , 1 , 0 , p[it].hp , 0 , "normal_attack" , p[x].name + " " , false);
    add(p[x].no , p[it].no , 1 , 0 , p[it].hp , 0 , "normal_attack" , "发起攻击" , false);
    add(p[x].no , p[it].no , 1 , 0 , p[it].hp , 0 , "normal_attack" , "，" , false);
    take_hit(x , it , d , 1 , "normal_attack" , "normal_attack_damage");
    clear_parry_action();
}

// 加拉哈德1号的唯一行动：按普通物理攻击公式随机选敌人，红色显示“斩击”，并将本次实际命中伤害的 100% 治疗所属科学性实验魔女。
// take_hit 的 dealt 输出会累加守护分摊后的真实扣血，同时会在招架时返回 0，保证魔女不会因被格挡的斩击获得治疗。
void galahad_one_attack(int x)
{
    int it = get_target(x);
    if(it == 0) return;

    int d = max(1LL , get_atk(x) - get_def(it));
    int f = 800 + rnd() % 401;
    d = max(1LL , (d * f + 500) / 1000);
    d = reduce_outgoing_damage(x , d);

    add(p[x].no , p[it].no , 34 , 0 , p[it].hp , 0 , "galahad_slash" , p[x].name + " " , false);
    add(p[x].no , p[it].no , 34 , 0 , p[it].hp , 3 , "galahad_slash" , "斩击" , false);
    add(p[x].no , p[it].no , 34 , 0 , p[it].hp , 0 , "galahad_slash" , p[it].name + "，" , false);
    int dealt = 0;
    take_hit(x , it , d , 34 , "galahad_slash" , "galahad_slash_damage" , "伤害" , HURT_DIRECT , false , true , &dealt);
    clear_parry_action();

    int witch = p[x].owner;
    if(dealt <= 0 || witch <= 0 || witch > n || p[witch].alive == false) return;

    int before = p[witch].hp;
    p[witch].hp = min(get_maxhp(witch) , p[witch].hp + dealt);
    int healed = p[witch].hp - before;
    add(p[x].no , p[witch].no , 34 , healed , p[witch].hp , 4 , "galahad_witch_heal" , p[witch].name + "恢复" , false);
    add(p[x].no , p[witch].no , 34 , healed , p[witch].hp , 0 , "galahad_witch_heal" , to_string(healed) + "生命" , true);
}

// 蓝色激光枪：单目标或三目标分别进行 150% 普攻伤害；命中存活目标后附加 10% 伤害值烧伤、2 层持续时间。
// 与普通斩击不同，本函数故意不读取或恢复所属魔女生命，激光枪永远不会回血。
void galahad_laser_attack(int x)
{
    vector<int> targets;
    for(int i = 1; i <= n; ++i)
    {
        if(p[i].alive && p[i].id != p[x].id) targets.push_back(i);
    }
    if(targets.empty()) return;

    int count = p[x].galahad_multi_target ? min(3LL, (int)targets.size()) : 1;
    for(int i = 0; i < count; ++i)
    {
        int pick = rnd_id((int)targets.size()) - 1;
        int it = targets[pick];
        targets.erase(targets.begin() + pick);
        int d = max(1LL, get_atk(x) * 150 / 100 - get_def(it));
        int f = 800 + rnd() % 401;
        d = max(1LL, (d * f + 500) / 1000);
        d = reduce_outgoing_damage(x, d);
        bool last = (i + 1 == count);

        add(p[x].no, p[it].no, 35, 0, p[it].hp, 0, "galahad_laser", p[x].name + " ", false);
        add(p[x].no, p[it].no, 35, 0, p[it].hp, 7, "galahad_laser", "激光枪射击", false);
        add(p[x].no, p[it].no, 35, 0, p[it].hp, 0, "galahad_laser", p[it].name + "，", false);
        int dealt = 0;
        int res = take_hit(x, it, d, 35, "galahad_laser", "galahad_laser_damage", "伤害", HURT_DIRECT, false, last, &dealt);
        clear_parry_action();
        if(res == 1 && dealt > 0)
        {
            p[it].burn_int += max(1LL, dealt * 10 / 100);
            p[it].burn_time += 2;
            add(p[x].no, p[it].no, 35, 0, p[it].hp, 0, "galahad_laser", "，被烧伤了", last);
        }
    }
}

// 魔女阶段技能：三条生命阈值只在首次跨越时排入自身待命队列，实际效果在魔女下一次行动执行。
void check_scientific_witch_phases(int x)
{
    if(p[x].alive == false) return;
    int galahad = 0;
    for(int i = 1; i <= n; ++i)
    {
        if(p[i].alive && p[i].is_galahad_one && p[i].owner == x) { galahad = i; break; }
    }

    if(galahad == 0) return;
    if(p[x].witch_phase_50_used == false && p[x].hp * 100 < get_maxhp(x) * 50)
    {
        p[x].witch_phase_50_used = true;
        p[x].tmp.push_back(witch_galahad_never_fall);
        p[x].tmp_free.push_back(false);
    }
    if(p[x].witch_phase_30_used == false && p[x].hp * 100 < get_maxhp(x) * 30)
    {
        p[x].witch_phase_30_used = true;
        p[x].tmp.push_back(witch_talila_tulila);
        p[x].tmp_free.push_back(false);
    }
    if(p[x].witch_phase_10_used == false && p[x].hp * 100 < get_maxhp(x) * 10)
    {
        p[x].witch_phase_10_used = true;
        p[x].tmp.push_back(witch_multiple_path);
        p[x].tmp_free.push_back(false);
    }
}

void witch_galahad_never_fall(int x)
{
    for(int i = 1; i <= n; ++i)
    {
        if(p[i].alive && p[i].is_galahad_one && p[i].owner == x)
        {
            p[i].galahad_laser_mode = true;
            add(p[x].no, p[i].no, 36, 0, p[x].hp, 16, "witch_phase_50", p[x].name + "使用加拉哈德你永远不会倒下", true);
            return;
        }
    }
}

void witch_talila_tulila(int x)
{
    for(int i = 1; i <= n; ++i)
    {
        if(p[i].alive && p[i].is_galahad_one && p[i].owner == x)
        {
            p[i].def += 100;
            p[i].sdef += 100;
            add(p[x].no, p[i].no, 37, 0, p[x].hp, 16, "witch_phase_30", p[x].name + "使用talila tulila，加拉哈德1号属性提升", true);
            return;
        }
    }
}

void witch_multiple_path(int x)
{
    for(int i = 1; i <= n; ++i)
    {
        if(p[i].alive && p[i].is_galahad_one && p[i].owner == x)
        {
            p[i].galahad_multi_target = true;
            add(p[x].no, p[i].no, 38, 0, p[x].hp, 16, "witch_phase_10", p[x].name + "使用多路化，加拉哈德1号的激光枪指向三个敌人", true);
            return;
        }
    }
}

// 戳刺伤害函数：目标随机选择，物理伤害为 max(1, 150%物攻 - 物防) 后施加 80%～120% 波动。
// 命中且目标存活（take_hit 返回 1）后，静默叠加 5 强度、2 层速度削减；被招架或击杀时不施加减速。
// 日志只输出技能名和伤害，减速状态通过 Cmd 快照同步到左侧状态徽记，而不额外写文本。
void stab(int x)
{
    int it = get_target(x); // 戳刺目标的座位编号。

    if(it == 0) return;

    int d = max(1LL , get_atk(x) * 150 / 100 - get_def(it)); // 以 150% 物攻减物防得到未波动基础伤害。
    int f = 800 + rnd() % 401; // 非法术物理伤害沿用普攻的 80%～120% 波动。
    d = max(1LL , (d * f + 500) / 1000);
    d = reduce_outgoing_damage(x , d);

    add(p[x].no , p[it].no , 8 , 0 , p[it].hp , 0 , "stab" , p[x].name , false);
    add(p[x].no , p[it].no , 8 , 0 , p[it].hp , 3 , "stab" , "戳刺" , false);
    add(p[x].no , p[it].no , 8 , 0 , p[it].hp , 0 , "stab" , "，" , false);
    int res = take_hit(x , it , d , 8 , "stab" , "stab_damage");
    clear_parry_action();
    if(res != 1) return;

    p[it].spd_down_int += 5;
    p[it].spd_down_time += 2;
}

// 会心一击伤害函数：目标随机选择，伤害为 max(1, 200%物攻 - 物防) 并施加 80%～120% 波动。
// 文本片段先写姓名、再以技能颜色写“会心一击”、最后委托 take_hit 输出命中/格挡/守护后的实际结果。
void critical_strike(int x)
{
    int it = get_target(x); // 会心一击目标的座位编号。

    if(it == 0) return;

    int d = max(1LL , get_atk(x) * 200 / 100 - get_def(it)); // 以 200% 物攻减物防得到未波动基础伤害。
    int f = 800 + rnd() % 401; // 会心一击沿用非法术物理伤害的 80%～120% 波动。
    d = max(1LL , (d * f + 500) / 1000);
    d = reduce_outgoing_damage(x , d);

    add(p[x].no , p[it].no , 13 , 0 , p[it].hp , 0 , "critical_strike" , p[x].name , false);
    add(p[x].no , p[it].no , 13 , 0 , p[it].hp , 2 , "critical_strike" , "会心一击" , false);
    add(p[x].no , p[it].no , 13 , 0 , p[it].hp , 0 , "critical_strike" , "，" , false);
    take_hit(x , it , d , 13 , "critical_strike" , "critical_strike_damage");
    clear_parry_action();
}

// 招架：非法术特殊技能，施放者获得两次自身回合的反击窗口；窗口内未触发时静默跳过行动并在第二次后强制结束。
void parry(int x)
{
    p[x].parry_time += 2;
    add(p[x].no , p[x].no , 14 , p[x].parry_time , p[x].parry_time , 0 , "parry" , p[x].name + "开始" , false);
    add(p[x].no , p[x].no , 14 , p[x].parry_time , p[x].parry_time , 10 , "parry" , "招架" , true);
}

// 张洋专属“串”：随机指定一名存活敌方目标，固定覆盖 16 强度、3 层防御削弱和伤害削弱。
// 本技能不计算伤害，也不进入 take_hit/receive_damage，因此不会触发招架、守护、反击、死亡或吞噬。
// 防御削弱令目标基础物防每级降低 5%；伤害削弱令目标之后造成的最终攻击伤害每级降低 5%，当前 16 强度即各保留 20%。
void string_skill(int x)
{
    int it = get_target(x); // “串”的随机敌方目标。

    if(it == 0) return;

    p[it].def_down_int = 16;
    p[it].def_down_time = 3;
    p[it].damage_down_int = 16;
    p[it].damage_down_time = 3;
    p[x].boss_string_count++;

    // 最后一条 Cmd 在状态已写入后生成，确保 React 首个文本片段就获得两种减益的权威快照。
    add(p[x].no , p[it].no , 24 , 0 , p[it].hp , 0 , "boss_string" , p[x].name + " " , false);
    add(p[x].no , p[it].no , 24 , 16 , p[it].hp , 5 , "boss_string" , "串" , false);
    add(p[x].no , p[it].no , 24 , 16 , p[it].hp , 0 , "boss_string" , " " + p[it].name + "，" , false);
    add(p[x].no , p[it].no , 24 , 16 , p[it].hp , 0 , "boss_string" , p[it].name + "放松警惕!" , true);
}

// 张洋专属“魔”：随机无重复指向最多三名存活敌方，向每人覆盖 1 强度、5 层方状态。
// 方不造成伤害，因此不会进入招架、守护、反击或死亡管线；它只让目标在状态存在期间的 get_mreg 返回 0。
// 所有目标的“被方住了”说明合并在同一行；具体层数仅由 Cmd 快照同步给 React，不重复写入日志。
void magic_square(int x)
{
    vector<int> target;

    for(int it = 1;it <= n;++ it) if(p[it].alive && p[it].id != p[x].id) target.push_back(it);
    for(int i = (int)target.size();i >= 2;-- i) swap(target[i - 1] , target[rnd_id(i) - 1]);
    int cnt = min(3LL , (int)target.size());
    if(cnt == 0) return;

    add(p[x].no , p[x].no , 26 , 0 , p[x].hp , 0 , "boss_magic" , p[x].name + "使用" , false);
    add(p[x].no , p[x].no , 26 , 0 , p[x].hp , 5 , "boss_magic" , "魔" , true);

    for(int i = 0;i < cnt;++ i)
    {
        int it = target[i];
        p[it].square_int = 1;
        p[it].square_time = 5;
        add(p[x].no , it , 26 , 1 , p[it].square_time , 0 , "boss_magic_square" , p[it].name + "被" , false);
        add(p[x].no , it , 26 , 1 , p[it].square_time , 5 , "boss_magic_square" , "方住了" + string(i + 1 == cnt ? "" : "，") , i + 1 == cnt);
    }

    p[x].boss_magic_ready = false;
}

// 守护是被动非法术；资格由 make_player 写入 has_guard，主动技能抽取会将其排除，因此此函数不会在自身回合执行。
void guard(int x)
{
}

// 投毒文本与状态函数：x 随机选择敌方 it，先输出“使用 投毒”；随后进行独立 50% 成功判定。
// 失败时只追加深绿色“失败”并结束该行，不改变任何战斗属性。成功时令中毒强度累加施法者物攻的 20%（最低 1），层数加一。
// 中毒并不在此刻扣血；行动结束时由持续伤害结算按 poison_time 次数重复调用 receive_damage，Cmd 快照会把新强度/层数同步给前端。
void poison(int x)
{
    int it = get_target(x); // 被投毒的敌方目标座位编号。

    if(it == 0) return;

    add(p[x].no , p[it].no , 12 , 0 , p[it].hp , 0 , "poison" , p[x].name + "使用" , false);
    add(p[x].no , p[it].no , 12 , 0 , p[it].hp , 14 , "poison" , "投毒" , false);

    if(rnd() % 100 >= 50)
    {
        add(p[x].no , p[it].no , 12 , 0 , p[it].hp , 14 , "poison_fail" , "失败" , true);
        return;
    }

    int d = max(1LL , get_atk(x) * 20 / 100); // 中毒强度为施法者物攻的 20%，最低保持为 1。
    p[it].posion_int += d;
    p[it].poison_time++;
    add(p[x].no , p[it].no , 12 , p[it].posion_int , p[it].poison_time , 0 , "poison_apply" , "，" + p[it].name + "获得" , false);
    add(p[x].no , p[it].no , 12 , p[it].posion_int , p[it].poison_time , 14 , "poison_apply" , to_string(p[it].posion_int) + "中毒" , false);
    add(p[x].no , p[it].no , 12 , p[it].posion_int , p[it].poison_time , 0 , "poison_apply" , "强度，层数为" , false);
    add(p[x].no , p[it].no , 12 , p[it].posion_int , p[it].poison_time , 14 , "poison_apply" , to_string(p[it].poison_time) , true);
}

// 吸血攻击伤害函数：随机选择敌人，造成 max(1, 100%物攻 - 物防) 的 80%～120% 波动物理伤害。
// take_hit 仅在目标存活时返回 1；此时把其最后一条“点伤害”片段改写为“伤害，吸取”，让回血文本与伤害仍位于同一日志行。
// 回血量按本次计算的 d 取 100%，再以最大生命封顶；实际被上限截断后的 heal 才写入 Cmd，确保前端浅绿色血条与 C++ 数值一致。
void lifesteal_attack(int x)
{
    int it = get_target(x); // 吸血攻击目标的座位编号。

    if(it == 0) return;

    int d = max(1LL , get_atk(x) - get_def(it)); // 100% 物攻倍率减物防得到未波动基础伤害。
    int f = 800 + rnd() % 401; // 非法术物理伤害沿用普攻的 80%～120% 波动。
    d = max(1LL , (d * f + 500) / 1000);
    d = reduce_outgoing_damage(x , d);

    add(p[x].no , p[it].no , 9 , 0 , p[it].hp , 0 , "lifesteal_attack" , p[x].name , false);
    add(p[x].no , p[it].no , 9 , 0 , p[it].hp , 3 , "lifesteal_attack" , "吸血攻击" , false);
    add(p[x].no , p[it].no , 9 , 0 , p[it].hp , 0 , "lifesteal_attack" , "，" , false);
    int res = take_hit(x , it , d , 9 , "lifesteal_attack" , "lifesteal_damage");
    clear_parry_action();
    if(res != 1) return;

    e.back().nl = false;
    e.back().str = "伤害，吸取";

    int before = p[x].hp; // 吸血前生命，用于限制有效恢复不超过最大生命。
    int heal = d;
    p[x].hp = min(get_maxhp(x) , p[x].hp + heal);
    heal = p[x].hp - before;
    add(p[x].no , p[x].no , 9 , heal , p[x].hp , 4 , "lifesteal_heal" , to_string(heal) , false);
    add(p[x].no , p[x].no , 9 , heal , p[x].hp , 0 , "lifesteal_heal" , "血量" , true);

}

// 火球术伤害函数：魔力由 use_skill/cost 在进入本函数前统一扣除；随机选择敌人，使用 magic_damage 计算 150% 魔攻法术伤害。
// take_hit 处理招架和守护；仅当目标存活时，才把刚结束的伤害后缀改为“点伤害，”并叠加烧伤强度（本次 d 的 5%）与 2 层持续时间。
// 后续烧伤在目标行动结束时结算，火球的 add 调用只负责“火球术”“被点燃了”及当前烧伤快照文本。
void fireball(int x)
{
    int it = get_target(x); // 火球术目标的座位编号。

    if(it == 0) return;

    add(p[x].no , it , 10 , 0 , p[it].hp , 0 , "fireball" , p[x].name + "使用" , false);
    add(p[x].no , it , 10 , 0 , p[it].hp , 13 , "fireball" , "火球术" , false);

    int d = magic_damage(x , it , 150); // 火球术基础伤害为魔攻的 150%，幻魔受常驻易损影响。
    d = reduce_outgoing_damage(x , d);

    int res = take_hit(x , it , d , 10 , "fireball" , "fireball_damage");
    clear_parry_action();
    if(res != 1) return;

    e.back().nl = false;
    e.back().str = "点伤害，";

    p[it].burn_int += d * 5 / 100;
    p[it].burn_time += 2;
    add(p[x].no , it , 10 , p[it].burn_int , p[it].burn_time , 0 , "burn_apply" , p[it].name + "被" , false);
    add(p[x].no , it , 10 , p[it].burn_int , p[it].burn_time , 13 , "burn_apply" , "点燃了" , false);
    add(p[x].no , it , 10 , p[it].burn_int , p[it].burn_time , 0 , "burn_apply" , "，获得" , false);
    add(p[x].no , it , 10 , p[it].burn_int , p[it].burn_time , 13 , "burn_apply" , to_string(p[it].burn_int) + "烧伤" , false);
    add(p[x].no , it , 10 , p[it].burn_int , p[it].burn_time , 0 , "burn_apply" , "强度，层数为2" , true);
}

// 瘟疫伤害函数：高智慧施法者优先锁定当前生命最高敌人，其他情况随机选敌；消耗魔力由外层统一结算。
// rate 在 [low, 100] 中均匀取值，low 会随魔攻从 70% 提高到最高 100%；伤害等于目标当前生命 * rate / 100，再对幻魔的魔法易损翻倍。
// 瘟疫明确绕过 take_hit，因此不会被招架或守护；它直接调用 receive_damage，随后手动输出红色伤害片段、死亡报告或 finish_hurt。
void plague(int x)
{
    int it = get_plague_target(x); // 高智慧优先当前生命最高的敌人，其他角色随机选择。

    if(it == 0) return;

    int low = 70 + min(30LL , max(0LL , (get_satk(x) - 100) * 30 / 100)); // 魔攻 100 时为 70%～100%，魔攻 200 时稳定为 100%。
    int rate = low + rnd_id(101 - low) - 1;
    int d = p[it].hp * rate / 100;
    if(p[it].magic_vuln > 0) d *= 2;

    add(p[x].no , it , 16 , rate , p[it].hp , 0 , "plague" , p[x].name + "使用" , false);
    add(p[x].no , it , 16 , rate , p[it].hp , 14 , "plague" , "瘟疫" , false);
    add(p[x].no , it , 16 , rate , p[it].hp , 0 , "plague" , "，" , false);
    int actual = reduce_k2_burn_source_damage(x , it , d);
    int res = receive_damage(it , actual , HURT_PLAGUE);
    add(p[x].no , it , 16 , actual , p[it].hp , 3 , "plague_damage" , to_string(actual) , false);
    add(p[x].no , it , 16 , actual , p[it].hp , 0 , "plague_damage" , "点伤害" , true);
    if(res == 2)
    {
        report_death(x , it , 16);
    }
    else if(res == 1) finish_hurt(x , it , HURT_PLAGUE);
    clear_parry_action();
}

// 生命之轮资源交换函数：选取当前生命最高敌人，直接交换双方当前 hp；它不是伤害结算，因此不调用 receive_damage/take_hit，也不会触发招架、守护、反击或垂死挣扎。
// 先完成 C++ 双方 hp 交换，再输出技能说明。之后按交换前的生命差单独生成红色“失去生命”和绿色“恢复生命”Cmd，仅用于前端条形动画。
// 两人原生命相同则只输出互换说明，不额外生成扣血或回血片段。
void life_wheel(int x)
{
    int it = get_life_wheel_target(x);

    if(it == 0) return;

    int before_x = p[x].hp;
    int before_it = p[it].hp;
    p[x].hp = before_it;
    p[it].hp = before_x;

    add(p[x].no , p[it].no , 19 , 0 , p[it].hp , 0 , "life_wheel" , p[x].name + "使用" , false);
    add(p[x].no , p[it].no , 19 , 0 , p[it].hp , 16 , "life_wheel" , "生命之轮" , false);
    add(p[x].no , p[it].no , 19 , 0 , p[it].hp , 0 , "life_wheel" , "，" + p[x].name + "与" + p[it].name + "的血量互换!" , true);

    if(before_it > before_x)
    {
        int d = before_it - before_x;
        add(p[x].no , p[it].no , 19 , 0 , p[it].hp , 0 , "life_wheel_damage" , p[it].name + "失去" , false);
        add(p[x].no , p[it].no , 19 , d , p[it].hp , 3 , "life_wheel_damage" , to_string(d) , false);
        add(p[x].no , p[it].no , 19 , d , p[it].hp , 0 , "life_wheel_damage" , "生命" , true);
        add(p[x].no , p[x].no , 19 , 0 , p[x].hp , 0 , "life_wheel" , p[x].name + "恢复" , false);
        add(p[x].no , p[x].no , 19 , d , p[x].hp , 4 , "life_wheel_heal" , to_string(d) , false);
        add(p[x].no , p[x].no , 19 , d , p[x].hp , 0 , "life_wheel_heal" , "生命" , true);
    }
    else if(before_x > before_it)
    {
        int d = before_x - before_it;
        add(p[x].no , p[x].no , 19 , 0 , p[x].hp , 0 , "life_wheel_damage" , p[x].name + "失去" , false);
        add(p[x].no , p[x].no , 19 , d , p[x].hp , 3 , "life_wheel_damage" , to_string(d) , false);
        add(p[x].no , p[x].no , 19 , d , p[x].hp , 0 , "life_wheel_damage" , "生命" , true);
        add(p[x].no , p[it].no , 19 , 0 , p[it].hp , 0 , "life_wheel" , p[it].name + "恢复" , false);
        add(p[x].no , p[it].no , 19 , d , p[it].hp , 4 , "life_wheel_heal" , to_string(d) , false);
        add(p[x].no , p[it].no , 19 , d , p[it].hp , 0 , "life_wheel_heal" , "生命" , true);
    }
}

// 雷击术多段伤害函数：随机选定一个敌人后，先输出技能名和 3～5 个空行 Cmd，制造雷击落下前的视觉间隔。
// 随后造成 3～5 段魔法伤害：普通段固定 30% 魔攻倍率，最后一段固定 80%；每段都通过 magic_damage（含幻魔易损）和 take_hit（含招架/守护）结算。
// 任一段击杀目标或施法者因反击死亡时立即停止余下段；最后 clear_parry_action 只在整次雷击结束后清理，以保证同一轮多段招架只触发一次反击。
void thunder(int x)
{
    int it = get_target(x); // 雷击目标的座位编号。

    if(it == 0) return;

    add(p[x].no , it , 2 , 0 , p[it].hp , 0 , "thunder" , p[x].name + "使用" , false);
    add(p[x].no , it , 2 , 0 , p[it].hp , 7 , "thunder" , "雷击术" , true);

    int gap = 3 + rnd() % 3; // 技能名后等待的空行数量，范围为 3～5。

    for(int i = 1;i <= gap;++ i) add(p[x].no , it , 2 , 0 , p[it].hp , 0 , "thunder_gap" , "" , true);

    int cnt = 3 + rnd() % 3; // 本次雷击的总段数，范围为 3～5。

    for(int i = 1;i <= cnt;++ i)
    {
        int mul = 30; // 当前段的魔攻倍率百分比。
        if(i == cnt) mul = 80;

        int d = magic_damage(x , it , mul); // 与 0 取最大值的法术基础伤害，幻魔受常驻易损影响。
        d = reduce_outgoing_damage(x , d);

        int res = take_hit(x , it , d , 2 , "thunder" , i == cnt ? "thunder_damage_last" : "thunder_damage");
        if(p[x].alive == false || res == 2)
        {
            clear_parry_action();
            return;
        }
    }

    clear_parry_action();
}

// 地裂术多目标伤害函数：先收集所有存活敌人，以 Fisher-Yates 洗牌后无重复选取 4～6 名；目标不足四名时会攻击全部敌人。
// 先以独立换行输出“地裂术”，随后每名目标承受一次 80% 魔攻的 magic_damage，并逐个通过 take_hit 结算招架、守护、死亡和反击。
// 地裂共享同一攻击动作的招架标记；只有遍历完成或施法者死亡时才 clear_parry_action，避免前一个目标的招架影响同次剩余目标的错误次数。
void earthquake(int x)
{
    vector<int> target; // 本次可供地裂术无重复抽取的存活敌方座位。

    for(int it = 1;it <= n;++ it)
    {
        if(p[it].alive && p[it].id != p[x].id) target.push_back(it);
    }

    if(target.empty()) return;

    for(int i = (int)target.size();i >= 2;-- i)
    {
        int j = rnd_id(i) - 1; // Fisher-Yates 随机置换，保持本局种子确定性。
        swap(target[i - 1] , target[j]);
    }

    int cnt = min((int)target.size() , 4 + rnd() % 3); // 敌人不足四名时自然收缩为全部目标数。

    add(p[x].no , p[x].no , 6 , 0 , p[x].hp , 0 , "earthquake" , p[x].name + "使用" , false);
    add(p[x].no , p[x].no , 6 , 0 , p[x].hp , 11 , "earthquake" , "地裂术" , true);

    for(int i = 1;i <= cnt;++ i)
    {
        int it = target[i - 1]; // 每名敌人本次至多受到一段地裂伤害。

        int d = magic_damage(x , it , 80); // 地裂术基础伤害为魔攻的 80%，幻魔受常驻易损影响。
        d = reduce_outgoing_damage(x , d);

        take_hit(x , it , d , 6 , "earthquake" , "earthquake_damage");
        if(p[x].alive == false)
        {
            clear_parry_action();
            return;
        }
    }

    clear_parry_action();
}

// 治愈魔法：治疗同队未满血成员；智力高于 100 时优先救治当前生命最低者。
void heal_magic(int x)
{
    int it = get_heal_target(x); // 由智力决定的实际受治疗友方座位。

    if(it == 0) return;

    int d = get_satk(x) * 120 / 100; // 未波动的治疗基础值，为魔攻的 120%。
    int f = 900 + rnd() % 201; // 90%～110% 的治疗波动系数。
    d = max(0LL , (d * f + 500) / 1000);
    int before = p[it].hp; // 治疗前生命，用于计算实际有效治疗量。
    p[it].hp = min(get_maxhp(it) , p[it].hp + d);
    d = p[it].hp - before;

    // 发动描述和有效治疗量共用一行，前端据 heal 片段播放浅绿色待恢复生命条。
    add(p[x].no , p[it].no , 4 , 0 , p[it].hp , 0 , "heal_magic" , p[x].name + "使用" , false);
    add(p[x].no , p[it].no , 4 , 0 , p[it].hp , 4 , "heal_magic" , "治愈魔法" , false);
    add(p[x].no , p[it].no , 4 , 0 , p[it].hp , 0 , "heal_magic" , "，为" + p[it].name + "恢复" , false);
    add(p[x].no , p[it].no , 4 , d , p[it].hp , 4 , "heal" , to_string(d) , false);
    add(p[x].no , p[it].no , 4 , d , p[it].hp , 0 , "heal" , "生命" , true);
}

// 双岛牛奶：mili 自身恢复最大生命的 30%，并清除全部可清除负面状态；不改变正面强化。
void double_island_milk(int x)
{
    p[x].freeze = 0; p[x].freeze_int = 0;
    p[x].def_down_int = 0; p[x].def_down_time = 0;
    p[x].damage_down_int = 0; p[x].damage_down_time = 0;
    p[x].square_int = 0; p[x].square_time = 0;
    p[x].spd_down_int = 0; p[x].spd_down_time = 0;
    p[x].burn_int = 0; p[x].burn_time = 0;
    p[x].posion_int = 0; p[x].poison_time = 0;
    p[x].parry_time = 0;
    p[x].lament_time = 0;
    int before = p[x].hp;
    int d = get_maxhp(x) * 30 / 100;
    p[x].hp = min(get_maxhp(x), p[x].hp + d);
    d = p[x].hp - before;
    add(p[x].no, p[x].no, 27, 0, p[x].hp, 0, "mili_milk", "mili饮用", false);
    add(p[x].no, p[x].no, 27, 0, p[x].hp, 10, "mili_milk", "双岛牛奶!! ", false);
    add(p[x].no, p[x].no, 27, d, p[x].hp, 4, "heal", "mili恢复" + to_string(d) + "生命", true);
    p[x].mili_milk_count++;
    if(p[x].mili_milk_count >= 5) world_execute(x);
}

// Lament：mili 对一名存活敌人施加 2 层哀悼。哀悼者的每次自身行动均会改为无差别普通攻击，层数随该行动消耗。
void lament(int x)
{
    int it = get_target(x);
    if(it == 0) return;

    p[it].lament_time += 2;
    add(p[x].no , p[it].no , 35 , 0 , p[it].hp , 0 , "lament" , "mili使用", false);
    add(p[x].no , p[it].no , 35 , 0 , p[it].hp , 3 , "lament" , "Lament", false);
    add(p[x].no , p[it].no , 35 , 0 , p[it].hp , 0 , "lament" , "，" + p[it].name + "获得2层", false);
    add(p[x].no , p[it].no , 35 , 0 , p[it].hp , 3 , "lament" , "哀悼", true);
}

// world.search(you);：对一名存活敌方造成 250% 物攻减物防的直接物理伤害，仍经过统一命中、守护、格挡和死亡管线。
void world_search(int x)
{
    int it = get_target(x);
    if(it == 0) return;

    int d = max(1LL , get_atk(x) * 250 / 100 - get_def(it));
    d = reduce_outgoing_damage(x , d);
    add(p[x].no , p[it].no , 36 , 0 , p[it].hp , 0 , "world_search" , "mili使用", false);
    add(p[x].no , p[it].no , 36 , 0 , p[it].hp , 0 , "world_search" , "world.search(you);", false);
    add(p[x].no , p[it].no , 36 , 0 , p[it].hp , 0 , "world_search" , "，", false);
    take_hit(x , it , d , 36 , "world_search" , "world_search_damage");
    clear_parry_action();
}

// world.execute(me);：每一次命中都先发出独立的 execute 视觉事件，再以当时全场最低存活生命作为绝对伤害结算。
// 该终局伤害不走格挡、守护、反击或垂死挣扎；每轮至少会击杀当前最低生命者，直到全场仅余一名存活单位。
void world_execute(int x)
{
    add(p[x].no , p[x].no , 37 , 0 , p[x].hp , 0 , "world_execute" , "mili使用", false);
    add(p[x].no , p[x].no , 37 , 0 , p[x].hp , 0 , "world_execute" , "world.execute(me);", true);
    dismiss_familiars(x);
    world_execute_finished = true;

    while(true)
    {
        int alive_count = 0;
        int lowest_hp = 0;
        for(int it = 1;it <= n;++ it)
        {
            if(p[it].alive == false) continue;
            alive_count++;
            if(lowest_hp == 0 || p[it].hp < lowest_hp) lowest_hp = p[it].hp;
        }
        if(alive_count <= 1 || lowest_hp <= 0) return;

        vector<int> targets;
        for(int it = 1;it <= n;++ it) if(p[it].alive) targets.push_back(it);
        for(int it : targets)
        {
            int remaining = 0;
            for(int check = 1;check <= n;++ check) if(p[check].alive) remaining++;
            if(remaining <= 1) return;
            if(p[it].alive == false) continue;

            add(p[x].no , p[it].no , 37 , 0 , p[it].hp , 0 , "world_execute" , "execute", true);
            int result = receive_damage(it , lowest_hp , HURT_LIFE_WHEEL);
            add(p[x].no , p[it].no , 37 , lowest_hp , p[it].hp , 3 , "world_execute_damage" , p[it].name + "受到" + to_string(lowest_hp) + "伤害", true);
            if(result == 2) report_death(0 , it , 37);
        }
    }
}

// 张洋偷到复苏术时的替代结算：不触发治愈魔法的选目标与施法文本，只按相同公式为张洋自身恢复生命。
// 仅输出绿色数值 Cmd，让前端播放与治愈等价的生命条恢复动画；“张洋没偷到，美美补觉”由 steal_skill 负责提前说明。
void boss_nap_heal(int x)
{
    int d = get_satk(x) * 120 / 100;
    int f = 900 + rnd() % 201;
    d = max(0LL , (d * f + 500) / 1000);
    int before = p[x].hp;
    p[x].hp = min(get_maxhp(x) , p[x].hp + d);
    d = p[x].hp - before;
    add(p[x].no , p[x].no , 25 , d , p[x].hp , 4 , "heal" , to_string(d) , true);
}

// 复苏术：消耗由 cost 统一结算，每队每局仅成功使用一次；令一名死亡队友以 60% 最大生命重新加入战斗，清除负面状态并给予 20000 行动值。
void revive(int x)
{
    int it = get_revive_target(x);

    if(it == 0 || revive_used[p[x].id]) return;

    revive_used[p[x].id] = true;
    int hp = get_maxhp(it) * 60 / 100;
    p[it].hp = hp;
    p[it].alive = true;
    p[it].freeze = 0;
    p[it].freeze_int = 0;
    p[it].def_down_int = 0;
    p[it].def_down_time = 0;
    p[it].damage_down_int = 0;
    p[it].damage_down_time = 0;
    p[it].square_int = 0;
    p[it].square_time = 0;
    p[it].spd_down_int = 0;
    p[it].spd_down_time = 0;
    p[it].burn_int = 0;
    p[it].burn_time = 0;
    p[it].posion_int = 0;
    p[it].poison_time = 0;
    p[it].tmp.clear();
    p[it].tmp_free.clear();
    p[it].act += 20000;
    p[it].lament_time = 0;
    add(p[x].no , it , 17 , hp , p[it].hp , 4 , "revive_heal" , p[it].name + "复活" , true);
}

// 净化：消耗由 cost 统一结算，移除友方全部负面状态，并恢复其 10% 魔攻生命。
void purify(int x)
{
    int it = get_purify_target(x);

    if(it == 0) return;

    p[it].freeze = 0;
    p[it].freeze_int = 0;
    p[it].def_down_int = 0;
    p[it].def_down_time = 0;
    p[it].damage_down_int = 0;
    p[it].damage_down_time = 0;
    p[it].spd_down_int = 0;
    p[it].spd_down_time = 0;
    p[it].burn_int = 0;
    p[it].burn_time = 0;
    p[it].posion_int = 0;
    p[it].poison_time = 0;
    p[it].lament_time = 0;
    p[it].tmp.clear();
    p[it].tmp_free.clear();
    int before = p[it].hp;
    int d = get_satk(x) * 10 / 100;
    p[it].hp = min(get_maxhp(it) , p[it].hp + d);
    d = p[it].hp - before;

    add(p[x].no , p[it].no , 11 , 0 , p[it].hp , 0 , "purify" , p[x].name + "使用" , false);
    add(p[x].no , p[it].no , 11 , 0 , p[it].hp , 4 , "purify" , "净化" , false);
    add(p[x].no , p[it].no , 11 , 0 , p[it].hp , 0 , "purify" , "，为" + p[it].name + "清除所有负面状态，恢复" , false);
    add(p[x].no , p[it].no , 11 , d , p[it].hp , 4 , "status_heal" , to_string(d) , false);
    add(p[x].no , p[it].no , 11 , d , p[it].hp , 0 , "status_heal" , "生命" , true);
}

// 选择铁壁目标：高智力施法者优先强化同队低于 40% 生命且生命最低者，否则强化自己。
int get_ironwall_target(int x)
{
    if(get_iq(x) > 100)
    {
        int ans = 0; // 当前生命最低的濒危同队成员。

        for(int it = 1;it <= n;++ it)
        {
            if(p[it].alive == false || p[it].id != p[x].id) continue;
            if(p[it].hp * 100 >= get_maxhp(it) * 40) continue;
            if(ans == 0 || p[it].hp < p[ans].hp) ans = it;
        }

        if(ans != 0) return ans;
    }

    return x;
}

// 疾走术：消耗由 cost 统一结算，给予自身 5 强度速度强化，覆盖之后十个自身回合并在下一回合开始时解除。
void rage(int x)
{
    p[x].spd_up_int += 5;
    p[x].spd_up_time += 10;
    add(p[x].no , p[x].no , 7 , 5 , get_spd(x) , 0 , "rage" , p[x].name + "发动" , false);
    add(p[x].no , p[x].no , 7 , 5 , get_spd(x) , 15 , "rage" , "疾走术" , true);
}

// 快速行动：施法者先获得 20000 行动值；再将另一份 20000 行动值随机拆分给存活队友，没有队友时整份给予自身。
void fast_action(int x)
{
    vector<int> ally;

    for(int i = 1;i <= n;++ i) if(i != x && p[i].alive && p[i].id == p[x].id) ally.push_back(i);

    p[x].act += 20000;

    if(ally.empty()) ally.push_back(x);

    int rest = 20000;

    for(int i = 0;i < (int)ally.size();++ i)
    {
        int min_rest = (int)ally.size() - i - 1;
        int give = i + 1 == (int)ally.size() ? rest : rnd_id(rest - min_rest);
        rest -= give;
        p[ally[i]].act += give;
    }
}

// 铁壁：消耗由 cost 统一结算，给予目标 100% 普通防御强化，覆盖三个自身回合并在第四个开始时解除。
void ironwall(int x)
{
    int it = get_ironwall_target(x); // 本次实际获得防御强化的友方座位。
    p[it].def_plus_int += 100;
    p[it].def_plus_time += 4;
    add(p[x].no , p[it].no , 5 , 0 , get_def(it) , 0 , "ironwall" , p[x].name + "发动" , false);
    add(p[x].no , p[it].no , 5 , 0 , get_def(it) , 10 , "ironwall" , "铁壁" , false);
    add(p[x].no , p[it].no , 5 , 0 , get_def(it) , 0 , "ironwall" , "，" + p[it].name + "防御力大幅提升!!!" , true);
}

// 解除冻结束缚技能：每次尝试有 75% 概率消去 1 层，失败时将自身重新插回队首。
void unfreeze(int x)
{
    if(p[x].freeze == 0) return;

    if(rnd() % 100 >= 75)
    {
        p[x].tmp.push_front(unfreeze);
        p[x].tmp_free.push_front(false);
        add(p[x].no , p[x].no , 4 , 0 , p[x].freeze , 0 , "unfreeze_fail" , p[x].name + " 解冻 失败" , true);
        return;
    }

    p[x].freeze--;

    if(p[x].freeze > 0)
    {
        p[x].tmp.push_front(unfreeze);
        p[x].tmp_free.push_front(false);
        add(p[x].no , p[x].no , 4 , 0 , p[x].freeze , 0 , "status_sync" , "" , true);
        return;
    }

    p[x].freeze_int = 0;

    add(p[x].no , p[x].no , 4 , 0 , 0 , 0 , "unfreeze" , p[x].name + "从 " , false);
    add(p[x].no , p[x].no , 4 , 0 , 0 , 8 , "unfreeze" , "冻结" , false);
    add(p[x].no , p[x].no , 4 , 0 , 0 , 0 , "unfreeze" , " 中 解除" , true);
}

// 冰冻术：对一个敌人造成 25% 魔攻倍率法术伤害，存活目标获得 3 层冻结。
void ice(int x)
{
    int it = get_target(x); // 冰冻术目标的座位编号。

    if(it == 0) return;

    add(p[x].no , it , 3 , 0 , p[it].hp , 0 , "ice" , p[x].name + "使用" , false);
    add(p[x].no , it , 3 , 0 , p[it].hp , 2 , "ice" , "冰冻术" , true);

    int d = magic_damage(x , it , 25); // 与 0 取最大值的冰冻术基础伤害，幻魔受常驻易损影响。
    d = reduce_outgoing_damage(x , d);

    int res = take_hit(x , it , d , 3 , "ice" , "ice_damage");
    clear_parry_action();
    if(res != 1) return;

    p[it].freeze_int += 1;
    p[it].freeze += 3;
    p[it].tmp.push_front(unfreeze);
    p[it].tmp_free.push_front(false);
    add(p[x].no , it , 3 , p[it].freeze , p[it].freeze , 0 , "freeze_apply" , "，获得" , false);
    add(p[x].no , it , 3 , p[it].freeze , p[it].freeze , 8 , "freeze_apply" , "冻结" , false);
    add(p[x].no , it , 3 , p[it].freeze , p[it].freeze , 0 , "freeze_apply" , " buff " , false);
    add(p[x].no , it , 3 , p[it].freeze , p[it].freeze , 9 , "freeze_apply" , to_string(p[it].freeze) , false);
    add(p[x].no , it , 3 , p[it].freeze , p[it].freeze , 0 , "freeze_apply" , "层" , true);
}

// 判断函数指针 f 是否为需要消耗魔力的法术技能。
bool spell(void (*f)(int))
{
    return f == thunder || f == earthquake || f == rage || f == fast_action || f == plague || f == revive || f == life_wheel || f == ice || f == heal_magic || f == purify || f == ironwall || f == fireball || f == summon;
}

// 返回某个法术函数的固定魔力消耗；非魔法技能返回 0。
int cost(void (*f)(int))
{
    if(f == thunder) return 60;
    if(f == earthquake) return 120;
    if(f == rage) return 75;
    if(f == fast_action) return 70;
    if(f == plague) return 160;
    if(f == revive) return 200;
    if(f == life_wheel) return 180;
    if(f == ice) return 160;
    if(f == heal_magic) return 150;
    if(f == purify) return 50;
    if(f == ironwall) return 80;
    if(f == fireball) return 100;
    if(f == summon) return 40;

    return 0;
}

// 判断玩家 x 的第 i 个技能槽能否参加本次特殊技能共享加权抽取；魔力不足的法术不会进入候选池。
bool pickable_special(int x , int i)
{
    if(p[x].skill[i] == nullptr || p[x].skill[i] == basic_attack || p[x].can[i] == false) return false;
    if(p[x].skill[i] == guard) return false;
    if(spell(p[x].skill[i]) && p[x].magic < cost(p[x].skill[i])) return false;
    if(p[x].skill[i] == heal_magic && has_heal_target(x) == false) return false;
    if(p[x].skill[i] == revive && has_revive_target(x) == false) return false;
    if(p[x].skill[i] == purify && has_purify_target(x) == false) return false;
    return true;
}

// 张洋偷取的召唤技能也必须保留原本的召唤阈值；普通“召唤”当前按规则无数量上限，因此不在此限制。
bool stolen_skill_within_summon_cap(int x , void (*f)(int))
{
    if(f == summon_moon_child) return has_living_moon_child(x) == false;
    if(f == summon_k2) return has_living_k2(x) == false;
    if(f == summon_scientific_witch) return p[x].mili_witch_summoned == false && has_living_scientific_witch(x) == false;
    return true;
}

// 直接按角色此刻的可用资格、魔力和权重抽取一项合格特殊技能，但不执行它。
// 该函数只服务于张洋的偷，刻意不走普通行动的“是否选择特殊技能”概率；没有合格特殊技能时返回 nullptr，由调用方以串替代。
// 张洋的偷取权重额外增加 30，只作用于本次被偷目标的特殊技能候选，不改变目标自己的正常行动权重。
void (*generate_ready_skill(int x))(int)
{
    int sum = 0;
    for(int i = 1;i <= p[x].sn;++ i) if(pickable_special(x , i)) sum += special_weight(x , i) + 30;
    if(sum == 0) return nullptr;

    int k = rnd_id(sum);

    for(int i = 1;i <= p[x].sn;++ i)
    {
        if(pickable_special(x , i) == false) continue;
        k -= special_weight(x , i) + 30;
        if(k <= 0) return p[x].skill[i];
    }

    return nullptr;
}

// 将函数指针转为“偷”日志可读的中文技能名称；名字仅用于 C++ 生成的前端文本，不参与任何战斗判定。
string skill_display_name(void (*f)(int))
{
    if(f == basic_attack) return "普通攻击";
    if(f == thunder) return "雷击术";
    if(f == earthquake) return "地裂术";
    if(f == rage) return "疾走术";
    if(f == ice) return "冰冻术";
    if(f == heal_magic) return "治愈魔法";
    if(f == purify) return "净化";
    if(f == ironwall) return "铁壁";
    if(f == stab) return "戳刺";
    if(f == lifesteal_attack) return "吸血攻击";
    if(f == poison) return "投毒";
    if(f == critical_strike) return "会心一击";
    if(f == parry) return "招架";
    if(f == fireball) return "火球术";
    if(f == plague) return "瘟疫";
    if(f == fast_action) return "快速行动";
    if(f == revive) return "复苏术";
    if(f == life_wheel) return "生命之轮";
    if(f == summon) return "召唤";
    if(f == double_island_milk) return "双岛牛奶";
    if(f == summon_moon_child) return "召唤月之子";
    if(f == moon_chant) return "吟唱";
    if(f == summon_k2) return "召唤K-2";
    if(f == iron_blood_lotus) return "Iron Lotus";
    if(f == summon_scientific_witch) return "召唤科学性实验魔女";
    if(f == lament) return "Lament";
    if(f == lancelot_zero_attack) return "攻击";
    if(f == brew_rebirth_potion) return "调制重生药水";
    if(f == world_search) return "world.search(you);";
    if(f == world_execute) return "world.execute(me);";
    if(f == unfreeze) return "解除冻结";
    if(f == string_skill) return "串";
    return "未知技能";
}

// 返回偷取执行确认文字中技能名应使用的既有色号；只影响 C++ 命令流展示，不参与技能结算。
int stolen_skill_tone(void (*f)(int))
{
    if(f == thunder) return 7;
    if(f == earthquake) return 11;
    if(f == rage) return 12;
    if(f == ice) return 8;
    if(f == heal_magic || f == purify || f == revive) return 4;
    if(f == ironwall || f == parry) return 10;
    if(f == fireball) return 13;
    if(f == plague || f == poison) return 14;
    if(f == fast_action) return 15;
    if(f == life_wheel) return 16;
    if(f == summon) return 17;
    if(f == double_island_milk) return 10;
    if(f == summon_moon_child) return 16;
    if(f == moon_chant) return 16;
    if(f == summon_k2) return 10;
    if(f == iron_blood_lotus) return 3;
    if(f == lament) return 3;
    if(f == summon_scientific_witch) return 10;
    if(f == lancelot_zero_attack) return 2;
    if(f == brew_rebirth_potion) return 4;
    if(f == world_search || f == world_execute) return 16;
    if(f == string_skill) return 5;
    if(f == stab || f == lifesteal_attack) return 3;
    return 2;
}

// 张洋专属“偷”：累计三次串后随机选择三次存活敌方，同一目标可重复被选中；每次直接按其当前合格特殊技能权重复制一项。
// 没有合格特殊技能时改为串。偷到的法术全部无视张洋当前魔力和原始消耗。
// 多个招架会被延后：当前三项中仅保留一个在末尾施放，其他招架继续留在待命队列。偷到复苏术时改为张洋自身的静默等价回血。
void steal_skill(int x)
{
    vector<int> target;
    vector<void (*)(int)> now;
    vector<void (*)(int)> later_parry;
    void (*first_parry)(int) = nullptr;

    for(int it = 1;it <= n;++ it) if(p[it].alive && p[it].id != p[x].id) target.push_back(it);
    int cnt = target.empty() ? 0 : 3;

    for(int i = 0;i < cnt;++ i)
    {
        int it = target[rnd_id((int)target.size()) - 1];
        void (*f)(int) = nullptr;

        f = generate_ready_skill(it);

        // world.execute(me); 属于结束技。张洋即使复制到它，也只会改为可正常结算的 world.search(you);。
        if(f == world_execute) f = world_search;

        if(f == nullptr || f == basic_attack || f == unfreeze)
        {
            add(p[x].no , it , 25 , 0 , p[it].hp , 0 , "boss_steal_fail" , p[x].name + "啥也没偷到，" , false);
            add(p[x].no , it , 25 , 0 , p[it].hp , 0 , "boss_steal_fail" , "美美开串" , true);
            now.push_back(string_skill);
            continue;
        }
        if(f == revive)
        {
            add(p[x].no , it , 25 , 0 , p[x].hp , 0 , "boss_steal_nap" , p[x].name + "没偷到，" , false);
            add(p[x].no , it , 25 , 0 , p[x].hp , 0 , "boss_steal_nap" , "美美补觉" , true);
            boss_nap_heal(x);
            continue;
        }
        if(f == parry)
        {
            if(first_parry == nullptr) first_parry = f;
            else later_parry.push_back(f);
        }
        else now.push_back(f);
        add(p[x].no , p[it].no , 25 , 0 , p[it].hp , 0 , "boss_steal" , p[x].name + " " , false);
        add(p[x].no , p[it].no , 25 , 0 , p[it].hp , 3 , "boss_steal" , "偷" , false);
        add(p[x].no , p[it].no , 25 , 0 , p[it].hp , 0 , "boss_steal" , "了" + p[it].name + "的" + skill_display_name(f) , false);
        add(p[x].no , p[it].no , 25 , 0 , p[it].hp , 0 , "boss_steal" , "!" , true);
    }

    for(void (*f)(int) : now)
    {
        p[x].tmp.push_back(f);
        p[x].tmp_free.push_back(true);
    }
    if(first_parry != nullptr)
    {
        p[x].tmp.push_back(first_parry);
        p[x].tmp_free.push_back(true);
    }
    for(void (*f)(int) : later_parry)
    {
        p[x].tmp.push_back(f);
        p[x].tmp_free.push_back(true);
    }

    p[x].act += 60000;
    p[x].boss_string_count = 0;
    p[x].boss_magic_ready = true;
}

// 返回技能槽 i 的共享抽取权重；低于 40% 血量且智力高于 100 时，治愈取得智力驱动的大幅优先级。
int special_weight(int x , int i)
{
    if(p[x].skill[i] == heal_magic && p[x].hp * 100 < get_maxhp(x) * 40 && get_iq(x) > 100)
    {
        return p[x].w[i] + (get_iq(x) - 100) * 1000000;
    }

    if(p[x].skill[i] == revive && has_revive_target(x)) return p[x].w[i] + 100000000;

    if(p[x].skill[i] == life_wheel && p[x].hp * 100 < get_maxhp(x) * 20) return p[x].w[i] + 100000000;

    return p[x].w[i];
}

// 返回本次行动选择特殊技能的百分比；智慧不超过 150 时每增加 5 点增加 1%，超过 150 后每增加 1 点增加 1%，最多 100%。
int special_chance(int x)
{
    int iq = get_iq(x);
    if(iq <= 150) return min(100LL , max(1LL , (iq - 100) / 5 + 1));
    return min(100LL , 11 + iq - 150);
}

// 每个玩家自身回合开始时结算持续状态；铁壁覆盖之后三个自身回合，暴走覆盖之后四个自身回合，速度削减静默持续指定层数。
void begin_turn(int x)
{
    if(p[x].def_plus_time > 0)
    {
        p[x].def_plus_time--;
        if(p[x].def_plus_time == 0)
        {
            p[x].def_plus_int = 0;
            add(p[x].no , p[x].no , 5 , 0 , get_def(x) , 0 , "ironwall_remove" , p[x].name + "从" , false);
            add(p[x].no , p[x].no , 5 , 0 , get_def(x) , 10 , "ironwall_remove" , "铁壁" , false);
            add(p[x].no , p[x].no , 5 , 0 , get_def(x) , 0 , "ironwall_remove" , "中解除" , true);
        }
    }

    if(p[x].def_down_time > 0)
    {
        p[x].def_down_time--;
        if(p[x].def_down_time == 0) p[x].def_down_int = 0;
    }

    if(p[x].damage_down_time > 0)
    {
        p[x].damage_down_time--;
        if(p[x].damage_down_time == 0) p[x].damage_down_int = 0;
    }

    if(p[x].spd_up_time > 0)
    {
        p[x].spd_up_time--;
        if(p[x].spd_up_time == 0)
        {
            p[x].spd_up_int = 0;
            add(p[x].no , p[x].no , 7 , 0 , get_spd(x) , 0 , "rage_remove" , p[x].name + "从" , false);
            add(p[x].no , p[x].no , 7 , 0 , get_spd(x) , 15 , "rage_remove" , "疾走术" , false);
            add(p[x].no , p[x].no , 7 , 0 , get_spd(x) , 0 , "rage_remove" , "中解除" , true);
        }
    }

    if(p[x].spd_down_time > 0)
    {
        p[x].spd_down_time--;
        if(p[x].spd_down_time == 0) p[x].spd_down_int = 0;
    }
}

// 在自身行动开始时消耗一次未触发的招架窗口；返回 true 时本次行动必须静默跳过。
bool skip_parry_turn(int x)
{
    if(p[x].parry_time == 0) return false;

    p[x].parry_time--;
    return true;
}

// 回合末持续伤害结算器：只处理自身行动结束后的烧伤和中毒，死亡单位不会再次结算。
// 烧伤每回合只造成一次 burn_int 伤害并消耗一层 burn_time；中毒则以当前 poison_time 为次数，对每层各造成一次 posion_int 伤害。
// 两者直接调用 receive_damage，故不会被招架或守护；死亡后以 source=0 调用 report_death，且 finish_hurt 收到无来源后不会错误触发反击。
// 所有 add 调用都把“受到 + 红色数字 + 状态名”拆成片段：数值保持伤害色，状态名保持专属色，便于前端分别做文字与血条动画。
void end_turn(int x)
{
    if(p[x].alive == false) return;

    if(p[x].square_time > 0)
    {
        p[x].square_time--;
        if(p[x].square_time == 0) p[x].square_int = 0;
        add(p[x].no , p[x].no , 26 , 0 , p[x].square_time , 0 , "status_sync" , "" , false);
    }

    if(p[x].burn_time > 0)
    {
        int d = p[x].burn_int; // 本次烧伤通过统一伤害函数扣除的固定生命值；烧伤层数只控制持续回合，不会令单次 d 乘层。
        int res = receive_damage(x , d , HURT_BURN);
        p[x].burn_time--; // 烧伤固定每个自身回合减少一层，即使本次伤害令目标濒死也先记录新的状态快照。
        add(p[x].no , p[x].no , 10 , 0 , p[x].hp , 0 , "burn_tick" , p[x].name + "受到" , false);
        add(p[x].no , p[x].no , 10 , d , p[x].hp , 3 , "burn_damage" , to_string(d) , false);
        add(p[x].no , p[x].no , 10 , d , p[x].hp , 0 , "burn_tick" , "点" , false);
        add(p[x].no , p[x].no , 10 , d , p[x].hp , 13 , "burn_tick" , "烧伤" , false);

        if(p[x].burn_time == 0)
        {
            p[x].burn_int = 0;
            add(p[x].no , p[x].no , 10 , 0 , p[x].hp , 0 , "burn_remove" , "伤害，从" , false);
            add(p[x].no , p[x].no , 10 , 0 , p[x].hp , 13 , "burn_remove" , "烧伤" , false);
            add(p[x].no , p[x].no , 10 , 0 , p[x].hp , 0 , "burn_remove" , "中解除" , true);
        }
        else add(p[x].no , p[x].no , 10 , 0 , p[x].hp , 0 , "burn_tick" , "伤害" , true);

        if(res == 2)
        {
            report_death(0 , x , 10);
            return;
        }
        if(res == 1) finish_hurt(0 , x , HURT_BURN);
    }

    if(p[x].poison_time == 0)
    {
        if(p[x].is_k2 && p[x].alive) trigger_k2_extra_burn(x);
        return;
    }

        int d = p[x].posion_int; // 每层中毒各自结算一次的固定毒素伤害；重复投毒累加的是该单次伤害值。
        int cnt = p[x].poison_time; // 在循环前冻结本回合的结算次数，避免死亡、净化或其他被动中途改变层数导致次数漂移。

    for(int i = 1;i <= cnt;++ i)
    {
        int res = receive_damage(x , d , HURT_POISON);
        add(p[x].no , p[x].no , 12 , 0 , p[x].hp , 0 , "poison_tick" , p[x].name + "受到" , false);
        add(p[x].no , p[x].no , 12 , d , p[x].hp , 3 , "poison_damage" , to_string(d) , false);
        add(p[x].no , p[x].no , 12 , d , p[x].hp , 0 , "poison_tick" , "点" , false);
        add(p[x].no , p[x].no , 12 , d , p[x].hp , 14 , "poison_tick" , "毒素伤害" , true);

        if(res == 2)
        {
            report_death(0 , x , 12);
            return;
        }
        if(res == 1) finish_hurt(0 , x , HURT_POISON);
        if(p[x].poison_time == 0) return; // 被动效果若已在结算中清除毒素，则立即停止剩余层的伤害。
    }

    if(rnd() % 100 < 5) // 所有层数均结算完成后独立进行 5% 解毒判定；成功时一次性清空强度与层数。
    {
        p[x].posion_int = 0;
        p[x].poison_time = 0;
        add(p[x].no , p[x].no , 12 , 0 , p[x].poison_time , 14 , "poison_remove" , "解毒成功" , true);
        return;
    }

    add(p[x].no , p[x].no , 12 , 0 , p[x].poison_time , 14 , "poison_fail" , "解毒失败" , true);
    if(p[x].is_k2 && p[x].alive) trigger_k2_extra_burn(x);
}

// 让玩家 x 执行一次行动：优先执行 tmp 队列中的束缚技能，否则先按智慧判定特殊分支，再统一加权抽取法术或非法术。
void use_skill(int x)
{
    // 哀悼会把本次行动完全替换为一次无差别普通攻击；每完成一次该行动后静默消耗一层。
    if(p[x].lament_time > 0)
    {
        basic_attack(x);
        p[x].lament_time--;
        add(p[x].no , p[x].no , 35 , 0 , p[x].hp , 0 , "status_sync" , "", true);
        return;
    }

    if(p[x].is_scientific_witch && p[x].tmp.empty())
    {
        if(p[x].has_rebirth_potion) brew_rebirth_potion(x);
        else idle_skill(x);
        return;
    }

    if(p[x].is_lancelot_zero && p[x].tmp.empty())
    {
        lancelot_zero_attack(x);
        return;
    }

    if(p[x].is_moon_child && p[x].tmp.empty())
    {
        moon_chant(x);
        return;
    }

    if(p[x].is_k2 && p[x].tmp.empty())
    {
        iron_blood_lotus(x);
        return;
    }

    if(p[x].is_galahad_one && p[x].tmp.empty())
    {
        if(p[x].galahad_laser_mode) galahad_laser_attack(x);
        else galahad_one_attack(x);
        return;
    }

    if(p[x].is_boss && p[x].tmp.empty())
    {
        if(p[x].is_mili)
        {
            int moon_weight = has_living_moon_child(x) ? 0 : 15;
            int k2_weight = has_living_k2(x) ? 0 : 10;
            int witch_weight = p[x].mili_witch_summoned ? 0 : 10; // 科学性实验魔女每局只允许成功召唤一次，死亡后也不再补召。
            int lament_weight = 10;
            int search_weight = 10;
            int milk_weight = p[x].hp * 100 <= get_maxhp(x) * 50 ? 1 : 0; // 高于 50% 生命时不参与双岛牛奶抽取。
            int sum = moon_weight + k2_weight + witch_weight + lament_weight + search_weight + milk_weight;
            int k = rnd_id(sum);
            if(k <= moon_weight) summon_moon_child(x);
            else if(k <= moon_weight + k2_weight) summon_k2(x);
            else if(k <= moon_weight + k2_weight + witch_weight) summon_scientific_witch(x);
            else if(k <= moon_weight + k2_weight + witch_weight + lament_weight) lament(x);
            else if(k <= moon_weight + k2_weight + witch_weight + lament_weight + search_weight) world_search(x);
            else double_island_milk(x);
            p[x].mili_skill_turn++;
            return;
        }
        else if(p[x].boss_magic_ready) magic_square(x);
        else if(p[x].boss_string_count >= 3) steal_skill(x);
        else string_skill(x);
        return;
    }

    if(p[x].tmp.empty())
    {
        void (*f)(int) = basic_attack; // 普攻是本次行动的默认分支。

        if(rnd_id(100) <= special_chance(x))
        {
            int sum = 0; // 可参加本次特殊技能共享抽取的权重总和。

            for(int i = 1;i <= p[x].sn;++ i)
            {
                if(pickable_special(x , i)) sum += special_weight(x , i);
            }

            if(sum > 0)
            {
                int k = rnd_id(sum); // 在 1～sum 内抽取共享权重位置。

                for(int i = 1;i <= p[x].sn;++ i)
                {
                    if(pickable_special(x , i) == false) continue;

                    k -= special_weight(x , i);

                    if(k <= 0)
                    {
                        f = p[x].skill[i];
                        break;
                    }
                }
            }
        }

        p[x].tmp.push_back(f);
        p[x].tmp_free.push_back(false);
    }

    while(p[x].tmp.empty() == false)
    {
        void (*f)(int) = p[x].tmp.front(); // 当前要执行的队首技能函数。
        p[x].tmp.pop_front();
        bool free_stolen = p[x].tmp_free.empty() ? false : p[x].tmp_free.front();
        if(p[x].tmp_free.empty() == false) p[x].tmp_free.pop_front();

        if(free_stolen && f == revive && has_revive_target(x) == false) f = string_skill;
        if(free_stolen && f == world_execute) f = world_search;
        if(free_stolen && stolen_skill_within_summon_cap(x , f) == false) continue;

        if(free_stolen == false && spell(f) && p[x].magic < cost(f))
        {
            basic_attack(x);
            return;
        }

        if(free_stolen == false && spell(f))
        {
            int c = cost(f); // 当前法术实际消耗的魔力。
            p[x].magic -= c;
            add(p[x].no , p[x].no , 0 , c , p[x].magic , 0 , "mana_cost" , "" , false);
        }

        // 每个被偷的队列项都单独写入确认行：重复技能不会被合并；快速行动虽不显示数值分配，也会明确表明其已实际触发。
        if(free_stolen)
        {
            add(p[x].no , p[x].no , 25 , 0 , p[x].hp , 0 , "boss_stolen_cast" , p[x].name + "连放" , false);
            add(p[x].no , p[x].no , 25 , 0 , p[x].hp , stolen_skill_tone(f) , "boss_stolen_cast" , skill_display_name(f) , true);
        }

        f(x);

        // 战斗循环通常保留本次行动值的正余量，使快速行动的 +20000 必然跨过严格大于 20000 的阈值。
        // 这里仅为张洋偷取路径补齐极端零余量，保证快速行动执行后至少再获得一次行动，且不改变正常角色的精确分配规则。
        if(free_stolen && f == fast_action && p[x].act <= 20000) p[x].act = 20001;
        return;
    }
}

// 返回第一个行动槽已超过阈值的存活玩家；若无人可行动则返回 0。
int ready()
{
    for(int i = 1;i <= n;++ i)
    {
        if(p[i].alive && p[i].is_inert == false && p[i].act > 20000) return i;
    }

    return 0;
}

// 计算距离下一次有人行动还需跨过的最少战斗时刻；行动条件是严格大于 20000。
int next_action_time()
{
    int ans = 200001; // 所有存活角色中最早跨过行动阈值所需的时刻数。

    for(int i = 1;i <= n;++ i)
    {
        if(p[i].alive == false || p[i].is_inert) continue;

        int spd = get_spd(i); // 当前实际速度，已包含暴走等速度强化。
        int need = (20001 - p[i].act + spd - 1) / spd; // 向上取整，保证累加后严格大于 20000。
        ans = min(ans , max(1LL , need));
    }

    return ans;
}

// 执行完整战斗：数学跳跃到下一次行动时刻，再按原座位顺序处理该时刻内全部可行动角色。
void fight()
{
    if(alive_team() < 2) return;

    tim = 0;

    while(tim < 200000)
    {
        int step = next_action_time(); // 跳过其间不会产生任何行动、随机调用或状态结算的空白时刻。

        if(tim + step > 200000)
        {
            tim = 200001; // 与原 for 循环自然结束后的时刻计数保持一致。
            break;
        }

        tim += step;

        for(int i = 1;i <= n;++ i)
        {
            if(p[i].alive && p[i].is_inert == false) p[i].act += get_spd(i) * step;
        }

        while(true)
        {
            int x = ready(); // 本次将要行动的玩家座位。

            if(x == 0) break;

            p[x].act -= 20000;
            begin_turn(x);
            bool parry_skip = skip_parry_turn(x);
            add(p[x].no , p[x].no , 0 , 0 , 0 , 0 , "status_sync" , "" , false);
            if(parry_skip == false)
            {
                int before = p[x].magic; // 回魔前的当前魔力，用于计算本次实际回魔量。
                p[x].magic = min(get_maxmagic(x) , p[x].magic + get_mreg(x));
                if(p[x].magic > before) add(p[x].no , p[x].no , 0 , p[x].magic - before , p[x].magic , 0 , "mana_gain" , "" , false);
                use_skill(x);
            }
            if(world_execute_finished)
            {
                int survivor = 0;
                for(int i = 1;i <= n;++ i) if(p[i].alive) { survivor = i; break; }
                if(survivor > 0)
                {
                    win = p[survivor].id;
                    add(0 , 0 , 0 , win , 0 , 1 , "battle_end" , "队伍" + to_string(win) + "获胜" , true);
                }
                return;
            }
            end_turn(x);
            cnt++;

            if(alive_team() == 1)
            {
                add(0 , 0 , 0 , win , 0 , 1 , "battle_end" , "队伍" + to_string(win) + "获胜" , true);
                return;
            }
        }
    }

    add(0 , 0 , 0 , 0 , 0 , 6 , "battle_limit" , "战斗达到结算上限" , true);
}

// 将玩家 x 的当前状态序列化为前端 CppPlayerSnapshot 对象。
string player_json(int x)
{
    string s = "{"; // 正在拼接的单个玩家 JSON 文本。
    s += "\"id\":" + to_string(p[x].no) + ",";
    s += "\"teamId\":" + to_string(p[x].id) + ",";
    s += "\"seatId\":" + to_string(p[x].no) + ",";
    s += "\"inputIndex\":" + to_string(p[x].no) + ",";
    s += "\"isFamiliar\":" + string(p[x].is_familiar ? "true" : "false") + ",";
    s += "\"ownerPlayerId\":" + to_string(p[x].owner) + ",";
    s += "\"magicVulnerability\":" + to_string(p[x].magic_vuln) + ",";
    s += "\"name\":\"" + esc(p[x].name) + "\",";
    s += "\"hp\":" + to_string(p[x].hp) + ",";
    s += "\"maxHp\":" + to_string(get_maxhp(x)) + ",";
    s += "\"mana\":" + to_string(p[x].magic) + ",";
    s += "\"maxMana\":" + to_string(get_maxmagic(x)) + ",";
    s += "\"manaRecovery\":" + to_string(get_mreg(x)) + ",";
    s += "\"physicalAttack\":" + to_string(get_atk(x)) + ",";
    s += "\"physicalDefense\":" + to_string(get_def(x)) + ",";
    s += "\"magicAttack\":" + to_string(get_satk(x)) + ",";
    s += "\"magicDefense\":" + to_string(get_sdef(x)) + ",";
    s += "\"wisdom\":" + to_string(get_iq(x)) + ",";
    s += "\"speed\":" + to_string(get_spd(x)) + ",";
    s += "\"freezeLayers\":" + to_string(p[x].freeze) + ",";
    s += "\"freezeStrength\":" + to_string(p[x].freeze_int) + ",";
    s += "\"ironwallStrength\":" + to_string(p[x].def_plus_int) + ",";
    s += "\"ironwallLayers\":" + to_string(p[x].def_plus_time) + ",";
    s += "\"defenseDownStrength\":" + to_string(p[x].def_down_int) + ",";
    s += "\"defenseDownLayers\":" + to_string(p[x].def_down_time) + ",";
    s += "\"damageDownStrength\":" + to_string(p[x].damage_down_int) + ",";
    s += "\"damageDownLayers\":" + to_string(p[x].damage_down_time) + ",";
    s += "\"squareStrength\":" + to_string(p[x].square_int) + ",";
    s += "\"squareLayers\":" + to_string(p[x].square_time) + ",";
    s += "\"speedUpStrength\":" + to_string(p[x].spd_up_int) + ",";
    s += "\"speedUpLayers\":" + to_string(p[x].spd_up_time) + ",";
    s += "\"speedDownStrength\":" + to_string(p[x].spd_down_int) + ",";
    s += "\"speedDownLayers\":" + to_string(p[x].spd_down_time) + ",";
    s += "\"burnStrength\":" + to_string(p[x].burn_int) + ",";
    s += "\"burnLayers\":" + to_string(p[x].burn_time) + ",";
    s += "\"poisonStrength\":" + to_string(p[x].posion_int) + ",";
    s += "\"poisonLayers\":" + to_string(p[x].poison_time) + ",";
    s += "\"parryLayers\":" + to_string(p[x].parry_time) + ",";
    s += "\"lamentLayers\":" + to_string(p[x].lament_time) + ",";
    s += "\"alive\":" + string(p[x].alive ? "true" : "false");
    s += "}";
    return s;
}

// 将 p[1] 到 p[n] 的所有玩家状态序列化为 JSON 数组。
string players_json()
{
    string s = "["; // 正在拼接的玩家数组 JSON 文本。

    for(int i = 1;i <= n;++ i)
    {
        if(i > 1) s += ",";

        s += player_json(i);
    }

    s += "]";
    return s;
}

// 将所有 Cmd 渲染指令序列化为前端 CppRenderCommand 数组。
string cmds_json()
{
    string s = "["; // 正在拼接的指令数组 JSON 文本。

    for(int i = 1;i < (int)e.size();++ i)
    {
        if(i > 1) s += ",";

        s += "{";
        s += "\"sourcePlayerId\":" + to_string(e[i].a) + ",";
        s += "\"targetPlayerId\":" + to_string(e[i].b) + ",";
        s += "\"skillId\":" + to_string(e[i].sid) + ",";
        s += "\"value\":" + to_string(e[i].val) + ",";
        s += "\"valueAfter\":" + to_string(e[i].after) + ",";
        s += "\"renderTone\":" + to_string(e[i].col) + ",";
        s += "\"frontEndAnimation\":\"" + esc(e[i].ani) + "\",";
        s += "\"text\":\"" + esc(e[i].str) + "\",";
        s += "\"freezeLayers\":" + to_string(e[i].freeze) + ",";
        s += "\"freezeStrength\":" + to_string(e[i].freeze_int) + ",";
        s += "\"ironwallStrength\":" + to_string(e[i].ironwall_int) + ",";
        s += "\"ironwallLayers\":" + to_string(e[i].ironwall) + ",";
        s += "\"defenseDownStrength\":" + to_string(e[i].def_down_int) + ",";
        s += "\"defenseDownLayers\":" + to_string(e[i].def_down) + ",";
        s += "\"damageDownStrength\":" + to_string(e[i].damage_down_int) + ",";
        s += "\"damageDownLayers\":" + to_string(e[i].damage_down) + ",";
        s += "\"squareStrength\":" + to_string(e[i].square_int) + ",";
        s += "\"squareLayers\":" + to_string(e[i].square) + ",";
        s += "\"speedUpStrength\":" + to_string(e[i].spd_up_int) + ",";
        s += "\"speedUpLayers\":" + to_string(e[i].spd_up_time) + ",";
        s += "\"speedDownStrength\":" + to_string(e[i].spd_down_int) + ",";
        s += "\"speedDownLayers\":" + to_string(e[i].spd_down_time) + ",";
        s += "\"burnStrength\":" + to_string(e[i].burn_int) + ",";
        s += "\"burnLayers\":" + to_string(e[i].burn_time) + ",";
        s += "\"poisonStrength\":" + to_string(e[i].posion_int) + ",";
        s += "\"poisonLayers\":" + to_string(e[i].poison_time) + ",";
        s += "\"parryLayers\":" + to_string(e[i].parry) + ",";
        s += "\"lamentLayers\":" + to_string(e[i].lament) + ",";
        s += "\"alive\":" + string(e[i].alive ? "true" : "false") + ",";
        if(e[i].has_player_snapshot)
        {
            s += "\"playerName\":\"" + esc(e[i].player_name) + "\",";
            s += "\"playerMaxHp\":" + to_string(e[i].player_maxhp) + ",";
            s += "\"playerPhysicalAttack\":" + to_string(e[i].player_atk) + ",";
            s += "\"playerPhysicalDefense\":" + to_string(e[i].player_def) + ",";
            s += "\"playerMagicAttack\":" + to_string(e[i].player_satk) + ",";
            s += "\"playerMagicDefense\":" + to_string(e[i].player_sdef) + ",";
            s += "\"playerSpeed\":" + to_string(e[i].player_spd) + ",";
        }
        s += "\"newlineAfter\":" + string(e[i].nl ? "true" : "false");
        s += "}";
    }

    s += "]";
    return s;
}

// 只解析名单并输出初始属性快照，不执行战斗；用于前后端传输调试。
string snapshot(const string& s)
{
    read_input(s , true); // 初始属性调试快照固定使用完整生命。
    int h = (int)(get_hash(s) & 0x1fffffffffffffULL); // 可安全传给 JavaScript number 的 53 位传输哈希。
    return "{\"rawText\":\"" + esc(s) + "\",\"utf8ByteLength\":" + to_string((int)s.size()) + ",\"transportHash\":" + to_string(h) + ",\"players\":" + players_json() + "}";
}

// 解析名单、保存初始快照、执行战斗、保存终局快照并输出完整模拟结果。
string simulate(const string& s , bool long_battle)
{
    read_input(s , long_battle);

    if(alive_team() < 2) return "{\"error\":\"请输入至少两个队伍，并用空行分隔。\"}";

    string a = players_json(); // 战斗开始前的玩家快照。
    fight();
    string b = players_json(); // 战斗结束后的玩家快照。
    int h = (int)(get_hash(s) & 0x1fffffffffffffULL); // 返回前端用于识别本次输入的 53 位传输哈希。
    string ans = "{"; // 最终 CppBattleSimulationResponse 的 JSON 文本。
    ans += "\"rawText\":\"" + esc(s) + "\",";
    ans += "\"longBattle\":" + string(long_battle ? "true" : "false") + ",";
    ans += "\"utf8ByteLength\":" + to_string((int)s.size()) + ",";
    ans += "\"transportHash\":" + to_string(h) + ",";
    ans += "\"winnerTeamId\":" + to_string(win) + ",";
    ans += "\"momentCount\":" + to_string(tim) + ",";
    ans += "\"executedActionCount\":" + to_string(cnt) + ",";
    ans += "\"initialPlayers\":" + a + ",";
    ans += "\"finalPlayers\":" + b + ",";
    ans += "\"commands\":" + cmds_json();
    ans += "}";
    return ans;
}

// WASM 导出：传入 UTF-8 名单，返回初始属性 JSON；static 字符串保证返回指针在调用后仍有效。
extern "C" const char* name_arena_player_snapshot(const char* s)
{
    static string ans; // WASM 返回给 JavaScript 的持久 JSON 缓冲。
    ans = snapshot(s == nullptr ? "" : string(s));
    return ans.c_str();
}

// WASM 导出：传入 UTF-8 名单，返回完整战斗事件流 JSON。
extern "C" const char* name_arena_simulate_battle(const char* s , int long_battle)
{
    static string ans; // WASM 返回给 JavaScript 的持久 JSON 缓冲。
    ans = simulate(s == nullptr ? "" : string(s) , long_battle != 0);
    return ans.c_str();
}
