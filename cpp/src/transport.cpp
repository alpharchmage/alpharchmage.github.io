#include "namerena/common.hpp"
#include "namerena/transport.hpp"

namespace namerena::transport
{
    namespace
    {
        // 仅用于“解析”时统一换行：Windows 的 \r\n 和旧 Mac 的 \r 都视为 \n。
        // EchoResult.raw_text 仍保存原始文本，因此前端回显不会因本函数失真。
        string normalize_line_endings_for_parsing(const string& input)
        {
            string normalized;
            bool previous_was_carriage_return = false;
            normalized.reserve(input.size());

            for(char character : input)
            {
                if(character == '\r')
                {
                    normalized.push_back('\n');
                    previous_was_carriage_return = true;
                    continue;
                }
                // 已把 \r 转成 \n 时，跳过紧随的原 \n，避免 \r\n 被解析成两个空行。
                if(character == '\n' && previous_was_carriage_return)
                {
                    previous_was_carriage_return = false;
                    continue;
                }
                previous_was_carriage_return = false;
                normalized.push_back(character);
            }
            return normalized;
        }

        // 按换行拆分，并显式保留 lines[0] 作为哨兵。
        // 因此真实行号从 1 开始，最后一个没有换行的文本行也会被保留。
        vector<string> split_lines_1indexed(const string& normalized)
        {
            vector<string> lines(1);
            string current_line;

            for(char character : normalized)
            {
                if(character == '\n')
                {
                    lines.push_back(current_line);
                    current_line.clear();
                }
                else
                {
                    current_line.push_back(character);
                }
            }
            lines.push_back(current_line);
            return lines;
        }

        // 只有空格或 Tab 的行也等价于空行，用作队伍分隔符。
        bool is_separator_line(const string& line)
        {
            return line.find_first_not_of(" \t") == string::npos;
        }

        // 手写 JSON 字符串转义。UTF-8 的非控制字节保持原样，中文不会被转码或截断。
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
                else if(byte == '\b')
                {
                    output << "\\b";
                }
                else if(byte == '\f')
                {
                    output << "\\f";
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

        // 输出 1-index 字符串 vector 的真实元素。values[0] 永远不会写入 JSON。
        void append_json_string_array_1indexed(ostringstream& output , const vector<string>& values)
        {
            output << '[';
            for(int index = 1 ; index < static_cast<int>(values.size()) ; ++index)
            {
                if(index != 1)
                {
                    output << ',';
                }
                output << '"' << json_escape(values[index]) << '"';
            }
            output << ']';
        }
    }

    // FNV-1a 的 32 位版本。它足够快速，适合作为传输一致性标记；
    // 角色身份仍必须使用更强的 SHA-256，不能用此值替代。
    uint32_t fnv1a32(const string& bytes)
    {
        uint32_t hash = 0x811C9DC5u;

        for(unsigned char byte : bytes)
        {
            hash ^= static_cast<uint32_t>(byte);
            hash *= 0x01000193u;
        }
        return hash;
    }

    // 前端到 C++ 的第一道业务入口。
    // 规则：每一非空行是一名玩家；一个或多个空行结束当前队伍并开始下一支队伍。
    EchoResult echo_and_parse(const string& utf8_input)
    {
        EchoResult result;
        // 固定前缀把“传输校验”与其他 FNV 用途隔离开来。
        string hash_source = "namerena:v1:transport";
        vector<string> current_team(1);
        const vector<string> lines = split_lines_1indexed(normalize_line_endings_for_parsing(utf8_input));

        // 不修改原文；长度按 UTF-8 字节数统计，与 WASM 接收到的字节完全一致。
        result.raw_text = utf8_input;
        result.utf8_byte_length = static_cast<uint64_t>(utf8_input.size());
        hash_source.push_back('\0');
        hash_source += utf8_input;
        result.transport_hash = fnv1a32(hash_source);

        for(int line_index = 1 ; line_index < static_cast<int>(lines.size()) ; ++line_index)
        {
            if(is_separator_line(lines[line_index]))
            {
                // 连续空行不会产生空队伍；只有当前队伍有成员时才提交。
                if(current_team.size() > 1)
                {
                    result.teams.push_back(current_team);
                    current_team = vector<string>(1);
                }
            }
            else
            {
                current_team.push_back(lines[line_index]);
                result.ordered_names.push_back(lines[line_index]);
            }
        }
        // 输入末尾没有空行时，最后一支队伍在循环结束后提交。
        if(current_team.size() > 1)
        {
            result.teams.push_back(current_team);
        }
        return result;
    }

    // 将传输层结果暴露为 JSON，便于前端通信面板或调试工具直接检查。
    string to_json(const EchoResult& result)
    {
        ostringstream output;
        output << "{\"rawText\":\"" << json_escape(result.raw_text) << "\",";
        output << "\"utf8ByteLength\":" << result.utf8_byte_length << ',';
        output << "\"transportHash\":" << result.transport_hash << ',';
        output << "\"teams\":[";

        for(int team_index = 1 ; team_index < static_cast<int>(result.teams.size()) ; ++team_index)
        {
            if(team_index != 1)
            {
                output << ',';
            }
            append_json_string_array_1indexed(output , result.teams[team_index]);
        }

        output << "],\"orderedNames\":";
        append_json_string_array_1indexed(output , result.ordered_names);
        output << '}';
        return output.str();
    }
}
