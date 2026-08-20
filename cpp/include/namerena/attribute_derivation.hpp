#pragma once

#include "namerena/battle_types.hpp"
#include "namerena/name_identity.hpp"

namespace namerena::attribute
{
    // 当前阶段由名字身份摘要直接派生的基础数值。
    // 没有技能、状态或战斗中的临时修正；这些会写入统一 Player 后再由战斗内核处理。
    struct DerivedAttribute
    {
        int max_hp = 0 , physical_attack = 0 , physical_defense = 0 , magic_attack = 0;
        int magic_defense = 0 , wisdom = 0 , speed = 0 , max_mana = 0;
    };

    // 用完整 SHA-256 摘要和属性专属 salt，在 [minimum, maximum] 内生成确定性截断正态数值。
    // 同一个名字、同一个 salt、同一个区间永远返回相同值；不会使用全局随机数。
    int derive_value(const array<int , identity::SHA256_WORD_COUNT + 1>& hash_word , int salt , int minimum , int maximum);

    // 根据名字身份生成当前版本的八项基础数值。
    DerivedAttribute derive_attribute(const identity::NameIdentity& name_identity);

    // 将派生属性和输入元信息写入统一 Player；普通角色、召唤物和 Boss 均使用此结构。
    battle::Player create_player(const identity::NameIdentity& name_identity , int player_id , int seat_id);

    // 直接从前端 UTF-8 原文生成 1-index 的 Player 列表，并计算每个成员在本队的 seat_id。
    vector<battle::Player> create_players_from_input(const string& utf8_input);
}
