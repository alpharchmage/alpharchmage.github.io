#pragma once

#include "namerena/battle_types.hpp"

namespace namerena::render
{
    // 一条由技能直接写给前端的渲染指令。
    // 它没有逻辑时间、父子关系、优先级或异步调度字段。
    // C++ 技能按实际结算顺序调用 emit，前端按 command_list 的顺序显示即可。
    struct RenderCommand
    {
        // 来源、目标和技能编号仅用于前端查找单位名称、头像或动画素材。
        int source_player_id = 0 , target_player_id = 0 , skill_id = 0;
        // 数值及其变化后数值。例如伤害时 value 是伤害，value_after 是目标当前生命。
        int value = 0 , value_after = 0;
        // 使用 battle::RenderTone 决定文本语义颜色；不影响任何战斗数值。
        int render_tone = battle::RENDER_TONE_NORMAL;
        // front_end_animation 是可选动画代号，例如 normal_attack、damage、heal。
        string front_end_animation;
        // text 是 C++ 已决定好的展示文本；前端无需根据数值自行拼接战斗逻辑。
        string text;
        // false 表示可与同一行上一条文本直接拼接；true 表示本指令结束后换行。
        bool newline_after = true;
    };

    // 一次技能或整局战斗的顺序化渲染输出容器。
    // command_list[0] 是 1-index 哨兵；后续技能可持续 append，不需要记录逻辑 Tick。
    struct RenderCommandBuffer
    {
        vector<RenderCommand> command_list = vector<RenderCommand>(1);
    };

    // 清空已经被前端读取的指令，重新保留 0 号哨兵。
    void clear(RenderCommandBuffer& buffer);

    // 技能内最常使用的输出函数。
    // 调用顺序就是前端渲染顺序；此函数只写显示信息，绝不替技能计算伤害或修改 Player。
    void emit(RenderCommandBuffer& buffer , int source_player_id , int target_player_id , int skill_id , int value , int value_after , int render_tone , const string& front_end_animation , const string& text , bool newline_after = true);
}
