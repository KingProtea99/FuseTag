#pragma once

#include "mealkit/core/Reflect.h"

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

} // namespace fusetag
