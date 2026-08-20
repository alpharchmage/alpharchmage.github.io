#include "namerena/common.hpp"
#include "namerena/player_snapshot.hpp"

namespace namerena::snapshot
{
    namespace
    {
        // 为 JSON 文本中的名字转义控制字符，UTF-8 中文字节保持原样。
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

        // 把一个完整 Player 投影为前端当前需要展示的字段。
        // 注意：这里不计算任何属性，只读取 C++ 已生成的 Player 数值。
        void append_player_json(ostringstream& output , const battle::Player& player)
        {
            output << "{\"id\":" << player.id << ',';
            output << "\"teamId\":" << player.team_id << ',';
            output << "\"seatId\":" << player.seat_id << ',';
            output << "\"inputIndex\":" << player.input_index << ',';
            output << "\"name\":\"" << json_escape(player.display_name) << "\",";
            output << "\"hp\":" << player.hp << ',';
            output << "\"maxHp\":" << player.max_hp << ',';
            output << "\"mana\":" << player.mana << ',';
            output << "\"maxMana\":" << player.max_mana << ',';
            output << "\"physicalAttack\":" << player.current_attribute[battle::ATTRIBUTE_PHYSICAL_ATTACK] << ',';
            output << "\"physicalDefense\":" << player.current_attribute[battle::ATTRIBUTE_DEFENSE] << ',';
            output << "\"magicAttack\":" << player.current_attribute[battle::ATTRIBUTE_MAGIC_ATTACK] << ',';
            output << "\"magicDefense\":" << player.current_attribute[battle::ATTRIBUTE_MAGIC_DEFENSE] << ',';
            output << "\"wisdom\":" << player.current_attribute[battle::ATTRIBUTE_SPECIAL] << ',';
            output << "\"speed\":" << player.current_attribute[battle::ATTRIBUTE_SPEED] << '}';
        }
    }

    // 供 wasm_api 调用的总快照函数。
    // 每次调用都会独立完成“解析 → 身份摘要 → 属性派生 → Player 列表 → JSON”，
    // 因而前端无需保存任何数值规则，也无法影响结算数值。
    string player_snapshot_json(const string& utf8_input)
    {
        ostringstream output;
        const auto response = identity::create_identity_response(utf8_input);
        const auto players = attribute::create_players_from_input(utf8_input);

        output << "{\"rawText\":\"" << json_escape(response.raw_text) << "\",";
        output << "\"utf8ByteLength\":" << response.utf8_byte_length << ',';
        output << "\"transportHash\":" << response.transport_hash << ',';
        output << "\"players\":[";

        // players[0] 是 1-index 哨兵，永远不会被序列化。
        for(int index = 1 ; index < static_cast<int>(players.size()) ; ++index)
        {
            if(index != 1)
            {
                output << ',';
            }
            append_player_json(output , players[index]);
        }

        output << "]}";
        return output.str();
    }
}
