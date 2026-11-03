#pragma once

#include "mealkit/core/Reflect.h"
#include "mealkit/core/Types.h"

// Confirmed command types (mealkit/sim/Commands.h): the view side sends them with sim::sendCommand, and applyCommands processes them at the very start of World::step().
// Sent via memcpy, so no strings/containers.
namespace fusetag {

MEALKIT_ENUM(CheatKind, mealkit::u8, MakeIt, RefillFuse, BurnFuse, Eliminate, ToggleNavDebug);

// Debug cheats from the F1 window (dev builds only send them; the simulation applies them in every build so replays match)
MEALKIT_STRUCT(DebugCheat) {
    MEALKIT_FIELD(CheatKind, kind, CheatKind::MakeIt, "종류");
    MEALKIT_FIELD(mealkit::u32, slot, 0, "대상 슬롯");
};

} // namespace fusetag
