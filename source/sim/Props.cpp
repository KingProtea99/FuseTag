// Loose physics props (scene Prop objects) and the shove that pushes players and props around the shover.
#include "sim/Systems.h"

#include <algorithm>
#include <cmath>

namespace fusetag {
namespace {

namespace scene = mealkit::scene;

f32 flatDistance2(const Vec3& a, const Vec3& b) {
    const f32 dx = a.x - b.x, dz = a.z - b.z;
    return dx * dx + dz * dz;
}

// Horizontal unit direction from a to b (forward when they overlap)
Vec3 awayFrom(const Vec3& a, const Vec3& b) {
    const f32 d2 = flatDistance2(a, b);
    if (d2 < 1e-6f) return {0.0f, 0.0f, 1.0f}; // const-ok: overlapping
    const f32 d = std::sqrt(d2);
    return {(b.x - a.x) / d, 0.0f, (b.z - a.z) / d};
}

} // namespace

void createProps(World& world, const scene::Scene& flat) {
    u32 count = 0;
    for (const scene::Entity& e : flat.entities) count += e.is<Prop>() ? 1u : 0u;
    world.state.props.init(count);
    for (const scene::Entity& e : flat.entities) { // scene order = body creation order
        if (!e.is<Prop>()) continue;
        mealkit::physics::BodyDesc d;
        d.position = e.position;
        d.rotation = e.rotation;
        d.friction = world.tuning.propFriction;
        d.mass = scene::readObject<Prop>(e).mass;
        PropBox b;
        b.position = e.position;
        b.rotation = e.rotation;
        b.halfExtents = e.scale * 0.5f;
        b.color = e.color;
        b.mass = d.mass;
        b.body = world.physics.createBox(b.halfExtents, d);
        world.state.props.create(b);
    }
    world.state.props.endTick();
}

void senseProps(World& world) {
    for (auto [id, prop] : world.state.props) {
        prop.position = world.physics.position(prop.body);
        prop.rotation = world.physics.rotation(prop.body);
    }
}

void shoveFrom(World& world, u32 slot, f32 scale) {
    WorldState& s = world.state;
    const Tuning& t = world.tuning;
    const Pawn* self = playerPawn(s, slot);
    if (!self) return;
    reveal(world, slot);
    const f32 radius = t.shoveRadius * scale;
    const f32 r2 = radius * radius;
    for (u32 other = 0; other < s.playerCount; ++other) {
        PlayerSlot& o = s.slots[other];
        Pawn* pawn = s.pawns.get(o.pawn);
        if (other == slot || !pawn || !aliveInRound(o) || flatDistance2(pawn->position, self->position) >= r2) continue;
        const Vec3 dir = awayFrom(self->position, pawn->position);
        mealkit::sim::setMoverVelocity(world.physics, pawn->mover, dir * (t.shoveSpeed * scale) + Vec3{0.0f, t.shoveLift, 0.0f});
        o.stunned.start(s.tick, t.shoveStun);
        ++s.shoves;
        reveal(world, other);
        world.events.push(makeEvent(EventKind::ShoveHit, static_cast<u8>(other), pawn->position, 0.0f, static_cast<u8>(slot)));
    }
    for (auto [id, prop] : s.props) {
        if (flatDistance2(prop.position, self->position) >= r2) continue;
        const f32 mass = std::max(prop.mass, 1.0f);
        constexpr f32 kReferenceMass = 40.0f; // const-ok: shovePropSpeed is given for a 40 kg prop
        const f32 speed = t.shovePropSpeed * scale * std::min(1.0f, kReferenceMass / mass);
        const Vec3 v = world.physics.linearVelocity(prop.body) + awayFrom(self->position, prop.position) * speed +
                       Vec3{0.0f, speed * 0.3f, 0.0f}; // const-ok: a little lift so props tip over
        world.physics.setLinearVelocity(prop.body, v);
    }
    world.events.push(makeEvent(EventKind::Shove, static_cast<u8>(slot), self->position, radius));
}

void shovePlayers(World& world) {
    WorldState& s = world.state;
    const Tuning& t = world.tuning;
    if (!matchPlaying(world)) return;
    for (u32 slot = 0; slot < s.playerCount; ++slot) { // slot order
        PlayerSlot& p = s.slots[slot];
        if (!playerPawn(s, slot) || !aliveInRound(p) || !p.shove.ready(s.tick) || !p.stunned.ready(s.tick) || !p.hold.ready(s.tick))
            continue;
        const bool wants = botDriven(world, slot) ? p.botShove : (world.inputs[slot].buttons & kButtonShove) != 0;
        if (!wants) continue;
        p.shove.start(s.tick, t.shoveCooldown);
        p.botShove = false;
        shoveFrom(world, slot, 1.0f);
    }
}

} // namespace fusetag
