#include "namerena/common.hpp"
#include "namerena/attribute_derivation.hpp"
#include "namerena/battle_engine.hpp"

using namespace namerena;

// 测试失败时输出名称和数值，保持与现有基础结算测试一致的无框架风格。
static void expect_equal(int actual_value , int expected_value , const string& name)
{
    if(actual_value != expected_value)
    {
        cerr << "测试失败：" << name << "，实际值=" << actual_value << "，期望值=" << expected_value << '\n';
        exit(1);
    }
}

static void expect_true(bool condition , const string& name)
{
    if(condition == false)
    {
        cerr << "测试失败：" << name << '\n';
        exit(1);
    }
}

// 构造仅用于战斗循环的最小角色；真实网页角色仍由属性派生模块创建。
static battle::Player create_test_player(int id , int team_id , const string& name , int speed , int physical_attack , int physical_defense)
{
    battle::Player player;

    player.id = id;
    player.team_id = team_id;
    player.display_name = name;
    player.input_name = name;
    player.max_hp = 1000;
    player.hp = 1000;
    player.current_attribute[battle::ATTRIBUTE_SPEED] = speed;
    player.current_attribute[battle::ATTRIBUTE_PHYSICAL_ATTACK] = physical_attack;
    player.current_attribute[battle::ATTRIBUTE_DEFENSE] = physical_defense;
    player.resource[battle::RESOURCE_HP] = player.hp;
    player.resource_max[battle::RESOURCE_HP] = player.max_hp;
    return player;
}

// 此测试验证本轮只加速一次，并将所有 action_gauge > 20000 的行动全部执行后才允许下一轮加速。
signed main()
{
    // 名字派生出的速度必须符合新设定的闭区间 800–1200。
    vector<battle::Player> derived_players = attribute::create_players_from_input("甲\n\n乙\n丙");

    for(int player_index = 1 ; player_index < (int)derived_players.size() ; player_index++)
    {
        int speed = derived_players[player_index].current_attribute[battle::ATTRIBUTE_SPEED];
        expect_true(speed >= 800 && speed <= 1200 , "速度派生范围");
    }

    // 第一组：甲加速后正好等于 20000，不得行动；乙变为 20001，必须行动一次。
    vector<battle::Player> player_list = vector<battle::Player>(1);
    player_list.push_back(create_test_player(1 , 1 , "甲" , 1200 , 100 , 30));
    player_list.push_back(create_test_player(2 , 2 , "乙" , 1000 , 100 , 20));
    player_list[1].action_gauge = 18800;
    player_list[2].action_gauge = 19001;

    render::RenderCommandBuffer render_buffer;
    battle::MatchRandom first_match_random(123456U);
    battle::ActionGaugeResult first_result = battle::advance_one_moment(player_list , first_match_random , render_buffer);

    expect_equal(first_result.added_player_count , 2 , "统一加速角色数");
    expect_equal(first_result.executed_action_count , 1 , "严格大于阈值的行动数");
    expect_equal(player_list[1].action_gauge , 20000 , "等于阈值时不行动");
    expect_equal(player_list[2].action_gauge , 1 , "行动后减去阈值");
    expect_equal(player_list[1].hp , 930 , "乙普通攻击伤害");
    expect_equal((int)render_buffer.command_list.size() , 7 , "普通攻击六条渲染指令与哨兵");
    expect_equal(render_buffer.command_list[2].render_tone , battle::RENDER_TONE_SKILL , "攻击文字为蓝色技能语义");
    expect_true(render_buffer.command_list[2].text == "攻击" , "攻击描述文本");
    expect_true(render_buffer.command_list[3].newline_after == false , "攻击描述暂不换行");
    expect_true(render_buffer.command_list[4].text == "甲受到" , "受到伤害文字指令");
    expect_equal(render_buffer.command_list[5].render_tone , battle::RENDER_TONE_DAMAGE , "只有伤害数字为红色伤害语义");
    expect_true(render_buffer.command_list[5].text == "70" , "红色伤害数字指令");
    expect_true(render_buffer.command_list[6].text == "点伤害" && render_buffer.command_list[6].newline_after , "受伤描述行末换行");

    // 第二组：甲加速后积累到 42000，必须连续行动两次，不能在两次行动之间重新给所有人加速度。
    vector<battle::Player> multi_action_list = vector<battle::Player>(1);
    multi_action_list.push_back(create_test_player(1 , 1 , "甲" , 800 , 100 , 0));
    multi_action_list.push_back(create_test_player(2 , 2 , "乙" , 800 , 100 , 0));
    multi_action_list[1].action_gauge = 41200;

    render::RenderCommandBuffer multi_action_buffer;
    battle::MatchRandom second_match_random(123456U);
    battle::ActionGaugeResult second_result = battle::advance_one_moment(multi_action_list , second_match_random , multi_action_buffer);

    expect_equal(second_result.executed_action_count , 2 , "单次加速后的连续行动数");
    expect_equal(multi_action_list[1].action_gauge , 2000 , "连续行动后剩余行动变量");
    expect_equal(multi_action_list[1].action_count , 2 , "同轮普通攻击次数");
    expect_equal(multi_action_list[2].hp , 800 , "连续普通攻击累计伤害");
    expect_true(second_result.has_remaining_ready_player == false , "本轮就绪角色已清空");

    // 第三组：目标必须从全部存活敌人中由随机数选取；相同种子应稳定复现相同目标。
    vector<battle::Player> random_target_list_a = vector<battle::Player>(1);
    random_target_list_a.push_back(create_test_player(1 , 1 , "甲" , 800 , 100 , 0));
    random_target_list_a.push_back(create_test_player(2 , 2 , "乙" , 800 , 100 , 0));
    random_target_list_a.push_back(create_test_player(3 , 2 , "丙" , 800 , 100 , 0));
    random_target_list_a[1].action_gauge = 19201;

    vector<battle::Player> random_target_list_b = random_target_list_a;
    render::RenderCommandBuffer random_target_buffer_a , random_target_buffer_b;
    battle::MatchRandom random_a(987654321U) , random_b(987654321U);
    battle::advance_one_moment(random_target_list_a , random_a , random_target_buffer_a);
    battle::advance_one_moment(random_target_list_b , random_b , random_target_buffer_b);

    expect_true(random_target_buffer_a.command_list[1].target_player_id == 2 || random_target_buffer_a.command_list[1].target_player_id == 3 , "随机目标来自全部存活敌人");
    expect_equal(random_target_buffer_a.command_list[1].target_player_id , random_target_buffer_b.command_list[1].target_player_id , "相同对局种子复现目标");

    cout << "battle_engine_test passed\n";
    return 0;
}
