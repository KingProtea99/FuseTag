// Confirmed command processing: at the very start of World::step(), this tick's commands in player index → send order.
// Game commands are MEALKIT_STRUCTs the view side sends with mealkit::sim::sendCommand(session, value) and reads here with c.read(value).
#include "sim/Commands.h"
#include "sim/Systems.h"

#include "mealkit/sim/Commands.h"

namespace fusetag {
namespace {

void applyCheat(World& world, const DebugCheat& d) {
    WorldState& s = world.state;
    if (d.slot >= s.playerCount) return;
    PlayerSlot& p = s.slots[d.slot];
    switch (d.kind) {
    case CheatKind::MakeIt:
        if (!aliveInRound(p) || !matchPlaying(world) || s.itSlot == d.slot) return;
        if (s.itSlot != kNoSlot) s.slots[s.itSlot].it = false;
        p.it = true;
        s.itSlot = d.slot;
        break;
    case CheatKind::RefillFuse: p.fuse = world.tuning.fuseLength.value; break;
    case CheatKind::BurnFuse: p.fuse = p.fuse > world.tickRate * 5u ? p.fuse - world.tickRate * 5u : 1u; break; // const-ok: 5 seconds
    case CheatKind::Eliminate:
        if (aliveInRound(p) && matchPlaying(world)) eliminate(world, d.slot);
        break;
    case CheatKind::ToggleNavDebug: world.tuning.navDebug = !world.tuning.navDebug; break;
    }
}

} // namespace

void applyCommands(World& world, const mealkit::net::TickInputs& inputs) {
    mealkit::sim::forEachCommand(inputs, [&](const mealkit::sim::Command& c) {
        if (mealkit::sim::Presence p; mealkit::sim::readPresence(c, p)) {
            setPresence(world, p);
            return;
        }
        if (DebugCheat d; c.read(d)) applyCheat(world, d);
    });
}

} // namespace fusetag
