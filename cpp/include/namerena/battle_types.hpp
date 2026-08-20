#pragma once

#include "namerena/common.hpp"

namespace namerena::battle
{
    // 以下固定容量数组全部预留 0 号下标作为哨兵，真实业务编号从 1 开始。
    constexpr int ATTRIBUTE_COUNT = 8;
    constexpr int RESOURCE_COUNT = 6;
    constexpr int SKILL_PROPERTY_COUNT = 12;

    // Player.base_attribute/current_attribute 的下标定义。
    // ATTRIBUTE_SPECIAL 当前用于智慧；保留原名是为了以后扩展特殊属性时不破坏数组布局。
    enum AttributeType
    {
        ATTRIBUTE_NONE = 0,
        ATTRIBUTE_PHYSICAL_ATTACK = 1,
        ATTRIBUTE_DEFENSE = 2,
        ATTRIBUTE_SPEED = 3,
        ATTRIBUTE_AGILITY = 4,
        ATTRIBUTE_MAGIC_ATTACK = 5,
        ATTRIBUTE_MAGIC_DEFENSE = 6,
        ATTRIBUTE_SPECIAL = 7,
        ATTRIBUTE_MAX_HP = 8
    };

    // Player.resource/resource_max 的下标定义。
    enum ResourceType
    {
        RESOURCE_NONE = 0,
        RESOURCE_HP = 1,
        RESOURCE_MP = 2,
        RESOURCE_ENERGY = 3,
        RESOURCE_AMMO = 4,
        RESOURCE_SHIELD = 5,
        RESOURCE_SPECIAL_GAUGE = 6
    };

    // Skill 的用途分类；之后 C++ 战斗引擎据此选择结算时机。
    enum SkillType
    {
        SKILL_NONE = 0,
        SKILL_NORMAL_ATTACK = 1,
        SKILL_ACTIVE = 2,
        SKILL_PASSIVE = 3,
        SKILL_TRIGGERED = 4,
        SKILL_SUMMON = 5
    };

    // 一个技能可选择的目标范围。
    enum TargetMode
    {
        TARGET_NONE = 0,
        TARGET_SELF = 1,
        TARGET_SINGLE_ENEMY = 2,
        TARGET_RANDOM_ENEMY = 3,
        TARGET_ALL_ENEMIES = 4,
        TARGET_SINGLE_ALLY = 5,
        TARGET_ALL_ALLIES = 6,
        TARGET_SUMMON = 7
    };

    // StatusEffect 的效果类型。Buff/Debuff 暂未结算，但容器已预留。
    enum StatusType
    {
        STATUS_NONE = 0,
        STATUS_BUFF = 1,
        STATUS_DEBUFF = 2,
        STATUS_DOT = 3,
        STATUS_HOT = 4,
        STATUS_STUN = 5,
        STATUS_SHIELD = 6,
        STATUS_CHARGE = 7,
        STATUS_BLEED = 8
    };

    // 前端显示事件文本时可用的语义颜色，不承载任何战斗计算。
    enum RenderTone
    {
        RENDER_TONE_NORMAL = 0,
        RENDER_TONE_SYSTEM = 1,
        RENDER_TONE_SKILL = 2,
        RENDER_TONE_DAMAGE = 3,
        RENDER_TONE_HEAL = 4,
        RENDER_TONE_STATUS = 5,
        RENDER_TONE_WARNING = 6
    };

    // 一个可叠层、可持续、可移除的状态效果。
    struct StatusEffect
    {
        // 主键、拥有者、施加来源和来源技能。
        int id = 0 , owner_player_id = 0 , source_player_id = 0 , source_skill_id = 0;
        // 类型、层数、原始持续回合和剩余回合。
        int status_type = STATUS_NONE , layer_count = 0 , duration_turns = 0 , remaining_turns = 0;
        // 数值、叠层上限、触发时机和优先级。
        int value = 0 , stack_limit = 0 , trigger_time = 0 , priority = 0;
        // 显示与规则标记。
        bool removable = true , positive = false , expired = false , hidden = false;
        string code;
        string display_name;
        // 对八项属性的即时增减；下标遵循 AttributeType。
        array<int , ATTRIBUTE_COUNT + 1> attribute_delta{};
        // 供以后状态专属逻辑使用的固定数据槽；业务下标从 1 开始。
        array<int , SKILL_PROPERTY_COUNT + 1> property{};
    };

