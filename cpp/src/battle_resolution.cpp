#include "namerena/common.hpp"
#include "namerena/battle_resolution.hpp"

namespace namerena::battle
{
    // 显示名称优先使用 display_name；未设置时退回原始输入名，最后使用固定占位。
    // 这样基础结算函数不依赖前端，也不要求未来所有特殊角色立刻填写显示名称。
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

        return "目标";
    }

    // 将 Player.hp 同步到通用资源数组中的 HP 槽。
    // 当前 Player 同时保留直观的 hp/max_hp 字段和未来通用资源接口，基础函数必须同步两者。
    static void sync_hp_resource(Player& player)
    {
        player.resource[RESOURCE_HP] = player.hp;
        player.resource_max[RESOURCE_HP] = player.max_hp;
    }

    // 状态显示名称优先取 display_name，其次取可维护的 code；两者都没有时仍返回可显示文本。
    static string get_status_name(const StatusEffect& status)
    {
        if(status.display_name.empty() == false)
        {
            return status.display_name;
        }

        if(status.code.empty() == false)
        {
            return status.code;
        }

        return "状态";
    }

    // 为一个状态层数应用属性增减。增量可为负数，因此不在这里强行截断属性，
    // 避免 Debuff 到期时无法精确还原原始属性；具体行动逻辑可再按需要限制最低值。
    static void apply_status_attribute_delta(Player& target , const StatusEffect& status , int layer_delta)
    {
        for(int attribute_type = 1 ; attribute_type <= ATTRIBUTE_COUNT ; attribute_type++)
        {
            target.current_attribute[attribute_type] += status.attribute_delta[attribute_type] * layer_delta;
        }
    }

    // 两个状态是否属于同一种可合并效果。
    // 有 code 时以 code 为身份；没有 code 时退回 id。来源玩家和来源技能不同的状态独立保存，
    // 因而不同角色施加的同名效果不会意外互相覆盖。
    static bool can_merge_status(const StatusEffect& existing_status , const StatusEffect& incoming_status)
    {
        if(existing_status.expired)
        {
            return false;
        }

        bool same_identity = false;

        if(incoming_status.code.empty() == false)
        {
            same_identity = existing_status.code == incoming_status.code;
        }
        else
        {
            same_identity = existing_status.id == incoming_status.id;
        }

        return same_identity && existing_status.source_player_id == incoming_status.source_player_id && existing_status.source_skill_id == incoming_status.source_skill_id;
    }

    // 从 1-index 玩家列表中寻找状态来源。来源已死亡或不存在时，持续效果仍可结算；
    // 此时用承受者作为日志来源，保证基础结算层不会因缺少来源而中断。
    static Player& get_status_source(Player& target , vector<Player>& player_list , int source_player_id)
    {
        for(int player_index = 1 ; player_index < (int)player_list.size() ; player_index++)
        {
            if(player_list[player_index].id == source_player_id)
            {
                return player_list[player_index];
            }
        }

        return target;
    }

    // 基础治疗只负责生命变化、上限截断和输出；不解释技能倍率、暴击或复活等额外规则。
    HealResult heal(Player& source , Player& target , int amount , render::RenderCommandBuffer& render_buffer , const string& front_end_animation)
    {
        HealResult result;

        result.requested_amount = max(0LL , amount);
        result.hp_after = target.hp;

        // 基础治疗不复活死亡单位，也不为零治疗写入无意义日志。
        if(target.alive == false || result.requested_amount == 0)
        {
            return result;
        }

        int missing_hp = max(0LL , target.max_hp - target.hp);
        result.actual_amount = min(result.requested_amount , missing_hp);

        if(result.actual_amount == 0)
        {
            return result;
        }

        target.hp += result.actual_amount;
        sync_hp_resource(target);
        result.hp_after = target.hp;

        string text = get_player_name(target) + "回复体力" + to_string(result.actual_amount) + "点";
        render::emit(render_buffer , source.id , target.id , source.current_skill.id , result.actual_amount , target.hp , RENDER_TONE_HEAL , front_end_animation , text);
        return result;
    }

    // 基础伤害先扣护盾，再扣生命，最后完成死亡状态更新；每一步都直接写出前端命令。
    DamageResult damage(Player& source , Player& target , int amount , render::RenderCommandBuffer& render_buffer , const string& front_end_animation)
    {
        DamageResult result;

        result.requested_amount = max(0LL , amount);
        result.hp_after = target.hp;

        // 已死亡单位不会再次受伤；零伤害同样不会输出伪造的受伤日志。
        if(target.alive == false || result.requested_amount == 0)
        {
            return result;
        }

        result.shield_absorbed = min(result.requested_amount , max(0LL , target.shield_hp));
        target.shield_hp -= result.shield_absorbed;

        int remaining_damage = result.requested_amount - result.shield_absorbed;
        result.hp_lost = min(remaining_damage , max(0LL , target.hp));
        target.hp -= result.hp_lost;
        sync_hp_resource(target);
        result.hp_after = target.hp;

        if(result.shield_absorbed > 0)
        {
            string shield_text = get_player_name(target) + "的护盾抵挡了" + to_string(result.shield_absorbed) + "点伤害";
            render::emit(render_buffer , source.id , target.id , source.current_skill.id , result.shield_absorbed , target.shield_hp , RENDER_TONE_STATUS , "shield" , shield_text);
        }

        if(result.hp_lost > 0)
        {
            // 受伤文本被拆分为普通文字、红色数字和普通结尾。这样普通攻击可与它们合并为同一行，
            // 同时前端只会将伤害数字标示为红色；数值变更仍完全由本 C++ 函数执行。
            string receive_text = get_player_name(target) + "受到";
            render::emit(render_buffer , source.id , target.id , source.current_skill.id , 0 , target.hp , RENDER_TONE_NORMAL , front_end_animation , receive_text , false);
            render::emit(render_buffer , source.id , target.id , source.current_skill.id , result.hp_lost , target.hp , RENDER_TONE_DAMAGE , front_end_animation , to_string(result.hp_lost) , false);
            render::emit(render_buffer , source.id , target.id , source.current_skill.id , 0 , target.hp , RENDER_TONE_NORMAL , front_end_animation , "点伤害" , true);
        }

        if(target.hp == 0 && result.hp_lost > 0)
        {
            target.alive = false;
            target.can_act = false;
            target.defeated = true;
            target.death_count++;
            result.target_defeated = true;

            string death_text = get_player_name(target) + "消失了";
            render::emit(render_buffer , source.id , target.id , source.current_skill.id , 0 , 0 , RENDER_TONE_WARNING , "death" , death_text);
        }

        return result;
    }

    // 新增状态时复制调用者传入的完整定义；调用者可通过 StatusEffect 决定 Buff、Debuff、DOT 或 HOT。
    StatusApplyResult add_status(Player& source , Player& target , const StatusEffect& incoming_status , render::RenderCommandBuffer& render_buffer , const string& front_end_animation)
    {
        StatusApplyResult result;

        // 死亡目标不接收新的基础状态；这是当前版本的统一规则，复活类技能以后可自行改写。
        if(target.alive == false || incoming_status.expired)
        {
            return result;
        }

        StatusEffect normalized_status = incoming_status;

        if(normalized_status.owner_player_id == 0)
        {
            normalized_status.owner_player_id = target.id;
        }

        if(normalized_status.source_player_id == 0)
        {
            normalized_status.source_player_id = source.id;
        }

        if(normalized_status.source_skill_id == 0)
        {
            normalized_status.source_skill_id = source.current_skill.id;
        }

        int requested_layer = max(1LL , normalized_status.layer_count);

        // 先寻找同来源、同身份的存活状态；找到后只增加新增层数并刷新持续时间。
        for(int status_index = 1 ; status_index < (int)target.status_list.size() ; status_index++)
        {
            StatusEffect& existing_status = target.status_list[status_index];

            if(can_merge_status(existing_status , normalized_status) == false)
            {
                continue;
            }

            int stack_limit = existing_status.stack_limit;

            if(normalized_status.stack_limit > 0)
            {
                stack_limit = normalized_status.stack_limit;
            }

            int old_layer = max(1LL , existing_status.layer_count);
            int new_layer = old_layer + requested_layer;

            if(stack_limit > 0)
            {
                new_layer = min(new_layer , stack_limit);
            }

            int added_layer = new_layer - old_layer;
            existing_status.layer_count = new_layer;
            existing_status.stack_limit = stack_limit;

            if(normalized_status.duration_turns > 0)
            {
                existing_status.duration_turns = normalized_status.duration_turns;
                existing_status.remaining_turns = normalized_status.duration_turns;
            }

            if(normalized_status.value != 0)
            {
                existing_status.value = normalized_status.value;
            }

            apply_status_attribute_delta(target , existing_status , added_layer);
            result.status_id = existing_status.id;
            result.layer_count = existing_status.layer_count;
            result.remaining_turns = existing_status.remaining_turns;
            result.merged = true;

            string merge_text = get_player_name(target) + "的" + get_status_name(existing_status) + "叠加至" + to_string(existing_status.layer_count) + "层";
            render::emit(render_buffer , source.id , target.id , source.current_skill.id , added_layer , existing_status.layer_count , RENDER_TONE_STATUS , front_end_animation , merge_text);
            return result;
        }

        // 没有同类效果时，作为新的 1-index 状态条目插入并立即应用全部层数的属性修正。
        normalized_status.layer_count = requested_layer;

        if(normalized_status.stack_limit > 0)
        {
            normalized_status.layer_count = min(normalized_status.layer_count , normalized_status.stack_limit);
        }

        if(normalized_status.duration_turns > 0 && normalized_status.remaining_turns <= 0)
        {
            normalized_status.remaining_turns = normalized_status.duration_turns;
        }

        apply_status_attribute_delta(target , normalized_status , normalized_status.layer_count);
        target.status_list.push_back(normalized_status);
        result.status_id = normalized_status.id;
        result.layer_count = normalized_status.layer_count;
        result.remaining_turns = normalized_status.remaining_turns;
        result.added = true;

        string add_text = get_player_name(target) + "获得了" + get_status_name(normalized_status);

        if(normalized_status.layer_count > 1)
        {
            add_text += "（" + to_string(normalized_status.layer_count) + "层）";
        }

        render::emit(render_buffer , source.id , target.id , source.current_skill.id , normalized_status.layer_count , normalized_status.remaining_turns , RENDER_TONE_STATUS , front_end_animation , add_text);
        return result;
    }

    // 状态结算按照 Player.status_list 的 1-index 固定顺序进行。
    // 该顺序就是前端日志顺序；不记录 Tick，也不建立额外事件对象。
    StatusSettleResult settle_status(Player& target , vector<Player>& player_list , render::RenderCommandBuffer& render_buffer)
    {
        StatusSettleResult result;
        vector<StatusEffect> remaining_status_list = vector<StatusEffect>(1);

        for(int status_index = 1 ; status_index < (int)target.status_list.size() ; status_index++)
        {
            StatusEffect status = target.status_list[status_index];

            if(status.expired)
            {
                continue;
            }

            Player& source = get_status_source(target , player_list , status.source_player_id);
            int layer_count = max(1LL , status.layer_count);

            // DOT/HOT 的 value 视为单层数值；基础函数会按当前层数统一放大。
            if(target.alive && status.status_type == STATUS_DOT)
            {
                DamageResult damage_result = damage(source , target , max(0LL , status.value) * layer_count , render_buffer , "status_damage");
                result.damage_total += damage_result.hp_lost + damage_result.shield_absorbed;
            }

            if(target.alive && status.status_type == STATUS_HOT)
            {
                HealResult heal_result = heal(source , target , max(0LL , status.value) * layer_count , render_buffer , "status_heal");
                result.heal_total += heal_result.actual_amount;
            }

            // duration_turns 为 0 的状态视为永久状态；只有正持续回合数才会在结算后递减。
            if(status.remaining_turns > 0)
            {
                status.remaining_turns--;
            }

            if(status.duration_turns > 0 && status.remaining_turns == 0)
            {
                apply_status_attribute_delta(target , status , -layer_count);
                status.expired = true;
                result.removed_count++;

                string expire_text = get_player_name(target) + "的" + get_status_name(status) + "消失了";
                render::emit(render_buffer , source.id , target.id , status.source_skill_id , 0 , 0 , RENDER_TONE_STATUS , "status_remove" , expire_text);
                continue;
            }

            remaining_status_list.push_back(status);
        }

        target.status_list = remaining_status_list;
        return result;
    }
}
