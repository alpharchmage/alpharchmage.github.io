#include "namerena/common.hpp"
#include "namerena/render_command.hpp"

namespace namerena::render
{
    // 清空容器时重新构造为长度 1 的 vector，以持续遵守业务数据 1-index 约定。
    void clear(RenderCommandBuffer& buffer)
    {
        buffer.command_list = vector<RenderCommand>(1);
    }

    // 技能直接调用本函数即可向前端追加一条渲染命令。
    // 数值结算应在调用前由技能完成，例如先 target.hp -= damage，
    // 再把 damage 与 target.hp 作为 value/value_after 传入。
    void emit(RenderCommandBuffer& buffer , int source_player_id , int target_player_id , int skill_id , int value , int value_after , int render_tone , const string& front_end_animation , const string& text , bool newline_after)
    {
        RenderCommand command;

        command.source_player_id = source_player_id;
        command.target_player_id = target_player_id;
        command.skill_id = skill_id;
        command.value = value;
        command.value_after = value_after;
        command.render_tone = render_tone;
        command.front_end_animation = front_end_animation;
        command.text = text;
        command.newline_after = newline_after;
        buffer.command_list.push_back(command);
    }
}
