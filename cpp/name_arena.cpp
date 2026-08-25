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
    int blood_int; // 嗜血强度；吸血攻击会恢复伤害值的 5×嗜血强度%。
    int burn_int; // 烧伤强度；烧伤角色每次行动结束后直接失去该数值生命。
    int burn_time; // 烧伤剩余层数；每次烧伤角色行动结束后减 1，归零时解除。
    int posion_int; // 中毒强度；中毒角色每次毒素结算都会直接失去该数值生命。
    int poison_time; // 中毒层数；每次行动结束后按当前层数结算等次数的毒素伤害。
    int parry_time; // 招架剩余静默跳过回合数；存在时等待直接攻击触发反击，若连续两个自身回合未触发则归零。
    bool has_guard; // 是否拥有被动非法术守护；队友受到可转移伤害时可参与固定 30% 分摊判定。
    bool has_last_stand; // 是否拥有被动垂死挣扎；非生命之轮伤害后仍存活且生命低于 10% 时可触发一次。
    bool last_stand_used; // 本局是否已经发动过垂死挣扎；新对局创建角色时重置，避免重复触发。
    bool has_counter; // 是否拥有被动反击；受到带来源的伤害后有 25% 概率立即向来源发起一次普攻。
    bool has_devour; // 是否拥有被动吞噬；初始禁用法术，击杀敌人后继承其技能资格和权重并恢复最大生命的 50%。
    bool is_familiar; // 是否为召唤出的幻魔眷属；眷属不参与初始名单，且只拥有普通攻击。
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
};

// Cmd 是 C++ 发送给前端的最小渲染指令；一条战斗日志可由多条 Cmd 拼接而成。
struct Cmd
{
    int a; // 指令来源玩家的座位编号；纯文本系统事件为 0。
    int b; // 指令目标玩家的座位编号；纯文本系统事件为 0。
    int sid; // 技能编号：1 普攻、2 雷击术、3 冰冻术、4 状态/治愈、5 铁壁、6 地裂术、7 疾走术、8 戳刺、9 吸血攻击、10 火球术/烧伤、11 净化、12 投毒/中毒、13 会心一击、14 招架/反击、15 快速行动、16 瘟疫、17 复苏术、18 守护、19 生命之轮、20 垂死挣扎、21 反击、22 吞噬、23 召唤。
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
    int spd_up_int; // 当前目标的速度强化强度快照。
    int spd_up_time; // 当前目标的速度强化剩余层数快照。
    int spd_down_int; // 当前目标的速度削减强度快照。
    int spd_down_time; // 当前目标的速度削减剩余层数快照。
    int burn_int; // 当前目标的烧伤强度快照。
    int burn_time; // 当前目标的烧伤剩余层数快照。
    int posion_int; // 当前目标的中毒强度快照。
    int poison_time; // 当前目标的中毒剩余层数快照。
    int parry; // 当前目标的招架层数快照。
    bool alive; // 当前目标在该命令结算后的存活状态快照。
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
int take_hit(int x , int it , int d , int sid , const string& ani , const string& damage_ani , const string& damage_suffix = "伤害" , HurtType type = HURT_DIRECT , bool passive_counter_attack = false , bool end_line = true); // 所有直接攻击统一使用的受击前判定入口。
void passive_counter(int x , int it , bool end_line = true); // 被动反击：x 受伤后对来源 it 进行一次不可递归的普攻。
void report_death(int source , int it , int sid); // 统一输出死亡日志、处理绑定眷属死亡，并在存在攻击来源时触发吞噬。

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

// 组装一条 Cmd 并压入事件流；所有技能对前端的输出都经过此函数。
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
    x.spd_up_int = 0;
    x.spd_up_time = 0;
    x.spd_down_int = 0;
    x.spd_down_time = 0;
    x.burn_int = 0;
    x.burn_time = 0;
    x.posion_int = 0;
    x.poison_time = 0;
    x.parry = 0;
    x.alive = false;

