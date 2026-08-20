#pragma once

#include "namerena/transport.hpp"

namespace namerena::identity
{
    // SHA-256 固定输出 8 个 32 位词。数组额外保留 0 号哨兵，因此实际长度为 9。
    constexpr int SHA256_WORD_COUNT = 8;

    // 一个名字的稳定身份记录。
    // team_id、input_index 是本局输入元信息；只有 hash_word/hash_hex 代表名字本身的身份摘要。
    struct NameIdentity
    {
        int team_id = 0 , input_index = 0;
        string input_name;
        // hash_word[1..8] 是完整 256 位 SHA-256 摘要，hash_word[0] 不使用。
        array<int , SHA256_WORD_COUNT + 1> hash_word{};
        // 同一摘要的 64 位十六进制文本，便于日志、调试和前端查看。
        string hash_hex;
    };

    // 将完整输入的传输信息和每个名字的身份记录汇总为一份响应。
    struct IdentityResponse
    {
        string raw_text;
        uint64_t utf8_byte_length = 0;
        uint32_t transport_hash = 0;
        // name_list[1..n] 依输入顺序保存名字身份。
        vector<NameIdentity> name_list = vector<NameIdentity>(1);
    };

    // 对任意字节串计算 SHA-256，并以 1-index 数组返回 8 个摘要词。
    array<int , SHA256_WORD_COUNT + 1> sha256_words(const string& input);

    // 将 8 个摘要词格式化为标准 64 个十六进制字符。
    string sha256_hex(const array<int , SHA256_WORD_COUNT + 1>& hash_word);

    // 为一个名字创建身份。实现中会加固定领域前缀，避免与其他用途的 SHA-256 混用。
    NameIdentity create_name_identity(const string& input_name , int team_id , int input_index);

    // 从前端原文完成“解析队伍 → 按顺序建立每个名字身份”的完整流程。
    IdentityResponse create_identity_response(const string& utf8_input);

    // 将身份响应转为 JSON；此接口主要用于检查和后续调试面板。
    string to_json(const IdentityResponse& response);
}