    // 一个技能的静态定义和战斗中可变计数。
    struct Skill
    {
        int id = 0 , owner_player_id = 0 , skill_type = SKILL_NONE , target_mode = TARGET_NONE;
        int cooldown_max = 0 , cooldown_current = 0 , max_use_count = 0 , use_count = 0;
        int charge_required = 0 , charge_current = 0 , cast_time_ticks = 0 , priority = 0;
        int resource_type = RESOURCE_NONE , resource_cost = 0 , damage_ratio = 0 , heal_ratio = 0;
        bool enabled = true , passive = false , requires_target = false , interruptible = false;
        string code;
        string display_name;
        string cast_text;
        // 技能附加数值槽，具体语义由未来的技能实现解释。
        array<int , SKILL_PROPERTY_COUNT + 1> property{};
        // 标签列表同样采用 1-index。
        vector<int> tag_list = vector<int>(1);
    };

    // 所有普通角色、召唤物和 Boss 都使用同一个 Player；不存在独立的 Boss 属性结构。
    struct Player
    {
        // 身份、队伍、队内座位、原始输入位置。
        int id = 0 , team_id = 0 , seat_id = 0 , input_index = 0;
        // 召唤归属、成长字段和名字摘要的第一词（仅作快速标识，完整摘要在身份层）。
        int summon_owner_id = 0 , level = 1 , exp = 0 , name_hash = 0;
        // 当前/最大生命与魔力，以及护盾、战斗计数。
        int hp = 0 , max_hp = 0 , mana = 0 , max_mana = 0;
        int shield_hp = 0 , action_gauge = 0 , turn_count = 0 , action_count = 0;
        int kill_count = 0 , death_count = 0 , next_skill_id = 0;
        // 角色身份与可行动状态。
        bool special_name = false , alive = true , can_act = true , is_summon = false;
        bool hidden = false , defeated = false , escaped = false , waiting_for_input = false;
        // 输入原名、对外显示名和未来特殊角色代码。
        string input_name;
        string display_name;
        string role_code;
        // 基础属性永不直接被 Buff 改写；当前属性会在战斗中叠加状态修正。
        array<int , ATTRIBUTE_COUNT + 1> base_attribute{};
        array<int , ATTRIBUTE_COUNT + 1> current_attribute{};
        // 当前资源和资源上限，均按 ResourceType 访问。
        array<int , RESOURCE_COUNT + 1> resource{};
        array<int , RESOURCE_COUNT + 1> resource_max{};
        // 拥有技能、当前技能槽、已选当前技能、状态与眷属。vector 均预留 0 号哨兵。
        vector<Skill> skills = vector<Skill>(1);
        vector<Skill> current_skill_slots = vector<Skill>(1);
        Skill current_skill;
        vector<StatusEffect> status_list = vector<StatusEffect>(1);
        vector<int> skill_order = vector<int>(1);
        // 使用智能指针避免 Player 内嵌 Player 导致无限递归构造。
        vector<shared_ptr<Player>> familiar_list = vector<shared_ptr<Player>>(1);
    };

    // 一支队伍的成员编号与战斗状态。
    struct Team
    {
        int id = 0 , input_order = 0 , alive_count = 0 , total_score = 0;
        bool defeated = false , winner = false;
        string display_name;
        // player_id_list[1..n] 保存队伍成员的 Player.id。
        vector<int> player_id_list = vector<int>(1);
    };

    // 一局战斗的全量 C++ 状态容器；后续 battle_engine 将直接修改其中的 Player。
    // 技能产生的前端渲染内容由独立 RenderCommandBuffer 保存，不属于本快照。
    struct BattleSnapshot
    {
        int battle_seed = 0 , current_turn = 0 , current_player_id = 0;
        int winner_team_id = 0 , battle_state = 0;
        bool started = false , ended = false , paused = false , valid = true;
        vector<Player> players = vector<Player>(1);
        vector<Team> teams = vector<Team>(1);
    };
}
