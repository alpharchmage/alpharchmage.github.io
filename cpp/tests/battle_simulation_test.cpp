#include "namerena/common.hpp"
#include "namerena/battle_simulation.hpp"

using namespace namerena;

// 回归测试只检查 C++ 返回内容，不由测试端重算任何战斗数值。
static void expect_contains(const string& value , const string& expected_part , const string& name)
{
    if(value.find(expected_part) == string::npos)
    {
        cerr << "测试失败：" << name << "，缺少：" << expected_part << '\n';
        exit(1);
    }
}

// 两支队伍的完整对局必须在一次 C++ 调用内结束，并积累完整的前端播放指令。
signed main()
{
    string response_json = simulation::complete_battle_json("甲\n\n乙");

    expect_contains(response_json , "\"winnerTeamId\":" , "胜方编号");
    expect_contains(response_json , "\"matchSeed\":" , "对局顺序种子");
    expect_contains(response_json , "\"initialPlayers\":[" , "初始单位");
    expect_contains(response_json , "\"finalPlayers\":[" , "最终单位");
    expect_contains(response_json , "\"commands\":[" , "播放指令列表");
    expect_contains(response_json , "攻击" , "蓝色攻击指令");
    expect_contains(response_json , "受到" , "受到伤害指令");
    expect_contains(response_json , "获胜" , "战斗结算指令");

    if(response_json.find("造成") != string::npos)
    {
        cerr << "测试失败：前端播放 JSON 不应包含造成伤害文本\n";
        return 1;
    }

    string ordered_response_a = simulation::complete_battle_json("甲\n\n乙\n丙");
    string ordered_response_b = simulation::complete_battle_json("甲\n\n乙\n丙");
    string reordered_response = simulation::complete_battle_json("甲\n\n丙\n乙");

    if(ordered_response_a != ordered_response_b)
    {
        cerr << "测试失败：相同输入顺序必须复现完整对局结果\n";
        return 1;
    }

    if(ordered_response_a == reordered_response)
    {
        cerr << "测试失败：交换输入顺序后对局随机过程不应保持完全相同\n";
        return 1;
    }

    cout << "battle_simulation_test passed\n";
    return 0;
}
