#pragma once

#include "namerena/render_command.hpp"

namespace namerena::battle
{
    // damage 的结算结果只供技能或后续规则读取；前端展示内容已经由 damage 直接输出。
    struct DamageResult
    {
        int requested_amount = 0 , shield_absorbed = 0 , hp_lost = 0 , hp_after = 0;
        bool target_defeated = false;
    };

    // heal 的结算结果只供技能或后续规则读取；actual_amount 已经扣除了溢出治疗。
    struct HealResult
    {
        int requested_amount = 0 , actual_amount = 0 , hp_after = 0;
    };

    // add_status 的结算结果标明状态是新增还是与已有状态合并。
    struct StatusApplyResult
    {
        int status_id = 0 , layer_count = 0 , remaining_turns = 0;
        bool added = false , merged = false;
    };

    // settle_status 汇总本次持续状态实际造成的生命变化，供未来技能或统计模块读取。
    struct StatusSettleResult
    {
        int damage_total = 0 , heal_total = 0 , removed_count = 0;
    };

    // 恢复 target 的生命。恢复值自动截断到最大生命，死亡单位不会被此基础函数复活。
    // 函数完成数值修改后，会直接写入一条治疗渲染指令。
    HealResult heal(Player& source , Player& target , int amount , render::RenderCommandBuffer& render_buffer , const string& front_end_animation = "heal");

    // 对 target 造成伤害。函数依次处理护盾、生命、死亡标记和对应渲染指令。
    // amount 是技能已计算好的最终伤害；伤害公式不放在此处，避免基础结算层承担技能规则。
    DamageResult damage(Player& source , Player& target , int amount , render::RenderCommandBuffer& render_buffer , const string& front_end_animation = "damage");

    // 施加、刷新或叠加一个状态效果。具体规则由 StatusEffect 的 id、source 和属性决定。
    // 函数会处理层数上限、持续回合与属性修正，并直接输出状态文本。
    StatusApplyResult add_status(Player& source , Player& target , const StatusEffect& incoming_status , render::RenderCommandBuffer& render_buffer , const string& front_end_animation = "status_add");

    // 在调用方指定的结算点处理一个角色全部持续状态。
    // DOT 与 HOT 会分别调用 damage/heal；状态到期后撤销属性修正并从列表移除。
    StatusSettleResult settle_status(Player& target , vector<Player>& player_list , render::RenderCommandBuffer& render_buffer);
}
