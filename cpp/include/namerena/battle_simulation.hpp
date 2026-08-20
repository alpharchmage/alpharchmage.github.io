#pragma once

#include "namerena/battle_engine.hpp"

namespace namerena::simulation
{
    // 从前端原始 UTF-8 输入创建单位、在 C++ 内部完整结算战斗，并返回前端只读 JSON。
    // JSON 包含初始单位、最终单位、全部渲染指令与胜负；前端不需要也不得重算任何数值。
    string complete_battle_json(const string& utf8_input);
}
