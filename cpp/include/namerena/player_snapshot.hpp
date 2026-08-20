#pragma once

#include "namerena/attribute_derivation.hpp"

namespace namerena::snapshot
{
    // 前端调用的只读快照入口。
    // C++ 在内部解析名字、生成身份和属性后，把前端需要展示的字段序列化为 JSON。
    // 前端只渲染这份字符串，禁止自行重新计算任何属性。
    string player_snapshot_json(const string& utf8_input);
}