    if(1 <= b && b <= n)
    {
        x.freeze = p[b].freeze;
        x.freeze_int = p[b].freeze_int;
        x.ironwall = p[b].def_plus_time;
        x.ironwall_int = p[b].def_plus_int;
        x.spd_up_int = p[b].spd_up_int;
        x.spd_up_time = p[b].spd_up_time;
        x.spd_down_int = p[b].spd_down_int;
        x.spd_down_time = p[b].spd_down_time;
        x.burn_int = p[b].burn_int;
        x.burn_time = p[b].burn_time;
        x.posion_int = p[b].posion_int;
        x.poison_time = p[b].poison_time;
        x.parry = p[b].parry_time;
        x.alive = p[b].alive;
    }

    e.push_back(x);
}

// 生成一条无来源、无目标的独立纯文本提示。
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
    return p[x].def * (100 + p[x].def_plus_int) / 100;
}

int get_maxmagic(int x)
{
    return p[x].maxmagic;
}

int get_mreg(int x)
{
    return p[x].mreg;
}

int get_spd(int x)
{
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

// 计算法术伤害的统一倍率；幻魔固定带有 20 级常驻魔法易损，所有魔法伤害翻倍。
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
    p[n].name = s;
    p[n].no = n;
    p[n].id = id;
    p[n].maxhp = get_val(h , 400 , 800);
    if(long_battle == false) p[n].maxhp /= 2; // 短对局只由 C++ 将原始确定性生命减半。
    p[n].hp = p[n].maxhp;
    p[n].atk = get_val(h , 70 , 130);
    p[n].def = get_val(h , 30 , 60);
    p[n].spd = get_val(h , 800 , 1200);
    p[n].satk = get_val(h , 100 , 200);
    p[n].sdef = get_val(h , 0 , 100);
    p[n].iq = get_val(h , 100 , 200);
    uint64_t mh = get_hash("mana-arena:" + s); // 独立于基础属性的回魔正态分布种子。
    p[n].mreg = get_normal(mh , 20 , 40);
    p[n].maxmagic = 200;
    p[n].magic = 0;
    p[n].freeze = 0;
    p[n].freeze_int = 0;
    p[n].def_plus_int = 0;
    p[n].def_plus_time = 0;
    p[n].blood_int = 10;
    p[n].burn_int = 0;
    p[n].burn_time = 0;
    p[n].posion_int = 0;
    p[n].poison_time = 0;
    p[n].parry_time = 0;
    p[n].has_guard = false;
    p[n].has_last_stand = get_val(dh , 0 , 99) < 15;
    p[n].last_stand_used = false;
    p[n].has_counter = get_val(ch , 0 , 99) < 40;
    p[n].has_devour = get_val(eh , 0 , 99) < 10;
    p[n].is_familiar = false;
    p[n].owner = 0;
    p[n].magic_vuln = 0;
    p[n].spd_up_int = 0;
    p[n].spd_up_time = 0;
    p[n].spd_down_int = 0;
    p[n].spd_down_time = 0;
    p[n].act = 0;
    p[n].alive = true;
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
    }

    n = 0;
    e = vector<Cmd>(1);
    win = 0;
    tim = 0;
    cnt = 0;
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

