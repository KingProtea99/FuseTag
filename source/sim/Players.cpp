// Player characters: spawn, movement and sprint, respawn after falling, removal after a blast, players joining and leaving mid-match.
#include "sim/Systems.h"

#include <algorithm>

namespace fusetag {
namespace {

// The scene's spawn points in turn, stepping aside if there are more players than points. Without spawn points: a row from the origin
Vec3 spawnPosition(const World& world, u32 index) {
    const Tuning& t = world.tuning;
    if (world.spawns.empty()) return {static_cast<f32>(index) * t.spawnSpacing, t.pawnHalfHeight, 0.0f};
    const u32 n = static_cast<u32>(world.spawns.size());
    const f32 offset = static_cast<f32>(index / n) * t.spawnSpacing;
    return world.spawns[index % n] + Vec3{offset, 0.0f, 0.0f};
}

// A capsule moved by the engine's movement mode (mealkit/sim/Movement.h)
Id<Pawn> spawnPawn(World& world, u32 player, const Vec3& position) {
    Pawn p;
    p.player = player;
    p.position = position;
    const Id<Pawn> id = world.state.pawns.create(p);
    mealkit::sim::MoverShape shape;
    shape.position = position;
    shape.halfHeight = world.tuning.pawnHalfHeight;
    shape.radius = world.tuning.pawnRadius;
    shape.mass = world.tuning.pawnMass;
    shape.group = id.slot + 1;
    world.state.pawns.get(id)->mover =
        mealkit::sim::createMover(world.physics, mealkit::sim::MoveMode::Capsule, shape, world.tuning.move);
    return id;
}

mealkit::sim::MoveIntent intentFromInput(const GameInput& in) {
    constexpr f32 kAxis = 127.0f; // const-ok: input quantization (i8 -127..127 → -1..1)
    mealkit::sim::MoveIntent i;
    i.wish = {static_cast<f32>(in.moveX) / kAxis, 0.0f, static_cast<f32>(in.moveZ) / kAxis};
    i.jump = (in.buttons & kButtonJump) != 0;
    return i;
}

u32 staminaFull(const Tuning& t) { return t.staminaMax.value * t.staminaRecover.value; }

// Sprint wish → whether the player sprints this tick. Draining one tick of stamina costs staminaRecover units, a tick of rest gives
// back staminaMax units, so empty → full takes staminaRecover. Running dry leaves the player winded until full.
bool updateStamina(World& world, u32 slot, bool wish, bool moving) {
    const Tuning& t = world.tuning;
    PlayerSlot& p = world.state.slots[slot];
    const u32 full = staminaFull(t);
    const bool sprint = wish && moving && !p.winded && p.stamina > 0;
    if (sprint) {
        p.stamina = p.stamina > t.staminaRecover.value ? p.stamina - t.staminaRecover.value : 0u;
        if (p.stamina == 0) {
            p.winded = true;
            const Pawn* pawn = playerPawn(world.state, slot);
            world.events.push(makeEvent(EventKind::StaminaEmpty, static_cast<u8>(slot), pawn ? pawn->position : Vec3{}, 0.0f));
        }
    } else {
        p.stamina = std::min(full, p.stamina + t.staminaMax.value);
        if (p.stamina == full) p.winded = false;
    }
    if (sprint && !p.sprinting) {
        const Pawn* pawn = playerPawn(world.state, slot);
        world.events.push(makeEvent(EventKind::SprintStart, static_cast<u8>(slot), pawn ? pawn->position : Vec3{}, 0.0f));
    }
    p.sprinting = sprint;
    return sprint;
}

} // namespace

const Pawn* playerPawn(const WorldState& state, u32 slot) {
    return slot < state.playerCount ? state.pawns.get(state.slots[slot].pawn) : nullptr;
}

void respawnPawns(World& world) {
    WorldState& s = world.state;
    for (u32 slot = 0; slot < s.playerCount; ++slot) { // player slot order
        PlayerSlot& p = s.slots[slot];
        if (!aliveInRound(p) || s.pawns.alive(p.pawn) || !p.respawn.ready(s.tick)) continue;
        const Vec3 at = spawnPosition(world, slot);
        p.pawn = spawnPawn(world, slot, at);
        world.events.push(makeEvent(EventKind::Spawn, static_cast<u8>(slot), at, 0.0f));
    }
}

void respawnAll(World& world) {
    WorldState& s = world.state;
    for (u32 slot = 0; slot < s.playerCount; ++slot) {
        PlayerSlot& p = s.slots[slot];
        if (s.pawns.alive(p.pawn)) s.pawns.destroy(p.pawn);
        p.pawn = {};
        p.respawn = {};
    }
    respawnPawns(world); // the old bodies are deleted at the end of the tick
}

void movePawns(World& world) {
    const bool playing = matchPlaying(world); // during countdown and after the end, stand and wait
    const Tuning& t = world.tuning;
    WorldState& s = world.state;
    u32 pathBudget = t.botPathsPerTick;
    for (auto [id, pawn] : s.pawns) { // Pawn slot order
        PlayerSlot& p = s.slots[pawn.player];
        mealkit::sim::MoveIntent intent;
        bool sprintWish = false;
        f32 scale = 1.0f;
        if (playing && !p.out && p.hold.ready(s.tick)) {
            if (botDriven(world, pawn.player)) {
                intent = botIntent(world, pawn.player, pawn, pathBudget, sprintWish);
                scale *= t.botSpeedScale;
            } else {
                intent = intentFromInput(world.inputs[pawn.player]);
                sprintWish = (world.inputs[pawn.player].buttons & kButtonSprint) != 0;
            }
        }
        const bool moving = mealkit::lengthSq(intent.wish) > 0.01f; // const-ok: stick dead zone squared
        if (!p.out && updateStamina(world, pawn.player, sprintWish, moving)) scale *= t.sprintScale;
        if (p.it) scale *= t.itSpeedScale;
        if (!p.boost.ready(s.tick)) scale *= t.escapeBoost;
        if (p.out) continue; // a blown-up body only flies (no movement, no jump)
        if (mealkit::sim::move(world.physics, pawn.mover, intent, t.move, scale, t.gravity, world.tickDt))
            world.events.push(makeEvent(EventKind::Jump, static_cast<u8>(pawn.player), pawn.position, 0.0f));
    }
}

void removeFallenPawns(World& world) {
    WorldState& s = world.state;
    for (auto [id, pawn] : s.pawns) {
        if (pawn.position.y >= world.tuning.fallHeight) continue;
        PlayerSlot& slot = s.slots[pawn.player];
        slot.pawn = {};
        slot.respawn.start(s.tick, world.tuning.respawn);
        world.events.push(makeEvent(EventKind::Fall, static_cast<u8>(pawn.player), pawn.position, 0.0f));
        s.pawns.destroy(id); // the body is deleted at the end of the tick (World::step)
    }
}

void removeBlownPawns(World& world) {
    WorldState& s = world.state;
    for (u32 slot = 0; slot < s.playerCount; ++slot) {
        PlayerSlot& p = s.slots[slot];
        if (!p.out || !s.pawns.alive(p.pawn) || !p.remove.ready(s.tick)) continue;
        s.pawns.destroy(p.pawn);
        p.pawn = {};
    }
}

void sensePawns(World& world) {
    const Tuning& t = world.tuning;
    for (auto [id, pawn] : world.state.pawns) {
        const bool wasAir = pawn.mover.grounded == 0;
        mealkit::sim::senseMover(world.physics, pawn.mover, t.move, pawn.position, pawn.velocity);
        if (pawn.mover.grounded == 0) {
            pawn.fallSpeed = std::max(pawn.fallSpeed, -pawn.velocity.y);
            continue;
        }
        if (wasAir && pawn.fallSpeed >= t.landSpeed)
            world.events.push(makeEvent(EventKind::Land, static_cast<u8>(pawn.player), pawn.position, pawn.fallSpeed));
        pawn.fallSpeed = 0.0f;
    }
}

// Someone joined or left mid-match. A leaving person's seat is taken over by a bot (character, fuse and score stay);
// a joining person takes their seat back, or a new seat that sits the round out until the next one starts
void setPresence(World& world, const mealkit::sim::Presence& presence) {
    WorldState& s = world.state;
    if (presence.player >= mealkit::net::kMaxPlayers) return;
    PlayerSlot& slot = s.slots[presence.player];
    if (!presence.connected) {
        slot.bot = true;
        return;
    }
    slot.bot = false;
    s.humanCount = std::max(s.humanCount, presence.player + 1u);
    if (presence.player >= s.playerCount) {
        s.playerCount = presence.player + 1u;
        slot.connected = true;
        slot.out = true; // joins in the next round (resetRound)
    }
}

} // namespace fusetag
