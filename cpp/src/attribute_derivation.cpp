#include "namerena/common.hpp"
#include "namerena/attribute_derivation.hpp"

namespace namerena::attribute
{
    namespace
    {
        // 32 位循环左移。只用于把 SHA-256 的不同摘要词混合为独立样本。
        uint32_t rotate_left(uint32_t value , int shift)
        {
            return (value << shift) | (value >> (32 - shift));
        }

        // 轻量 32 位混合器：相邻输入位变化会扩散到更多输出位。
        // 它不是新的身份哈希，只是把 SHA-256 摘要词重排为属性采样值。
        uint32_t mix32(uint32_t value)
        {
            value ^= value >> 16;
            value *= 0x7feb352du;
            value ^= value >> 15;
            value *= 0x846ca68bu;
            value ^= value >> 16;
            return value;
        }
    }

    // 将 SHA-256 摘要映射到一个确定性截断正态近似值。
    // 做法：用 salt 从摘要派生 12 个 [0,1000000] 的独立样本并求和。
    // 多个均匀样本之和会集中在中间，因而多数属性靠近区间中心、靠近上下界的值较少。
    // 全程不调用 rand()；同名在任何位置均得到相同属性。
    int derive_value(const array<int , identity::SHA256_WORD_COUNT + 1>& hash_word , int salt , int minimum , int maximum)
    {
        int sum = 0 , sample_count = 12;

        for(int sample_index = 1 ; sample_index <= sample_count ; ++sample_index)
        {
            // 先从 8 个 1-index SHA 词中选出两个位置，再按属性 salt 进行旋转和异或。
            uint32_t value = static_cast<uint32_t>(hash_word[(sample_index + salt - 1) % identity::SHA256_WORD_COUNT + 1]);
            int shift = (sample_index * 5 + salt * 7) % 31 + 1;

            value ^= rotate_left(static_cast<uint32_t>(hash_word[(sample_index * 3 + salt) % identity::SHA256_WORD_COUNT + 1]) , shift);
            value ^= static_cast<uint32_t>((sample_index + salt) * 0x9e3779b9u);
            sum += static_cast<int>(mix32(value) % 1000001u);
        }
        // sum 的理论范围是 [0, sample_count * 1000000]。
        // 整数线性映射保证最终结果永远不会越过用户指定的闭区间。
        return minimum + (maximum - minimum) * sum / (sample_count * 1000000);
    }

    // 当前版本八项基础属性的唯一规则入口。
    // 如需调整数值区间，只改本函数的 minimum/maximum；不要在前端重算。
    DerivedAttribute derive_attribute(const identity::NameIdentity& name_identity)
    {
        DerivedAttribute attribute;

        attribute.max_hp = derive_value(name_identity.hash_word , 1 , 200 , 400);
        attribute.physical_attack = derive_value(name_identity.hash_word , 2 , 70 , 130);
        attribute.physical_defense = derive_value(name_identity.hash_word , 3 , 30 , 60);
        attribute.magic_attack = derive_value(name_identity.hash_word , 4 , 0 , 80);
        attribute.magic_defense = derive_value(name_identity.hash_word , 5 , 0 , 100);
        attribute.wisdom = derive_value(name_identity.hash_word , 6 , 100 , 200);
        attribute.speed = derive_value(name_identity.hash_word , 7 , 800 , 1200);
        attribute.max_mana = derive_value(name_identity.hash_word , 8 , 0 , 100);
        return attribute;
    }

    // 将“名字身份 + 固定属性”写入统一 Player。
    // 所有可战斗实体都走此结构，Boss 以后只需在此之后覆盖专属字段或装填专属技能。
    battle::Player create_player(const identity::NameIdentity& name_identity , int player_id , int seat_id)
    {
        battle::Player player;
        const DerivedAttribute attribute = derive_attribute(name_identity);

        // 输入和队伍信息由 transport/identity 提供，不从名字摘要推导。
        player.id = player_id;
        player.team_id = name_identity.team_id;
        player.seat_id = seat_id;
        player.input_index = name_identity.input_index;
        // 只保存摘要第一词作快速标识；完整 256 位身份仍在 NameIdentity 中可重建。
        player.name_hash = name_identity.hash_word[1];
        // 新建角色以满生命、满魔力进入战斗。
        player.hp = attribute.max_hp;
        player.max_hp = attribute.max_hp;
        player.mana = attribute.max_mana;
        player.max_mana = attribute.max_mana;
        player.input_name = name_identity.input_name;
        player.display_name = name_identity.input_name;

        // base_attribute 记录未受状态影响的基础值；current_attribute 起始时复制基础值。
        player.base_attribute[battle::ATTRIBUTE_PHYSICAL_ATTACK] = attribute.physical_attack;
        player.base_attribute[battle::ATTRIBUTE_DEFENSE] = attribute.physical_defense;
        player.base_attribute[battle::ATTRIBUTE_MAGIC_ATTACK] = attribute.magic_attack;
        player.base_attribute[battle::ATTRIBUTE_MAGIC_DEFENSE] = attribute.magic_defense;
        player.base_attribute[battle::ATTRIBUTE_SPECIAL] = attribute.wisdom;
        player.base_attribute[battle::ATTRIBUTE_SPEED] = attribute.speed;
        player.base_attribute[battle::ATTRIBUTE_MAX_HP] = attribute.max_hp;
        player.current_attribute = player.base_attribute;

        // 每个普通角色先拥有一个普通攻击，并把它填入当前技能槽第一个位置。
        // 特殊角色以后只需在此基础上追加或替换技能槽，不需要改变行动条选技规则。
        battle::Skill normal_attack;

        normal_attack.id = 1;
        normal_attack.owner_player_id = player.id;
        normal_attack.skill_type = battle::SKILL_NORMAL_ATTACK;
        normal_attack.target_mode = battle::TARGET_SINGLE_ENEMY;
        normal_attack.requires_target = true;
        normal_attack.code = "normal_attack";
        normal_attack.display_name = "攻击";
        normal_attack.cast_text = "攻击";
        player.skills.push_back(normal_attack);
        player.current_skill_slots.push_back(normal_attack);
        player.current_skill = normal_attack;

        // resource 数组是战斗结算统一读取的资源镜像。
        player.resource[battle::RESOURCE_HP] = player.hp;
        player.resource_max[battle::RESOURCE_HP] = player.max_hp;
        player.resource[battle::RESOURCE_MP] = player.mana;
        player.resource_max[battle::RESOURCE_MP] = player.max_mana;
        return player;
    }

    // 从整个前端输入批量创建 Player。
    // players[0] 是哨兵；team_seat[team_id] 用于计算每名成员在本队中的 1-index 位置。
    vector<battle::Player> create_players_from_input(const string& utf8_input)
    {
        vector<battle::Player> players(1);
        array<int , 100100> team_seat{};
        const auto response = identity::create_identity_response(utf8_input);

        for(int index = 1 ; index < static_cast<int>(response.name_list.size()) ; ++index)
        {
            const auto& name_identity = response.name_list[index];
            ++team_seat[name_identity.team_id];
            players.push_back(create_player(name_identity , index , team_seat[name_identity.team_id]));
        }
        return players;
    }
}
