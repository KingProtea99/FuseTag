// Items: crates in the scene hand out a random item (tables/items.table weights) to a player with an empty slot, and come back
// after their respawn time. The item button uses it; using an item shows a hidden player. Snares stay on the floor until someone steps in.
#include "sim/Systems.h"

#include <algorithm>

namespace fusetag {
namespace {

namespace scene = mealkit::scene;

f32 flatDistance2(const Vec3& a, const Vec3& b) {
    const f32 dx = a.x - b.x, dz = a.z - b.z;
    return dx * dx + dz * dz;
}

Vec3 feetOf(const Pawn& p) { return {p.position.x, p.position.y - p.mover.halfHeight, p.position.z}; }

// A row by weight (rng). kNoItem for an empty table or all weights 0
u32 rollItem(World& world) {
    u32 total = 0;
    for (u32 i = 0; i < world.items.size(); ++i) total += world.items[i].weight;
    if (total == 0) return kNoItem;
    u32 pick = world.state.rng.below(total);
    for (u32 i = 0; i < world.items.size(); ++i) {
        if (pick < world.items[i].weight) return i;
        pick -= world.items[i].weight;
    }
    return kNoItem;
}

void useItem(World& world, u32 slot, const Pawn& pawn) {
    WorldState& s = world.state;
    PlayerSlot& p = s.slots[slot];
    const ItemKind& kind = world.items[p.item];
    const u32 row = p.item;
    p.item = kNoItem;
    ++p.itemsUsed;
    if (kind.effect != ItemEffect::Vanish) reveal(world, slot);
    switch (kind.effect) {
    case ItemEffect::Sprint:
        p.speedy.start(s.tick, kind.duration);
        p.speedyScale = kind.strength;
        break;
    case ItemEffect::Snare: {
        Trap trap;
        trap.position = feetOf(pawn);
        trap.owner = slot;
        trap.armed.start(s.tick, world.tuning.trapArm);
        trap.expires.start(s.tick, world.tuning.trapLife);
        trap.duration = kind.duration.value;
        if (s.traps.count() >= s.traps.capacity()) { // full: the oldest goes (lowest slot order)
            for (auto [id, old] : s.traps) {
                s.traps.destroy(id);
                break;
            }
        }
        s.traps.create(trap);
        world.events.push(makeEvent(EventKind::TrapSet, static_cast<u8>(slot), trap.position, 0.0f));
        break;
    }
    case ItemEffect::Vanish: p.vanish.start(s.tick, kind.duration); break;
    case ItemEffect::Scope: p.scope.start(s.tick, kind.duration); break;
    case ItemEffect::Coolant: {
        const u32 add = static_cast<u32>(static_cast<f32>(world.tuning.fuseLength.value) * kind.strength);
        p.fuse = std::min(world.tuning.fuseLength.value, p.fuse + add);
        break;
    }
    case ItemEffect::BigShove: shoveFrom(world, slot, kind.strength); break;
    }
    world.events.push(makeEvent(EventKind::ItemUse, static_cast<u8>(slot), pawn.position, static_cast<f32>(row)));
}

} // namespace

void createCrates(World& world, const scene::Scene& flat) {
    u32 count = 0;
    for (const scene::Entity& e : flat.entities) count += e.is<ItemCrate>() ? 1u : 0u;
    world.state.crates.init(count);
    for (const scene::Entity& e : flat.entities) {
        if (!e.is<ItemCrate>()) continue;
        Crate c;
        c.position = e.position;
        c.respawn = scene::readObject<ItemCrate>(e).respawn.value;
        world.state.crates.create(c);
    }
    world.state.crates.endTick();
}

void updateItems(World& world) {
    WorldState& s = world.state;
    const Tuning& t = world.tuning;
    if (!matchPlaying(world)) return;
    // Crates come back, then get picked up (crate order, then slot order)
    for (auto [id, crate] : s.crates) {
        if (!crate.full && crate.refill.ready(s.tick)) {
            crate.full = true;
            world.events.push(makeEvent(EventKind::CrateBack, kNoPlayer, crate.position, 0.0f));
        }
        if (!crate.full) continue;
        for (u32 slot = 0; slot < s.playerCount; ++slot) {
            PlayerSlot& p = s.slots[slot];
            const Pawn* pawn = playerPawn(s, slot);
            if (!pawn || !aliveInRound(p) || p.item != kNoItem) continue;
            const f32 dy = pawn->position.y - crate.position.y;
            if (flatDistance2(pawn->position, crate.position) >= t.crateRadius * t.crateRadius || dy > 2.0f || dy < -2.0f) continue; // const-ok: about one character height
            p.item = rollItem(world);
            if (p.item == kNoItem) break;
            crate.full = false;
            crate.refill.start(s.tick, crate.respawn);
            ++s.pickups;
            world.events.push(makeEvent(EventKind::Pickup, static_cast<u8>(slot), crate.position, static_cast<f32>(p.item)));
            break;
        }
    }
    // Item button
    for (u32 slot = 0; slot < s.playerCount; ++slot) {
        PlayerSlot& p = s.slots[slot];
        const Pawn* pawn = playerPawn(s, slot);
        if (!pawn || !aliveInRound(p) || p.item == kNoItem || p.item >= world.items.size() || !p.stunned.ready(s.tick)) continue;
        const bool wants = botDriven(world, slot) ? p.botUseItem : (world.inputs[slot].buttons & kButtonItem) != 0;
        if (!wants) continue;
        p.botUseItem = false;
        useItem(world, slot, *pawn);
    }
    // Snares
    for (auto [id, trap] : s.traps) {
        if (trap.expires.ready(s.tick)) {
            s.traps.destroy(id);
            continue;
        }
        if (!trap.armed.ready(s.tick)) continue;
        for (u32 slot = 0; slot < s.playerCount; ++slot) {
            PlayerSlot& p = s.slots[slot];
            const Pawn* pawn = playerPawn(s, slot);
            if (slot == trap.owner || !pawn || !aliveInRound(p)) continue;
            const Vec3 feet = feetOf(*pawn);
            const f32 dy = feet.y - trap.position.y;
            if (flatDistance2(feet, trap.position) >= t.trapRadius * t.trapRadius || dy > 0.6f || dy < -0.6f) continue; // const-ok: about a step
            p.stunned.start(s.tick, trap.duration);
            reveal(world, slot);
            ++s.snares;
            world.events.push(makeEvent(EventKind::Snared, static_cast<u8>(slot), trap.position, 0.0f, static_cast<u8>(trap.owner)));
            s.traps.destroy(id);
            break;
        }
    }
}

} // namespace fusetag
