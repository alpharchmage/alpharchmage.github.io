#include "namerena/common.hpp"
#include "namerena/battle_engine.hpp"

namespace namerena::battle
{
    // 0 是 xorshift32 的锁死状态，统一改为固定非零种子。
    MatchRandom::MatchRandom(uint32_t seed)
    {
        state = seed;

        if(state == 0)
        {
            state = 0x6d2b79f5U;
        }
    }

    // xorshift32：所有操作均限定在 uint32_t 位域内，浏览器与本地结果一致。
    uint32_t MatchRandom::next_u32()
    {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return state;
    }

    // 返回 [0 , count - 1] 的确定性随机下标；无候选时返回 -1。
    int MatchRandom::choose_index(int count)
    {
        if(count <= 0)
        {
            return -1;
        }

        return static_cast<int>(next_u32() % static_cast<uint32_t>(count));
    }

    // 基础技能描述使用 display_name；没有显示名时退回输入名，保证渲染文本始终完整。
    static string get_player_name(const Player& player)
    {
        if(player.display_name.empty() == false)
        {
            return player.display_name;
        }

        if(player.input_name.empty() == false)
        {
            return player.input_name;
        }

        return "角色";
    }

    // 生成通用普通攻击技能。将来特殊角色可以在填槽后自行删除或替换这项技能。
    static Skill create_normal_attack_skill(const Player& player)
    {
        Skill normal_attack;

        normal_attack.id = 1;
        normal_attack.owner_player_id = player.id;
        normal_attack.skill_type = SKILL_NORMAL_ATTACK;
        normal_attack.target_mode = TARGET_SINGLE_ENEMY;
        normal_attack.requires_target = true;
        normal_attack.code = "normal_attack";
        normal_attack.display_name = "攻击";
        normal_attack.cast_text = "攻击";
        return normal_attack;
    }

    // 每个角色第一次进入战斗前都必须有技能可选。skills 与 current_skill_slots 均维持 1-index。
    void fill_current_skill_slots(Player& player)
    {
        if(player.skills.size() == 1)
        {
            player.skills.push_back(create_normal_attack_skill(player));
        }

        if(player.current_skill_slots.size() == 1)
        {
            for(int skill_index = 1 ; skill_index < (int)player.skills.size() ; skill_index++)
            {
                player.current_skill_slots.push_back(player.skills[skill_index]);
            }
        }
    }

    // 行动时始终从当前技能槽第一个可用技能开始选取。
    bool select_first_current_skill(Player& player)
    {
        fill_current_skill_slots(player);

        for(int skill_index = 1 ; skill_index < (int)player.current_skill_slots.size() ; skill_index++)
        {
            Skill& skill = player.current_skill_slots[skill_index];

            if(skill.enabled == false || skill.cooldown_current > 0)
            {
                continue;
            }

            if(skill.max_use_count > 0 && skill.use_count >= skill.max_use_count)
            {
                continue;
            }

            player.current_skill = skill;
            return true;
        }

        return false;
    }

    // 普通攻击仅负责选定后的技能描述和基础物理伤害；防御参与本次伤害的最低值截断。
    DamageResult execute_normal_attack(Player& source , Player& target , render::RenderCommandBuffer& render_buffer)
    {
        const int skill_id = source.current_skill.id;
        const string source_name = get_player_name(source);
        const string target_name = get_player_name(target);

        // 同一条描述拆成三个有顺序的渲染片段，前端据此把“攻击”显示为蓝色。
        // 描述暂不换行，因为 damage 会继续追加“受到 + 红色数字 + 点伤害”到本行。
        render::emit(render_buffer , source.id , target.id , skill_id , 0 , target.hp , RENDER_TONE_NORMAL , "normal_attack" , source_name + " " , false);
        render::emit(render_buffer , source.id , target.id , skill_id , 0 , target.hp , RENDER_TONE_SKILL , "normal_attack" , "攻击" , false);
        render::emit(render_buffer , source.id , target.id , skill_id , 0 , target.hp , RENDER_TONE_NORMAL , "normal_attack" , " " + target_name + "，" , false);

        int physical_attack = max(0LL , source.current_attribute[ATTRIBUTE_PHYSICAL_ATTACK]);
        int physical_defense = max(0LL , target.current_attribute[ATTRIBUTE_DEFENSE]);
        int damage_amount = max(1LL , physical_attack - physical_defense);

        source.current_skill.use_count++;
        source.action_count++;
        source.turn_count++;
        return damage(source , target , damage_amount , render_buffer , "normal_attack_damage");
    }

    // 当前速度为状态修正后的属性。死亡或不可行动角色不再积累行动变量。
    int add_speed_to_all_alive_players(vector<Player>& player_list)
    {
        int added_player_count = 0;

        for(int player_index = 1 ; player_index < (int)player_list.size() ; player_index++)
        {
            Player& player = player_list[player_index];

            if(player.alive == false || player.can_act == false)
            {
                continue;
            }

            player.action_gauge += max(0LL , player.current_attribute[ATTRIBUTE_SPEED]);
            added_player_count++;
        }

        return added_player_count;
    }

    // 输入顺序是同一瞬间多个角色都已就绪时的稳定 tie-break，保证同输入可复现。
    int find_first_ready_player_index(const vector<Player>& player_list)
    {
        for(int player_index = 1 ; player_index < (int)player_list.size() ; player_index++)
        {
            const Player& player = player_list[player_index];

            if(player.alive && player.can_act && player.action_gauge > ACTION_GAUGE_THRESHOLD)
            {
                return player_index;
            }
        }

        return 0;
    }

