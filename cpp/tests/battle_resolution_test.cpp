#include "namerena/common.hpp"
#include "namerena/battle_resolution.hpp"

using namespace namerena;

// 测试失败时输出原因并立即结束，避免使用额外测试框架影响用户后续本地编译。
static void expect_equal(int actual_value , int expected_value , const string& name)
{
    if(actual_value != expected_value)
    {
        cerr << "测试失败：" << name << "，实际值=" << actual_value << "，期望值=" << expected_value << '\n';
        exit(1);
    }
}

// 这个测试只验证四个基础结算函数，不引入任何具体角色、技能伤害公式或战斗循环。
signed main()
{
    battle::Player source;
    source.id = 1;
    source.display_name = "攻击者";

    battle::Player target;
    target.id = 2;
    target.display_name = "受击者";
    target.max_hp = 100;
    target.hp = 70;
    target.shield_hp = 20;
    target.current_attribute[battle::ATTRIBUTE_SPEED] = 7;

    render::RenderCommandBuffer render_buffer;

    // 50 点伤害先被护盾吸收 20 点，再扣除 30 点生命。
    battle::DamageResult damage_result = battle::damage(source , target , 50 , render_buffer);
    expect_equal(damage_result.shield_absorbed , 20 , "护盾吸收");
    expect_equal(damage_result.hp_lost , 30 , "生命伤害");
    expect_equal(target.hp , 40 , "伤害后生命");

    // 治疗请求为 80，但只能恢复缺失的 60 点生命。
    battle::HealResult heal_result = battle::heal(source , target , 80 , render_buffer);
    expect_equal(heal_result.actual_amount , 60 , "治疗截断");
    expect_equal(target.hp , 100 , "治疗后生命");

    // 减速效果叠加两层：每层 -1 速度，持续两次状态结算。
    battle::StatusEffect slow_status;
    slow_status.id = 101;
    slow_status.status_type = battle::STATUS_DEBUFF;
    slow_status.layer_count = 1;
    slow_status.stack_limit = 3;
    slow_status.duration_turns = 2;
    slow_status.remaining_turns = 2;
    slow_status.code = "slow";
    slow_status.display_name = "减速";
    slow_status.attribute_delta[battle::ATTRIBUTE_SPEED] = -1;

    battle::StatusApplyResult first_slow = battle::add_status(source , target , slow_status , render_buffer);
    battle::StatusApplyResult second_slow = battle::add_status(source , target , slow_status , render_buffer);
    expect_equal(first_slow.layer_count , 1 , "首次施加层数");
    expect_equal(second_slow.layer_count , 2 , "状态叠层");
    expect_equal(target.current_attribute[battle::ATTRIBUTE_SPEED] , 5 , "状态属性修正");

    // 两层中毒在一次结算造成 20 点伤害，并在一回合后到期移除。
    battle::StatusEffect poison_status;
    poison_status.id = 102;
    poison_status.status_type = battle::STATUS_DOT;
    poison_status.layer_count = 2;
    poison_status.duration_turns = 1;
    poison_status.remaining_turns = 1;
    poison_status.value = 10;
    poison_status.code = "poison";
    poison_status.display_name = "中毒";
    battle::add_status(source , target , poison_status , render_buffer);

    vector<battle::Player> player_list = vector<battle::Player>(1);
    player_list.push_back(source);
    player_list.push_back(target);
    battle::StatusSettleResult first_settle = battle::settle_status(target , player_list , render_buffer);
    expect_equal(first_settle.damage_total , 20 , "持续伤害");
    expect_equal(target.hp , 80 , "持续伤害后生命");
    expect_equal((int)target.status_list.size() , 2 , "中毒到期后状态数量");

    // 第二次结算后减速到期，速度必须精确恢复为原来的 7。
    battle::settle_status(target , player_list , render_buffer);
    expect_equal(target.current_attribute[battle::ATTRIBUTE_SPEED] , 7 , "状态到期属性还原");
    expect_equal((int)target.status_list.size() , 1 , "全部状态到期后状态数量");
    expect_equal((int)render_buffer.command_list.size() > 1 , 1 , "直接渲染指令输出");

    cout << "battle_resolution_test passed\n";
    return 0;
}
