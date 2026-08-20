#pragma once

#include "namerena/common.hpp"

namespace namerena::transport
{
    // 前端原始文本进入 C++ 后的第一层结果。
    // 所有业务 vector 均采用 1-index：下标 0 是哨兵，永不写入真实名字或队伍。
    struct EchoResult
    {
        // 完整保留前端传入的 UTF-8 原文，用于回显和前后端一致性检查。
        string raw_text;
        // 原始 UTF-8 的字节数，不是 Unicode 字符数量。
        uint64_t utf8_byte_length = 0;
        // 传输校验值，仅证明“收到的原文”是否一致；不是名字身份哈希。
        uint32_t transport_hash = 0;
        // teams[team_id][member_id] 为该队第 member_id 名成员，均从 1 开始。
        vector<vector<string>> teams = vector<vector<string>>(1);
        // 所有名字在输入文本中的扁平顺序，供后续生成 input_index。
        vector<string> ordered_names = vector<string>(1);
    };

    // 对字节串执行 FNV-1a 32 位校验；只用于传输校验，不用于角色身份。
    uint32_t fnv1a32(const string& bytes);

    // 解析前端 UTF-8 文本：换行分隔名字，空行分隔队伍。
    EchoResult echo_and_parse(const string& utf8_input);

    // 将解析结果转为调试/前端可读取的 JSON 字符串。
    string to_json(const EchoResult& result);
}
