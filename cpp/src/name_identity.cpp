#include "namerena/common.hpp"
#include "namerena/name_identity.hpp"

namespace namerena::identity
{
    namespace
    {
        // SHA-256 轮函数所需的 32 位循环右移。
        uint32_t rotate_right(uint32_t value , int shift)
        {
            return (value >> shift) | (value << (32 - shift));
        }

        // SHA-256 标准的 64 个轮常量。
        // 由于项目统一 1-index，SHA256_K[1..64] 有效，SHA256_K[0] 是哨兵。
        const array<int , 65> SHA256_K = {
            0,
            0x428a2f98u , 0x71374491u , 0xb5c0fbcfu , 0xe9b5dba5u , 0x3956c25bu , 0x59f111f1u , 0x923f82a4u , 0xab1c5ed5u,
            0xd807aa98u , 0x12835b01u , 0x243185beu , 0x550c7dc3u , 0x72be5d74u , 0x80deb1feu , 0x9bdc06a7u , 0xc19bf174u,
            0xe49b69c1u , 0xefbe4786u , 0x0fc19dc6u , 0x240ca1ccu , 0x2de92c6fu , 0x4a7484aau , 0x5cb0a9dcu , 0x76f988dau,
            0x983e5152u , 0xa831c66du , 0xb00327c8u , 0xbf597fc7u , 0xc6e00bf3u , 0xd5a79147u , 0x06ca6351u , 0x14292967u,
            0x27b70a85u , 0x2e1b2138u , 0x4d2c6dfcu , 0x53380d13u , 0x650a7354u , 0x766a0abbu , 0x81c2c92eu , 0x92722c85u,
            0xa2bfe8a1u , 0xa81a664bu , 0xc24b8b70u , 0xc76c51a3u , 0xd192e819u , 0xd6990624u , 0xf40e3585u , 0x106aa070u,
            0x19a4c116u , 0x1e376c08u , 0x2748774cu , 0x34b0bcb5u , 0x391c0cb3u , 0x4ed8aa4au , 0x5b9cca4fu , 0x682e6ff3u,
            0x748f82eeu , 0x78a5636fu , 0x84c87814u , 0x8cc70208u , 0x90befffau , 0xa4506cebu , 0xbef9a3f7u , 0xc67178f2u
        };

        // JSON 字符串转义；名字 UTF-8 内容保持原样，只有控制字符和引号会被替换。
        string json_escape(const string& value)
        {
            ostringstream output;

            for(unsigned char byte : value)
            {
                if(byte == '"')
                {
                    output << "\\\"";
                }
                else if(byte == '\\')
                {
                    output << "\\\\";
                }
                else if(byte == '\n')
                {
                    output << "\\n";
                }
                else if(byte == '\r')
                {
                    output << "\\r";
                }
                else if(byte == '\t')
                {
                    output << "\\t";
                }
                else if(byte < 0x20)
                {
                    output << "\\u00" << uppercase << hex << setw(2) << setfill('0')
                           << static_cast<int>(byte) << nouppercase << dec << setfill(' ');
                }
                else
                {
                    output << static_cast<char>(byte);
                }
            }
            return output.str();
        }
    }

