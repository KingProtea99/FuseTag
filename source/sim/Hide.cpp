// Hiding: a runner standing still inside a HideSpot for hideDelay is hidden (the view doesn't draw them for others).
// Moving, shoving or being tagged shows them again; the it player standing within searchRadius for searchTime finds them
// and pops them out. The it player can't hide. Lockstep peers all hold the hidden position (docs/DESIGN.md "Known limit").
#include "sim/Systems.h"

namespace fusetag {
namespace {

f32 flatDistance2(const Vec3& a, const Vec3& b) {
    const f32 dx = a.x - b.x, dz = a.z - b.z;
    return dx * dx + dz * dz;
}

} // namespace

void reveal(World& world, u32 slot) {
    PlayerSlot& p = world.state.slots[slot];
    p.stillTicks = 0;
    p.searched = 0;
    if (!p.hidden) return;
    p.hidden = false;
    const Pawn* pawn = playerPawn(world.state, slot);
    world.events.push(makeEvent(EventKind::Unhide, static_cast<u8>(slot), pawn ? pawn->position : Vec3{}, static_cast<f32>(p.hideSpot)));
}

void updateHiding(World& world) {
    WorldState& s = world.state;
    const Tuning& t = world.tuning;
    const bool playing = matchPlaying(world);
    const Pawn* hunter = s.itSlot != kNoSlot ? playerPawn(s, s.itSlot) : nullptr;
    for (u32 slot = 0; slot < s.playerCount; ++slot) {
        PlayerSlot& p = s.slots[slot];
        const Pawn* pawn = playerPawn(s, slot);
        if (!playing || !pawn || !aliveInRound(p) || p.it) {
            reveal(world, slot);
            p.hideSpot = kNoSlot;
            continue;
        }
        const Vec3 feet{pawn->position.x, pawn->position.y - pawn->mover.halfHeight, pawn->position.z};
        const u32 spot = hideSpotAt(world, feet);
        const f32 speed2 = pawn->velocity.x * pawn->velocity.x + pawn->velocity.z * pawn->velocity.z;
        const bool still = speed2 < t.hideStillSpeed * t.hideStillSpeed;
        p.hideSpot = spot;
        if (spot == kNoSlot || !still) {
            reveal(world, slot);
            continue;
        }
        if (!p.hidden && ++p.stillTicks >= t.hideDelay.value) {
            p.hidden = true;
            p.searched = 0;
            ++s.hides;
            world.events.push(makeEvent(EventKind::Hide, static_cast<u8>(slot), pawn->position, static_cast<f32>(spot)));
        }
        // The it player searching next to a hidden runner
        if (!p.hidden) continue;
        if (!hunter || flatDistance2(hunter->position, pawn->position) >= t.searchRadius * t.searchRadius) {
            p.searched = 0;
            continue;
        }
        if (++p.searched < t.searchTime.value) continue;
        reveal(world, slot);
        ++s.founds;
        mealkit::sim::setMoverVelocity(world.physics, pawn->mover, Vec3{0.0f, t.foundPop, 0.0f});
        world.events.push(makeEvent(EventKind::Found, static_cast<u8>(slot), pawn->position, static_cast<f32>(spot),
                                    static_cast<u8>(s.itSlot)));
    }
}

} // namespace fusetag
