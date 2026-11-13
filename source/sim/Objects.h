#pragma once

#include "mealkit/core/Reflect.h"
#include "sim/Tables.h"

// Scene object kinds declared by the game. Shown in the editor [Add] menu and inspector, and field values are saved in the scene file.
// The World reads them with scene::readObject<T>(entity) when a match starts (sim/World.cpp loadScene).
// Values that differ per map (ranges, counts, heights) go into the position/size/fields of these objects instead of code constants.
namespace fusetag {

// Play area (one per scene): the box the bots' nav grid is baked in and where they pick places to run to
MEALKIT_OBJECT(MatchArea) {
    static constexpr const char* kObjectLabel = "경기 영역";
    static constexpr mealkit::u32 kObjectFlags = mealkit::reflect::kObjectArea;
};

// Where runners hide (area): a runner standing still inside for hideDelay is hidden from the others until they move,
// shove, get tagged, or the it player stands next to them for searchTime. Drawn by the view as a bush/locker/pile of its size
MEALKIT_ENUM(HideKind, mealkit::u8, Bush, Locker, Pile);
MEALKIT_OBJECT(HideSpot) {
    static constexpr const char* kObjectLabel = "숨는 곳";
    static constexpr mealkit::u32 kObjectFlags = mealkit::reflect::kObjectArea;
    MEALKIT_FIELD(HideKind, kind, HideKind::Bush, "모양");
};

// A loose physics box (crate, barrel, cart) that can be knocked over and shoved to block a corridor. Size = the object's scale
MEALKIT_OBJECT(Prop) {
    static constexpr const char* kObjectLabel = "물리 물체";
    MEALKIT_FIELD(mealkit::f32, mass, 40.0f, "질량 (kg)");
};

// Item crate: touching it gives a random item (tables/items.table weights) to a player without one; comes back after respawn
MEALKIT_OBJECT(ItemCrate) {
    static constexpr const char* kObjectLabel = "아이템 상자";
    MEALKIT_FIELD(mealkit::Ticks, respawn, mealkit::Ticks::seconds(12.0f), "다시 나타나는 시간 (초)");
};

// Rule zone (area): runs a tables/rules.table row for players who enter, stay inside or leave (sim/Rules.cpp)
MEALKIT_OBJECT(RuleZone) {
    static constexpr const char* kObjectLabel = "규칙 구역";
    static constexpr mealkit::u32 kObjectFlags = mealkit::reflect::kObjectArea;
    MEALKIT_FIELD(mealkit::RowRef<Rule>, rule, {}, "규칙 (rules.table 행)");
};

// Rule timer: on for onTime at the start of every period from start; a Timer row reaches every player in the round while it is on
MEALKIT_OBJECT(RuleTimer) {
    static constexpr const char* kObjectLabel = "규칙 타이머";
    static constexpr mealkit::u32 kObjectFlags = mealkit::reflect::kObjectMarker;
    MEALKIT_FIELD(mealkit::Ticks, period, mealkit::Ticks::seconds(30.0f), "주기 (초, 0이면 한 번)");
    MEALKIT_FIELD(mealkit::Ticks, onTime, mealkit::Ticks::seconds(5.0f), "켜져 있는 시간 (초)");
    MEALKIT_FIELD(mealkit::Ticks, start, mealkit::Ticks::seconds(20.0f), "처음 켜지는 때 (매치 시작부터 초)");
    MEALKIT_FIELD(mealkit::RowRef<Rule>, rule, {}, "규칙 (rules.table 행)");
};

} // namespace fusetag