    // 自实现 SHA-256，输入按原始 UTF-8 字节处理，不依赖系统加密库。
    // 返回数组 hash_word[1..8]，每个元素保存摘要的一个 32 位词。
    array<int , SHA256_WORD_COUNT + 1> sha256_words(const string& input)
    {
        // message[0] 是 1-index 哨兵，后续所有块偏移均可直接用 1 开始计算。
        vector<int> message = vector<int>(1);
        array<int , SHA256_WORD_COUNT + 1> hash_word{};
        uint64_t bit_length = static_cast<uint64_t>(input.size()) * 8;

        // 复制输入字节，随后按照 SHA-256 规则添加 0x80、零填充和 64 位原始长度。
        for(unsigned char byte : input)
        {
            message.push_back(static_cast<int>(byte));
        }
        message.push_back(0x80);
        while((static_cast<int>(message.size()) - 1) % 64 != 56)
        {
            message.push_back(0);
        }
        for(int shift = 56 ; shift >= 0 ; shift -= 8)
        {
            message.push_back(static_cast<int>((bit_length >> shift) & 0xffu));
        }

        // SHA-256 八个初始散列值。
        hash_word[1] = 0x6a09e667u;
        hash_word[2] = 0xbb67ae85u;
        hash_word[3] = 0x3c6ef372u;
        hash_word[4] = 0xa54ff53au;
        hash_word[5] = 0x510e527fu;
        hash_word[6] = 0x9b05688cu;
        hash_word[7] = 0x1f83d9abu;
        hash_word[8] = 0x5be0cd19u;

        // 每轮读取 64 字节，展开为 64 个 schedule word，再执行 64 次压缩更新。
        for(int block_start = 1 ; block_start < static_cast<int>(message.size()) ; block_start += 64)
        {
            array<int , 65> word{};
            uint32_t a = static_cast<uint32_t>(hash_word[1]);
            uint32_t b = static_cast<uint32_t>(hash_word[2]);
            uint32_t c = static_cast<uint32_t>(hash_word[3]);
            uint32_t d = static_cast<uint32_t>(hash_word[4]);
            uint32_t e = static_cast<uint32_t>(hash_word[5]);
            uint32_t f = static_cast<uint32_t>(hash_word[6]);
            uint32_t g = static_cast<uint32_t>(hash_word[7]);
            uint32_t h = static_cast<uint32_t>(hash_word[8]);

            // 前 16 个 word 直接按大端序读取。
            for(int index = 1 ; index <= 16 ; ++index)
            {
                int offset = block_start + (index - 1) * 4;
                word[index] = (message[offset] << 24) | (message[offset + 1] << 16) | (message[offset + 2] << 8) | message[offset + 3];
            }
            // 后 48 个 word 依 SHA-256 小 sigma 函数递推得到。
            for(int index = 17 ; index <= 64 ; ++index)
            {
                uint32_t s0 = rotate_right(static_cast<uint32_t>(word[index - 15]) , 7) ^ rotate_right(static_cast<uint32_t>(word[index - 15]) , 18) ^ (static_cast<uint32_t>(word[index - 15]) >> 3);
                uint32_t s1 = rotate_right(static_cast<uint32_t>(word[index - 2]) , 17) ^ rotate_right(static_cast<uint32_t>(word[index - 2]) , 19) ^ (static_cast<uint32_t>(word[index - 2]) >> 10);
                word[index] = static_cast<int>(static_cast<uint32_t>(word[index - 16]) + s0 + static_cast<uint32_t>(word[index - 7]) + s1);
            }
            // 64 轮压缩。uint32_t 转换确保这里遵循 SHA-256 的 32 位溢出语义。
            for(int index = 1 ; index <= 64 ; ++index)
            {
                uint32_t s1 = rotate_right(e , 6) ^ rotate_right(e , 11) ^ rotate_right(e , 25);
                uint32_t choose = (e & f) ^ ((~e) & g);
                uint32_t temp1 = h + s1 + choose + static_cast<uint32_t>(SHA256_K[index]) + static_cast<uint32_t>(word[index]);
                uint32_t s0 = rotate_right(a , 2) ^ rotate_right(a , 13) ^ rotate_right(a , 22);
                uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
                uint32_t temp2 = s0 + majority;

                h = g;
                g = f;
                f = e;
                e = d + temp1;
                d = c;
                c = b;
                b = a;
                a = temp1 + temp2;
            }

            // 将当前块结果加回累计摘要状态。
            hash_word[1] = static_cast<int>(static_cast<uint32_t>(hash_word[1]) + a);
            hash_word[2] = static_cast<int>(static_cast<uint32_t>(hash_word[2]) + b);
            hash_word[3] = static_cast<int>(static_cast<uint32_t>(hash_word[3]) + c);
            hash_word[4] = static_cast<int>(static_cast<uint32_t>(hash_word[4]) + d);
            hash_word[5] = static_cast<int>(static_cast<uint32_t>(hash_word[5]) + e);
            hash_word[6] = static_cast<int>(static_cast<uint32_t>(hash_word[6]) + f);
            hash_word[7] = static_cast<int>(static_cast<uint32_t>(hash_word[7]) + g);
            hash_word[8] = static_cast<int>(static_cast<uint32_t>(hash_word[8]) + h);
        }
        return hash_word;
    }

    // 以每词 8 位、共 64 位十六进制文本输出完整 256 位摘要。
    string sha256_hex(const array<int , SHA256_WORD_COUNT + 1>& hash_word)
    {
        ostringstream output;

        for(int index = 1 ; index <= SHA256_WORD_COUNT ; ++index)
        {
            output << hex << setw(8) << setfill('0') << static_cast<uint32_t>(hash_word[index]);
        }
        return output.str();
    }

    // 建立单个名字的稳定身份。
    // 领域分离前缀确保角色身份摘要不会与传输校验、未来对局种子等用途共享同一哈希空间。
    NameIdentity create_name_identity(const string& input_name , int team_id , int input_index)
    {
        NameIdentity identity;
        string domain_separated_name = "namerena:v1:name-identity";

        domain_separated_name.push_back('\0');
        domain_separated_name += input_name;
        identity.team_id = team_id;
        identity.input_index = input_index;
        identity.input_name = input_name;
        identity.hash_word = sha256_words(domain_separated_name);
        identity.hash_hex = sha256_hex(identity.hash_word);
        return identity;
    }

    // 把 transport 的二维分队结果展平为按输入顺序排列的名字身份列表。
    IdentityResponse create_identity_response(const string& utf8_input)
    {
        IdentityResponse response;
        const auto parsed = namerena::transport::echo_and_parse(utf8_input);
        int input_index = 0;

        response.raw_text = parsed.raw_text;
        response.utf8_byte_length = parsed.utf8_byte_length;
        response.transport_hash = parsed.transport_hash;

        // team_index/member_index 均从 1 开始，与前端空行分队的顺序完全一致。
        for(int team_index = 1 ; team_index < static_cast<int>(parsed.teams.size()) ; ++team_index)
        {
            for(int member_index = 1 ; member_index < static_cast<int>(parsed.teams[team_index].size()) ; ++member_index)
            {
                ++input_index;
                response.name_list.push_back(create_name_identity(parsed.teams[team_index][member_index] , team_index , input_index));
            }
        }
        return response;
    }

    // 为调试或身份展示生成 JSON；真正的属性页面会使用 player_snapshot_json。
    string to_json(const IdentityResponse& response)
    {
        ostringstream output;
        output << "{\"rawText\":\"" << json_escape(response.raw_text) << "\",";
        output << "\"utf8ByteLength\":" << response.utf8_byte_length << ',';
        output << "\"transportHash\":" << response.transport_hash << ',';
        output << "\"names\":[";

        for(int index = 1 ; index < static_cast<int>(response.name_list.size()) ; ++index)
        {
            if(index != 1)
            {
                output << ',';
            }
            const auto& identity = response.name_list[index];
            output << "{\"name\":\"" << json_escape(identity.input_name) << "\",";
            output << "\"teamId\":" << identity.team_id << ',';
            output << "\"inputIndex\":" << identity.input_index << ',';
            output << "\"identityHash\":\"" << identity.hash_hex << "\"}";
        }

        output << "]}";
        return output.str();
    }
}