// 在角色实际承受伤害后结算被动：先按低生命条件处理垂死挣扎，再对带来源的非反击伤害进行 25% 反击判定。
void finish_hurt(int x , int it , HurtType type , bool end_line = true)
{
    if(type != HURT_LIFE_WHEEL && p[it].alive && p[it].has_last_stand && p[it].last_stand_used == false && p[it].hp * 100 < get_maxhp(it) * 10)
    {
        p[it].last_stand_used = true;
        p[it].freeze = 0;
        p[it].freeze_int = 0;
        p[it].posion_int = 0;
        p[it].poison_time = 0;
        p[it].tmp.clear();
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

// 所有生命减少统一经过此函数；type 显式标识伤害来源，返回 0 表示无实际伤害、1 表示存活、2 表示死亡。
int receive_damage(int it , int d , HurtType type)
{
    if(d <= 0 || p[it].alive == false) return 0;

    p[it].hp = max(0LL , p[it].hp - d);
    if(p[it].hp > 0) return 1;
    p[it].alive = false;
    return 2;
}

// 反击：招架触发后由招架者对原攻击者造成 350% 物攻倍率物理伤害；文字前缀已由触发者写入，同样进入统一受击入口。
void counter_attack(int x , int it)
{
    if(p[x].alive == false || p[it].alive == false) return;

    int d = max(1LL , get_atk(x) * 350 / 100 - get_def(it));
    int f = 800 + rnd() % 401;
    d = max(1LL , (d * f + 500) / 1000);
    take_hit(x , it , d , 14 , "parry_counter_damage" , "parry_counter_damage" , "点伤害");
}

// 被动反击：受伤者对实际伤害来源发起一次普攻；反击伤害使用 HURT_COUNTER，防止两个反击被动互相递归。
void passive_counter(int x , int it , bool end_line)
{
    if(p[x].alive == false || p[it].alive == false) return;

    int d = max(1LL , get_atk(x) - get_def(it));
    int f = 800 + rnd() % 401;
    d = max(1LL , (d * f + 500) / 1000);
    take_hit(x , it , d , 21 , "counter" , "counter_damage" , "点伤害" , HURT_COUNTER , true , end_line);
}

// 吞噬：拥有被动的攻击者击杀敌人后，继承其全部技能资格及权重；已拥有的同名技能直接累加权重，并恢复最大生命的 30%。
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

    int heal = min(get_maxhp(x) * 30 / 100 , get_maxhp(x) - p[x].hp);
    p[x].hp += heal;
    add(p[x].no , p[it].no , 22 , 0 , p[x].hp , 0 , "devour" , p[x].name , false);
    add(p[x].no , p[it].no , 22 , 0 , p[x].hp , 16 , "devour" , "吞噬" , false);
    add(p[x].no , p[it].no , 22 , 0 , p[x].hp , 0 , "devour" , p[it].name + " " , false);
    add(p[x].no , p[it].no , 22 , 0 , p[x].hp , 0 , "devour" , p[x].name + " " , false);
    add(p[x].no , p[x].no , 22 , heal , p[x].hp , 4 , "devour_heal" , "恢复30%生命" , true);
}

// 返回本体 x 当前仍存活的幻魔眷属；同一名本体最多同时存在一个幻魔。
bool has_familiar(int x)
{
    for(int i = 1;i <= n;++ i) if(p[i].alive && p[i].is_familiar && p[i].owner == x) return true;
    return false;
}

// 本体死亡时撤销其存活幻魔；幻魔死亡不会反向影响本体。
void dismiss_familiars(int x)
{
    for(int i = 1;i <= n;++ i)
    {
        if(p[i].alive == false || p[i].is_familiar == false || p[i].owner != x) continue;
        p[i].alive = false;
        p[i].hp = 0;
        p[i].tmp.clear();
        add(x , i , 23 , 0 , 0 , 1 , "familiar_depart" , p[i].name + "随本体消失了" , true);
    }
}

// 所有伤害来源在死亡后统一调用此函数，确保本体死亡会带走幻魔，且带来源击杀继续正常触发吞噬。
void report_death(int source , int it , int sid)
{
    add(source , it , sid , 0 , 0 , 1 , "death" , p[it].name + "消失了" , true);
    dismiss_familiars(it);
    if(source > 0) devour(source , it);
}

// 召唤：本体没有存活幻魔时创建一名只会普攻的绑定眷属；魔力消耗由 use_skill 统一扣除。
void summon(int x)
{
    if(has_familiar(x)) return;

    n++;
    p[n] = Player();
    p[n].skill.fill(nullptr);
    p[n].can.fill(false);
    p[n].w.fill(0);
    p[n].tmp.clear();
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
    p[n].is_familiar = true;
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

// 判断 guardian 是否可为 target 的本次伤害承担守护：必须同队存活、有守护、未被冻结/招架，且承担 40% 后仍存活。
bool can_guard_target(int guardian , int target , int d)
{
    if(guardian == target || p[guardian].alive == false || p[guardian].has_guard == false) return false;
    if(p[guardian].id != p[target].id || p[guardian].freeze > 0 || p[guardian].parry_time > 0) return false;

    int part = d * 40 / 100;
    return part > 0 && p[guardian].hp > part;
}

// 在 target 的同队守护者中依次进行固定 30% 判定；首个成功者承担本次唯一的守护，守护伤害不会再触发守护。
int get_guardian(int target , int d)
{
    for(int guardian = 1;guardian <= n;++ guardian)
    {
        if(can_guard_target(guardian , target , d) == false) continue;
        if(rnd_id(100) <= 30) return guardian;
    }

    return 0;
}

// 应用不再触发招架的单次守护分摊伤害；守护成功后目标与守护者各承受原伤害的 40%，两份分摊不会继续触发守护。
void take_guard_damage(int x , int it , int d , int sid , const string& ani , const string& damage_ani , const string& damage_suffix , HurtType type , bool passive_counter_attack , bool end_line)
{
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
        int res = receive_damage(target , val , type);
        add(p[x].no , p[target].no , sid , val , p[target].hp , 3 , damage_ani , to_string(val) , false);
        add(p[x].no , p[target].no , sid , val , p[target].hp , 0 , damage_ani , damage_suffix , this_end_line);

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
        return;
    }

    apply_damage(it , d , true , end_line);
}

// 统一处理所有直接攻击伤害：返回 0 表示被招架或无实际伤害、1 表示命中存活、2 表示命中后死亡；持续伤害则直接调用 receive_damage。
int take_hit(int x , int it , int d , int sid , const string& ani , const string& damage_ani , const string& damage_suffix , HurtType type , bool passive_counter_attack , bool end_line)
{
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

    take_guard_damage(x , it , d , sid , ani , damage_ani , damage_suffix , type , passive_counter_attack , end_line);
    return p[it].alive ? 1 : 2;
}

// 普通攻击：选择一个敌人，按物攻与物防计算最低为 1 的物理伤害。
void basic_attack(int x)
{
    int it = get_target(x); // 被攻击目标的座位编号。

    if(it == 0) return;

    int d = max(1LL , get_atk(x) - get_def(it)); // 未波动的物理基础伤害。
    int f = 800 + rnd() % 401; // 80%～120% 的整数伤害波动系数。
    d = max(1LL , (d * f + 500) / 1000);

    add(p[x].no , p[it].no , 1 , 0 , p[it].hp , 0 , "normal_attack" , p[x].name + " " , false);
    add(p[x].no , p[it].no , 1 , 0 , p[it].hp , 0 , "normal_attack" , "发起攻击" , false);
    add(p[x].no , p[it].no , 1 , 0 , p[it].hp , 0 , "normal_attack" , "，" , false);
    take_hit(x , it , d , 1 , "normal_attack" , "normal_attack_damage");
    clear_parry_action();
}

// 戳刺：非法术特殊技能，对一个敌人造成 150% 物攻倍率的物理伤害，并静默施加 2 层 5 强度速度削减。
void stab(int x)
{
    int it = get_target(x); // 戳刺目标的座位编号。

    if(it == 0) return;

    int d = max(1LL , get_atk(x) * 150 / 100 - get_def(it)); // 以 150% 物攻减物防得到未波动基础伤害。
    int f = 800 + rnd() % 401; // 非法术物理伤害沿用普攻的 80%～120% 波动。
    d = max(1LL , (d * f + 500) / 1000);

    add(p[x].no , p[it].no , 8 , 0 , p[it].hp , 0 , "stab" , p[x].name , false);
    add(p[x].no , p[it].no , 8 , 0 , p[it].hp , 3 , "stab" , "戳刺" , false);
    add(p[x].no , p[it].no , 8 , 0 , p[it].hp , 0 , "stab" , "，" , false);
    int res = take_hit(x , it , d , 8 , "stab" , "stab_damage");
    clear_parry_action();
    if(res != 1) return;

    p[it].spd_down_int += 5;
    p[it].spd_down_time += 2;
}

// 会心一击：非法术特殊技能，对一个敌人造成 200% 物攻倍率的物理伤害。
void critical_strike(int x)
{
    int it = get_target(x); // 会心一击目标的座位编号。

    if(it == 0) return;

    int d = max(1LL , get_atk(x) * 200 / 100 - get_def(it)); // 以 200% 物攻减物防得到未波动基础伤害。
    int f = 800 + rnd() % 401; // 会心一击沿用非法术物理伤害的 80%～120% 波动。
    d = max(1LL , (d * f + 500) / 1000);

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

// 守护是被动非法术；资格由 make_player 写入 has_guard，主动技能抽取会将其排除，因此此函数不会在自身回合执行。
void guard(int x)
{
}

// 投毒：非法术特殊技能，有 50% 概率使一个敌人获得施法者 20% 物攻强度的一层中毒。
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

// 吸血攻击：非法术特殊技能，按 100% 物攻倍率造成物理伤害，并恢复该次实际伤害的 100%。
void lifesteal_attack(int x)
{
    int it = get_target(x); // 吸血攻击目标的座位编号。

    if(it == 0) return;

    int d = max(1LL , get_atk(x) - get_def(it)); // 100% 物攻倍率减物防得到未波动基础伤害。
    int f = 800 + rnd() % 401; // 非法术物理伤害沿用普攻的 80%～120% 波动。
    d = max(1LL , (d * f + 500) / 1000);

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

// 火球术：消耗由 cost 统一结算，对一个敌人造成 150% 魔攻倍率法术伤害，并施加 2 层烧伤。
void fireball(int x)
{
    int it = get_target(x); // 火球术目标的座位编号。

    if(it == 0) return;

    add(p[x].no , it , 10 , 0 , p[it].hp , 0 , "fireball" , p[x].name + "使用" , false);
    add(p[x].no , it , 10 , 0 , p[it].hp , 13 , "fireball" , "火球术" , false);

    int d = magic_damage(x , it , 150); // 火球术基础伤害为魔攻的 150%，幻魔受常驻易损影响。

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

// 瘟疫：消耗由 cost 统一结算，按目标当前生命的 70%～100% 直接伤害；魔攻越高，下限越高，越偏向高倍率；该伤害不进入招架判定。
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
    int res = receive_damage(it , d , HURT_PLAGUE);
    add(p[x].no , it , 16 , d , p[it].hp , 3 , "plague_damage" , to_string(d) , false);
    add(p[x].no , it , 16 , d , p[it].hp , 0 , "plague_damage" , "点伤害" , true);
    if(res == 2)
    {
        report_death(x , it , 16);
    }
    else if(res == 1) finish_hurt(x , it , HURT_PLAGUE);
    clear_parry_action();
}

// 生命之轮：消耗由 cost 统一结算，与当前生命最高的敌方直接交换生命值；不调用 take_hit，因此不会触发招架或守护。
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

// 雷击术：对一个敌人造成 3～5 段伤害，普通段 30% 魔攻倍率，终段 80% 倍率。
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

        int res = take_hit(x , it , d , 2 , "thunder" , i == cnt ? "thunder_damage_last" : "thunder_damage");
        if(p[x].alive == false || res == 2)
        {
            clear_parry_action();
            return;
        }
    }

    clear_parry_action();
}

// 地裂术：从存活敌人中无重复随机选取 4～6 名；敌人不足四名时攻击全部存活敌人。
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
    p[it].spd_down_int = 0;
    p[it].spd_down_time = 0;
    p[it].burn_int = 0;
    p[it].burn_time = 0;
    p[it].posion_int = 0;
    p[it].poison_time = 0;
    p[it].tmp.clear();
    p[it].act += 20000;
    add(p[x].no , it , 17 , hp , p[it].hp , 4 , "revive_heal" , p[it].name + "复活" , true);
}

