#pragma once

#include "namerena/battle_resolution.hpp"

namespace namerena::battle
{
    // 行动变量严格大于此阈值才可以行动；等于阈值时必须等待下一次统一加速。
    constexpr int ACTION_GAUGE_THRESHOLD = 20000;

    // 一次“所有人加速度后，清空当前全部就绪行动”的处理结果。
    struct ActionGaugeResult
    {
        int added_player_count = 0 , executed_action_count = 0;
        bool has_remaining_ready_player = false;
    };

    // 对局随机数只服务于整局战斗过程，绝不参与名字属性派生。
    // 采用无符号 32 位 xorshift，确保本地 C++ 与 WebAssembly 的位运算结果一致。
    struct MatchRandom
    {
        uint32_t state = 1;

        explicit MatchRandom(uint32_t seed = 1);
        uint32_t next_u32();
        int choose_index(int count);
    };

    // 一局完整高速结算的结果。moment_count 只供调试和回归测试读取，前端不会播放这些内部瞬间。
    struct CompleteBattleResult
    {
        int winner_team_id = 0 , moment_count = 0 , executed_action_count = 0;
        bool ended = false , valid = true , limit_reached = false;
    };

    // 确保角色拥有普通攻击，并将拥有技能填入当前技能槽。
    // 当前版本只创建第一个普通攻击；后续角色技能会追加到 current_skill_slots 后面。
    void fill_current_skill_slots(Player& player);

    // 从 current_skill_slots 的 1 号位置开始选择第一个可用技能，复制到 current_skill。
    // 返回 false 说明当前角色没有可用技能。
    bool select_first_current_skill(Player& player);

    // 执行普通攻击。先写出“执行者 攻击 承受者”三段渲染指令，其中攻击词使用蓝色语义，
    // 再由 damage 输出造成伤害与受到伤害等细分渲染指令。
    DamageResult execute_normal_attack(Player& source , Player& target , render::RenderCommandBuffer& render_buffer);

    // 为所有存活角色统一增加当前速度值。这个函数不执行技能。
    int add_speed_to_all_alive_players(vector<Player>& player_list);

    // 返回输入顺序中第一个行动变量大于 20000 的角色下标；无就绪角色时返回 0。
    int find_first_ready_player_index(const vector<Player>& player_list);

    // 从所有存活敌人中由对局随机数选择一个下标；无目标时返回 0。
    int find_random_alive_enemy_index(const vector<Player>& player_list , int source_index , MatchRandom& match_random);

    // 消耗一名已经就绪角色的 20000 行动变量，并按当前技能槽第一个技能执行操作。
    // 返回 true 表示实际执行了技能；即使没有目标，行动变量也已按规则消耗。
    bool execute_ready_player_action(vector<Player>& player_list , int source_index , MatchRandom& match_random , render::RenderCommandBuffer& render_buffer);

    // 完整处理一个瞬间：全员加速度一次，然后只要任何角色变量大于 20000 就逐个执行。
    // 直到当前所有变量都不再大于 20000，函数才返回；不会在本函数内开始下一轮加速度。
    ActionGaugeResult advance_one_moment(vector<Player>& player_list , MatchRandom& match_random , render::RenderCommandBuffer& render_buffer);

    // 在 C++ 内部高速推进完整对局，直到只剩一个存活队伍、所有队伍消失或达到保护上限。
    // 每一个内部瞬间都完全结算，但前端只会接收最终积累好的 RenderCommandBuffer。
    CompleteBattleResult simulate_complete_battle(vector<Player>& player_list , MatchRandom& match_random , render::RenderCommandBuffer& render_buffer);
}
