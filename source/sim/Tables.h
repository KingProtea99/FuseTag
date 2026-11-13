#pragma once

#include "mealkit/core/Reflect.h"
#include "mealkit/sim/Table.h"
#include "objects/sim/Objects.h"

// Definition data tables (docs/CODING_MODEL.md "Many items of the same shape"): assets/tables/*.table, editor [Tables] panel.
namespace fusetag {

MEALKIT_ENUM(ItemEffect, mealkit::u8, Sprint, Snare, Vanish, Scope, Coolant, BigShove);

// Items from crates (sim/Items.cpp). The row id names the item: string key "item.<id>" on the view side
MEALKIT_STRUCT(ItemKind) {
    MEALKIT_FIELD(ItemEffect, effect, ItemEffect::Sprint, "효과");
    MEALKIT_FIELD(mealkit::u32, weight, 10, "상자에서 나올 비중");
    MEALKIT_FIELD(mealkit::Ticks, duration, mealkit::Ticks::seconds(4.0f), "지속 시간 (초: 달리기, 덫에 묶임, 투명, 투시)");
    MEALKIT_FIELD(mealkit::f32, strength, 1.0f, "세기 (달리기: 속도 배율, 냉각제: 채우는 퓨즈 비율, 큰 밀치기: 범위·속도 배율)");
};
MEALKIT_TABLE(ItemKind, "tables/items.table", 0);

// Rule rows (sim/Rules.cpp, objects module "Rule rows"): RuleZone and RuleTimer objects in a scene pick a row, and the row says
// when (enter, inside, exit, timer), which effect, how much and how long. Effects are added here as maps need them.
MEALKIT_ENUM(RuleEffect, mealkit::u8, Speed);

MEALKIT_STRUCT(Rule) {
    MEALKIT_FIELD(objects::RuleWhen, when, objects::RuleWhen::Inside, "언제 (들어올 때, 안에 있는 동안, 나갈 때, 타이머)");
    MEALKIT_FIELD(RuleEffect, effect, RuleEffect::Speed, "효과");
    MEALKIT_FIELD(mealkit::f32, amount, 1.0f, "세기 (속도: 배율)");
    MEALKIT_FIELD(mealkit::Ticks, duration, {}, "지속 시간 (초, 0이면 안에 있는 동안·타이머가 켜진 동안)");
};
MEALKIT_TABLE(Rule, "tables/rules.table", 0);

} // namespace fusetag