// 净化：消耗由 cost 统一结算，移除友方全部负面状态，并恢复其 10% 魔攻生命。
void purify(int x)
{
    int it = get_purify_target(x);

    if(it == 0) return;

    p[it].freeze = 0;
    p[it].freeze_int = 0;
    p[it].spd_down_int = 0;
    p[it].spd_down_time = 0;
    p[it].burn_int = 0;
    p[it].burn_time = 0;
    p[it].posion_int = 0;
    p[it].poison_time = 0;
    p[it].tmp.clear();

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
        add(p[x].no , p[x].no , 4 , 0 , p[x].freeze , 0 , "unfreeze_fail" , p[x].name + " 解冻 失败" , true);
        return;
    }

    p[x].freeze--;

    if(p[x].freeze > 0)
    {
        p[x].tmp.push_front(unfreeze);
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

    int res = take_hit(x , it , d , 3 , "ice" , "ice_damage");
    clear_parry_action();
    if(res != 1) return;

    p[it].freeze_int += 1;
    p[it].freeze += 3;
    p[it].tmp.push_front(unfreeze);
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
    if(p[x].skill[i] == summon && has_familiar(x)) return false;
    if(p[x].skill[i] == heal_magic && has_heal_target(x) == false) return false;
    if(p[x].skill[i] == revive && has_revive_target(x) == false) return false;
    if(p[x].skill[i] == purify && has_purify_target(x) == false) return false;
    return true;
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

// 每个玩家自身回合结束时结算烧伤和中毒；烧伤固定减层，中毒按层数造成多次伤害后以 5% 概率全量解毒。
void end_turn(int x)
{
    if(p[x].alive == false) return;

    if(p[x].burn_time > 0)
    {
        int d = p[x].burn_int; // 本次烧伤通过统一伤害函数扣除的固定生命值。
        int res = receive_damage(x , d , HURT_BURN);
        p[x].burn_time--;
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

    if(p[x].poison_time == 0) return;

    int d = p[x].posion_int; // 每层中毒各自结算一次的固定毒素伤害。
    int cnt = p[x].poison_time; // 本回合开始前记录层数，避免结算中意外改变循环次数。

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
        if(p[x].poison_time == 0) return;
    }

    if(rnd() % 100 < 5)
    {
        p[x].posion_int = 0;
        p[x].poison_time = 0;
        add(p[x].no , p[x].no , 12 , 0 , p[x].poison_time , 14 , "poison_remove" , "解毒成功" , true);
        return;
    }

    add(p[x].no , p[x].no , 12 , 0 , p[x].poison_time , 14 , "poison_fail" , "解毒失败" , true);
}

// 让玩家 x 执行一次行动：优先执行 tmp 队列中的束缚技能，否则先按智慧判定特殊分支，再统一加权抽取法术或非法术。
void use_skill(int x)
{
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
    }

    while(p[x].tmp.empty() == false)
    {
        void (*f)(int) = p[x].tmp.front(); // 当前要执行的队首技能函数。
        p[x].tmp.pop_front();

        if(spell(f) && p[x].magic < cost(f))
        {
            basic_attack(x);
            return;
        }

        if(spell(f))
        {
            int c = cost(f); // 当前法术实际消耗的魔力。
            p[x].magic -= c;
            add(p[x].no , p[x].no , 0 , c , p[x].magic , 0 , "mana_cost" , "" , false);
        }

        f(x);
        return;
    }
}

