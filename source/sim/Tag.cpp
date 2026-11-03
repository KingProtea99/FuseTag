// Tagging: it touches a runner and the roles swap on that tick. The new it is held for a moment, the old it runs faster
// for a moment and can't be tagged straight back by the one it just tagged.
#include "sim/Systems.h"

namespace fusetag {
namespace {

f32 flatDistance2(const Vec3& a, const Vec3& b) {
    const f32 dx = a.x - b.x, dz = a.z - b.z;
    return dx * dx + dz * dz;
}

} // namespace

void tagPlayers(World& world) {
    WorldState& s = world.state;
    const Tuning& t = world.tuning;
    if (!matchPlaying(world) || s.itSlot == kNoSlot) return;
    PlayerSlot& it = s.slots[s.itSlot];
    const Pawn* hunter = playerPawn(s, s.itSlot);
    if (!hunter || !it.hold.ready(s.tick)) return;

    // The nearest runner in reach (slot order on ties)
    u32 victim = kNoSlot;
    f32 best = t.tagRange * t.tagRange;
    for (u32 slot = 0; slot < s.playerCount; ++slot) {
        const PlayerSlot& p = s.slots[slot];
        if (slot == s.itSlot || !aliveInRound(p)) continue;
        if (p.immuneFrom == s.itSlot && !p.immune.ready(s.tick)) continue;
        const Pawn* runner = playerPawn(s, slot);
        if (!runner) continue;
        const f32 dy = runner->position.y - hunter->position.y;
        if (dy > t.tagHeight || -dy > t.tagHeight) continue;
        const f32 d2 = flatDistance2(runner->position, hunter->position);
        if (d2 < best) best = d2, victim = slot;
    }
    if (victim == kNoSlot) return;

    const u32 from = s.itSlot;
    PlayerSlot& next = s.slots[victim];
    it.it = false;
    it.immune.start(s.tick, t.tagBackImmunity);
    it.immuneFrom = victim;
    it.boost.start(s.tick, t.escapeBoostTime);
    ++it.tags;
    reveal(world, victim);
    next.it = true;
    next.hold.start(s.tick, t.tagHold);
    next.immune = {};
    next.immuneFrom = kNoSlot;
    s.itSlot = victim;
    ++s.swaps;
    const Vec3 at = (hunter->position + playerPawn(s, victim)->position) * 0.5f;
    world.events.push(makeEvent(EventKind::Tag, static_cast<u8>(from), at, 0.0f, static_cast<u8>(victim)));
}

} // namespace fusetag
