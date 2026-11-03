// The fuse: every player has one per round. It burns only while that player is it (faster late in the round), and at 0 the
// player blows up and is out. A new it is then picked at random among the players still in.
#include "sim/Systems.h"

#include "mealkit/core/DetMath.h"

namespace fusetag {
namespace {

// A random player still in the round (rng), held for `hold` ticks
void pickIt(World& world, u32 hold) {
    WorldState& s = world.state;
    u32 alive = 0;
    for (u32 slot = 0; slot < s.playerCount; ++slot) alive += aliveInRound(s.slots[slot]) ? 1u : 0u;
    s.itSlot = kNoSlot;
    if (alive < 2) return; // nobody to chase: the round is over (checkRoundEnd)
    u32 pick = s.rng.below(alive);
    for (u32 slot = 0; slot < s.playerCount; ++slot) {
        if (!aliveInRound(s.slots[slot])) continue;
        if (pick-- != 0) continue;
        PlayerSlot& p = s.slots[slot];
        p.it = true;
        p.hold.start(s.tick, hold);
        s.itSlot = slot;
        const Pawn* pawn = playerPawn(s, slot);
        world.events.push(makeEvent(EventKind::NewIt, static_cast<u8>(slot), pawn ? pawn->position : Vec3{}, 0.0f));
        return;
    }
}

u32 fuseMark(u32 length, f32 fraction) { return static_cast<u32>(static_cast<f32>(length) * fraction); }

} // namespace

void resetRound(World& world) {
    WorldState& s = world.state;
    const Tuning& t = world.tuning;
    for (u32 slot = 0; slot < s.playerCount; ++slot) {
        PlayerSlot& p = s.slots[slot];
        p.fuse = t.fuseLength.value;
        p.it = p.out = p.winded = p.sprinting = false;
        p.outOrder = 0;
        p.hold = p.immune = p.boost = p.remove = p.shove = p.stunned = {};
        p.hidden = p.botHiding = p.botShove = p.botUseItem = false;
        p.speedy = p.vanish = p.scope = {};
        p.hideSpot = kNoSlot;
        p.stillTicks = p.searched = p.botSearchTicks = 0;
        p.botSearchSpot = kNoSlot;
        p.immuneFrom = kNoSlot;
        p.stamina = t.staminaMax.value * t.staminaRecover.value;
        p.botThink = {};
        p.botRepath = false;
        p.botTarget = kNoSlot;
        p.botPathCount = p.botPathNext = 0;
    }
    s.outCount = 0;
    s.itSlot = kNoSlot;
}

void startRound(World& world) {
    world.state.roundStart = world.state.tick;
    pickIt(world, world.tuning.startHold.value);
}

void eliminate(World& world, u32 slot) {
    WorldState& s = world.state;
    const Tuning& t = world.tuning;
    PlayerSlot& p = s.slots[slot];
    p.out = true;
    p.it = false;
    p.fuse = 0;
    p.outOrder = ++s.outCount;
    p.remove.start(s.tick, t.blastRemove);
    Vec3 at;
    if (Pawn* pawn = s.pawns.get(p.pawn)) {
        at = pawn->position;
        const f32 angle = s.rng.range(-mealkit::kDetPi, mealkit::kDetPi);
        const Vec3 launch{mealkit::detSin(angle) * t.blastSpin, t.blastLaunch, mealkit::detCos(angle) * t.blastSpin};
        mealkit::sim::setMoverVelocity(world.physics, pawn->mover, launch);
    }
    world.events.push(makeEvent(EventKind::Eliminated, static_cast<u8>(slot), at, 0.0f));
    if (s.itSlot == slot) pickIt(world, t.tagHold.value);
}

void burnFuse(World& world) {
    WorldState& s = world.state;
    const Tuning& t = world.tuning;
    if (!matchPlaying(world) || s.itSlot == kNoSlot) return;
    const u32 slot = s.itSlot;
    PlayerSlot& p = s.slots[slot];
    const Pawn* pawn = playerPawn(s, slot);
    const Vec3 at = pawn ? pawn->position : Vec3{};
    if (p.hold.readyTick == s.tick && s.tick > 0) world.events.push(makeEvent(EventKind::HoldEnd, static_cast<u8>(slot), at, 0.0f));
    // Burns during the hold too: being tagged costs fuse
    const u32 burn = s.tick - s.roundStart >= t.fuseRampAfter.value ? t.fuseRampBurn : 1u;
    const u32 before = p.fuse;
    p.fuse = p.fuse > burn ? p.fuse - burn : 0u;
    const u32 warn = fuseMark(t.fuseLength.value, t.fuseWarn), danger = fuseMark(t.fuseLength.value, t.fuseDanger);
    if (before > warn && p.fuse <= warn && p.fuse > 0)
        world.events.push(makeEvent(EventKind::FuseWarn, static_cast<u8>(slot), at, t.fuseWarn));
    if (before > danger && p.fuse <= danger && p.fuse > 0)
        world.events.push(makeEvent(EventKind::FuseDanger, static_cast<u8>(slot), at, t.fuseDanger));
    if (p.fuse == 0) eliminate(world, slot);
}

} // namespace fusetag
