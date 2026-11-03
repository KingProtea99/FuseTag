#pragma once

#include "mealkit/core/Reflect.h"
#include "mealkit/sim/Table.h"

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

} // namespace fusetag
