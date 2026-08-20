#include "namerena/common.hpp"

// Emscripten 编译时引入导出标记；普通本地 C++ 编译时把标记定义为空，
// 这样同一份源文件仍可用于静态检查或其他本地工具。
#ifdef __EMSCRIPTEN__
#include<emscripten/emscripten.h>
#else
#define EMSCRIPTEN_KEEPALIVE
#endif

#include "namerena/player_snapshot.hpp"
#include "namerena/battle_simulation.hpp"

// extern "C" 禁止 C++ 名字改编，使 JavaScript 可稳定用
// "name_arena_player_snapshot" 调用该函数。
extern "C"
{
    // 唯一的当前前端 → C++ 属性接口。
    // 参数 utf8_input 指向前端传入的以 \0 结尾的 UTF-8 文本；
    // 返回值指向 C++ 管理的 JSON 字符串。前端只能读取并渲染 JSON，不能修改 Player。
    EMSCRIPTEN_KEEPALIVE const char* name_arena_player_snapshot(const char* utf8_input)
    {
        // static 让 c_str() 返回的内存在函数结束后仍然有效，直到下一次调用覆盖它。
        // 因此前端必须在下一次调用前立即复制/解析返回的 JSON。
        static string response_json;

        // 防御空指针。任何异常输入都返回合法 JSON，而不是使 WASM 直接崩溃。
        if(utf8_input == nullptr)
        {
            response_json = "{\"error\":\"input pointer is null\"}";
            return response_json.c_str();
        }

        // string(utf8_input) 复制原始文本后，完整处理仍由 C++ 执行：
        // 解析队伍、SHA-256 身份、正态属性派生和 JSON 快照都不在前端计算。
        response_json = namerena::snapshot::player_snapshot_json(string(utf8_input));
        return response_json.c_str();
    }

    // 完整战斗接口：C++ 会在本次调用内高速结算到胜负已定，前端只读取最终 JSON 并播放 commands。
    EMSCRIPTEN_KEEPALIVE const char* name_arena_simulate_battle(const char* utf8_input)
    {
        static string response_json;

        if(utf8_input == nullptr)
        {
            response_json = "{\"error\":\"input pointer is null\"}";
            return response_json.c_str();
        }

        response_json = namerena::simulation::complete_battle_json(string(utf8_input));
        return response_json.c_str();
    }
}