// 返回第一个行动槽已超过阈值的存活玩家；若无人可行动则返回 0。
int ready()
{
    for(int i = 1;i <= n;++ i)
    {
        if(p[i].alive && p[i].act > 20000) return i;
    }

    return 0;
}

// 计算距离下一次有人行动还需跨过的最少战斗时刻；行动条件是严格大于 20000。
int next_action_time()
{
    int ans = 200001; // 所有存活角色中最早跨过行动阈值所需的时刻数。

    for(int i = 1;i <= n;++ i)
    {
        if(p[i].alive == false) continue;

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
            if(p[i].alive) p[i].act += get_spd(i) * step;
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
    s += "\"speedUpStrength\":" + to_string(p[x].spd_up_int) + ",";
    s += "\"speedUpLayers\":" + to_string(p[x].spd_up_time) + ",";
    s += "\"speedDownStrength\":" + to_string(p[x].spd_down_int) + ",";
    s += "\"speedDownLayers\":" + to_string(p[x].spd_down_time) + ",";
    s += "\"burnStrength\":" + to_string(p[x].burn_int) + ",";
    s += "\"burnLayers\":" + to_string(p[x].burn_time) + ",";
    s += "\"poisonStrength\":" + to_string(p[x].posion_int) + ",";
    s += "\"poisonLayers\":" + to_string(p[x].poison_time) + ",";
    s += "\"parryLayers\":" + to_string(p[x].parry_time) + ",";
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
        s += "\"speedUpStrength\":" + to_string(e[i].spd_up_int) + ",";
        s += "\"speedUpLayers\":" + to_string(e[i].spd_up_time) + ",";
        s += "\"speedDownStrength\":" + to_string(e[i].spd_down_int) + ",";
        s += "\"speedDownLayers\":" + to_string(e[i].spd_down_time) + ",";
        s += "\"burnStrength\":" + to_string(e[i].burn_int) + ",";
        s += "\"burnLayers\":" + to_string(e[i].burn_time) + ",";
        s += "\"poisonStrength\":" + to_string(e[i].posion_int) + ",";
        s += "\"poisonLayers\":" + to_string(e[i].poison_time) + ",";
        s += "\"parryLayers\":" + to_string(e[i].parry) + ",";
        s += "\"alive\":" + string(e[i].alive ? "true" : "false") + ",";
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
