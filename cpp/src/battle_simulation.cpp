#include "namerena/common.hpp"
#include "namerena/attribute_derivation.hpp"
#include "namerena/battle_simulation.hpp"
#include "namerena/name_identity.hpp"
#include "namerena/transport.hpp"

namespace namerena::simulation
{
    namespace
    {
        // JSON 文本只转义控制字符与引号；UTF-8 中文字节直接保留，供浏览器 JSON.parse 读取。
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

        // 初始和最终 Player 都使用同一份字段布局，避免前端在不同阶段套用不同数值规则。
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
            output << "\"speed\":" << player.current_attribute[battle::ATTRIBUTE_SPEED] << ',';
            output << "\"alive\":" << (player.alive ? "true" : "false") << '}';
        }

        // command_list[0] 是 1-index 哨兵，不可发给前端。
        void append_command_json(ostringstream& output , const render::RenderCommand& command)
        {
            output << "{\"sourcePlayerId\":" << command.source_player_id << ',';
            output << "\"targetPlayerId\":" << command.target_player_id << ',';
            output << "\"skillId\":" << command.skill_id << ',';
            output << "\"value\":" << command.value << ',';
            output << "\"valueAfter\":" << command.value_after << ',';
            output << "\"renderTone\":" << command.render_tone << ',';
            output << "\"frontEndAnimation\":\"" << json_escape(command.front_end_animation) << "\",";
            output << "\"text\":\"" << json_escape(command.text) << "\",";
            output << "\"newlineAfter\":" << (command.newline_after ? "true" : "false") << '}';
        }

        void append_player_list_json(ostringstream& output , const vector<battle::Player>& player_list)
        {
            output << '[';

            for(int player_index = 1 ; player_index < (int)player_list.size() ; player_index++)
            {
                if(player_index != 1)
                {
                    output << ',';
                }

                append_player_json(output , player_list[player_index]);
            }

            output << ']';
        }

        // 队伍顺序、队内顺序和解析后的名字共同组成对局种子文本；属性派生不会读取它。
        uint32_t create_match_seed(const transport::EchoResult& parsed_input)
        {
            string ordered_team_text;

            for(int team_index = 1 ; team_index < (int)parsed_input.teams.size() ; team_index++)
            {
                if(team_index > 1)
                {
                    ordered_team_text += "\n\n";
                }

                for(int member_index = 1 ; member_index < (int)parsed_input.teams[team_index].size() ; member_index++)
                {
                    if(member_index > 1)
                    {
                        ordered_team_text += '\n';
                    }

                    ordered_team_text += parsed_input.teams[team_index][member_index];
                }
            }

            string seeded_text = "namerena:v1:match";
            seeded_text.push_back('\0');
            seeded_text += ordered_team_text;
            return transport::fnv1a32(seeded_text);
        }
    }

    // 一次函数调用内完整结算，不把“所有人加速”之类的内部过程返回给前端。
    string complete_battle_json(const string& utf8_input)
    {
        ostringstream output;
        const auto identity_response = identity::create_identity_response(utf8_input);
        const auto parsed_input = transport::echo_and_parse(utf8_input);
        const uint32_t match_seed = create_match_seed(parsed_input);
        vector<battle::Player> initial_player_list = attribute::create_players_from_input(utf8_input);
        vector<battle::Player> final_player_list = initial_player_list;
        render::RenderCommandBuffer render_buffer;
        battle::MatchRandom match_random(match_seed);

        if(final_player_list.size() <= 1)
        {
            return "{\"error\":\"请输入至少两个名字并用空行分隔队伍。\"}";
        }

        battle::CompleteBattleResult battle_result = battle::simulate_complete_battle(final_player_list , match_random , render_buffer);

        if(battle_result.valid == false)
        {
            if(battle_result.limit_reached)
            {
                return "{\"error\":\"战斗未能在结算上限内结束。\"}";
            }

            return "{\"error\":\"至少需要两个队伍才能开始战斗，请用空行分隔队伍。\"}";
        }

        output << "{\"rawText\":\"" << json_escape(identity_response.raw_text) << "\",";
        output << "\"utf8ByteLength\":" << identity_response.utf8_byte_length << ',';
        output << "\"transportHash\":" << identity_response.transport_hash << ',';
        output << "\"matchSeed\":" << match_seed << ',';
        output << "\"winnerTeamId\":" << battle_result.winner_team_id << ',';
        output << "\"momentCount\":" << battle_result.moment_count << ',';
        output << "\"executedActionCount\":" << battle_result.executed_action_count << ',';
        output << "\"initialPlayers\":";
        append_player_list_json(output , initial_player_list);
        output << ",\"finalPlayers\":";
        append_player_list_json(output , final_player_list);
        output << ",\"commands\":[";

        for(int command_index = 1 ; command_index < (int)render_buffer.command_list.size() ; command_index++)
        {
            if(command_index != 1)
            {
                output << ',';
            }

            append_command_json(output , render_buffer.command_list[command_index]);
        }

        output << "]}";
        return output.str();
    }
}