    // 先收集全部存活敌人，再使用顺序相关的对局随机数选择其中之一。
    // 候选容器保持 1-index，避免与项目其他业务下标约定冲突。
    int find_random_alive_enemy_index(const vector<Player>& player_list , int source_index , MatchRandom& match_random)
    {
        if(source_index <= 0 || source_index >= (int)player_list.size())
        {
            return 0;
        }

        const int source_team_id = player_list[source_index].team_id;

        vector<int> enemy_index_list = vector<int>(1);

        for(int player_index = 1 ; player_index < (int)player_list.size() ; player_index++)
        {
            const Player& player = player_list[player_index];

            if(player.alive && player.team_id != source_team_id)
            {
                enemy_index_list.push_back(player_index);
            }
        }

        int selected_offset = match_random.choose_index((int)enemy_index_list.size() - 1);

        if(selected_offset < 0)
        {
            return 0;
        }

        return enemy_index_list[selected_offset + 1];
    }

    // 就绪后先扣除一次行动阈值，再从当前技能槽第一项选择技能；这保证超出多个阈值时会连续行动。
    bool execute_ready_player_action(vector<Player>& player_list , int source_index , MatchRandom& match_random , render::RenderCommandBuffer& render_buffer)
    {
        if(source_index <= 0 || source_index >= (int)player_list.size())
        {
            return false;
        }

        Player& source = player_list[source_index];

        if(source.alive == false || source.can_act == false || source.action_gauge <= ACTION_GAUGE_THRESHOLD)
        {
            return false;
        }

        source.action_gauge -= ACTION_GAUGE_THRESHOLD;

        if(select_first_current_skill(source) == false)
        {
            return false;
        }

        int target_index = find_random_alive_enemy_index(player_list , source_index , match_random);

        if(target_index == 0)
        {
            return false;
        }

        Player& target = player_list[target_index];

        if(source.current_skill.skill_type == SKILL_NORMAL_ATTACK)
        {
            execute_normal_attack(source , target , render_buffer);
            return true;
        }

        return false;
    }

    // 一次瞬间的严格顺序：先统一加速度一次，再持续执行全部 action_gauge > 20000 的角色。
    ActionGaugeResult advance_one_moment(vector<Player>& player_list , MatchRandom& match_random , render::RenderCommandBuffer& render_buffer)
    {
        ActionGaugeResult result;

        result.added_player_count = add_speed_to_all_alive_players(player_list);

        while(true)
        {
            int source_index = find_first_ready_player_index(player_list);

            if(source_index == 0)
            {
                break;
            }

            if(execute_ready_player_action(player_list , source_index , match_random , render_buffer))
            {
                result.executed_action_count++;
            }
        }

        result.has_remaining_ready_player = find_first_ready_player_index(player_list) != 0;
        return result;
    }

    // 收集仍拥有至少一名存活成员的队伍编号。结果列表同样使用 1-index 以符合项目约定。
    static vector<int> get_alive_team_id_list(const vector<Player>& player_list)
    {
        vector<int> alive_team_id_list = vector<int>(1);
        set<int> alive_team_set;

        for(int player_index = 1 ; player_index < (int)player_list.size() ; player_index++)
        {
            const Player& player = player_list[player_index];

            if(player.alive)
            {
                alive_team_set.insert(player.team_id);
            }
        }

        for(int team_id : alive_team_set)
        {
            alive_team_id_list.push_back(team_id);
        }

        return alive_team_id_list;
    }

    // 完整模拟不会把行动条的“瞬间”交给前端。它在一次 WASM 调用中连续推进，
    // 直到胜负明确，因此浏览器只需播放已经完成的渲染指令列表。
    CompleteBattleResult simulate_complete_battle(vector<Player>& player_list , MatchRandom& match_random , render::RenderCommandBuffer& render_buffer)
    {
        constexpr int MAX_MOMENT_COUNT = 200000;
        CompleteBattleResult result;
        vector<int> alive_team_id_list = get_alive_team_id_list(player_list);

        if(alive_team_id_list.size() <= 2)
        {
            result.valid = false;
            return result;
        }

        for(int moment_index = 1 ; moment_index <= MAX_MOMENT_COUNT ; moment_index++)
        {
            ActionGaugeResult moment_result = advance_one_moment(player_list , match_random , render_buffer);

            result.moment_count = moment_index;
            result.executed_action_count += moment_result.executed_action_count;
            alive_team_id_list = get_alive_team_id_list(player_list);

            if(alive_team_id_list.size() == 2)
            {
                result.ended = true;
                result.winner_team_id = alive_team_id_list[1];

                string winner_text = "队伍" + to_string(result.winner_team_id) + "获胜";
                render::emit(render_buffer , 0 , 0 , 0 , result.winner_team_id , 0 , RENDER_TONE_SYSTEM , "battle_end" , winner_text);
                return result;
            }

            if(alive_team_id_list.size() == 1)
            {
                result.ended = true;
                render::emit(render_buffer , 0 , 0 , 0 , 0 , 0 , RENDER_TONE_SYSTEM , "battle_end" , "战斗结束，没有存活队伍");
                return result;
            }
        }

        result.valid = false;
        result.limit_reached = true;
        render::emit(render_buffer , 0 , 0 , 0 , 0 , 0 , RENDER_TONE_WARNING , "battle_limit" , "战斗达到结算上限");
        return result;
    }
}
