#include "view/Juice.h"

#include <algorithm>

using namespace mealkit;

namespace fusetag {
namespace {

constexpr f32 kTau = 6.28318531f; // const-ok: 2π

} // namespace

void Spring::update(f32 target, f32 frequency, f32 damping, f32 dt) {
    const f32 w = kTau * frequency;
    velocity += (-(value - target) * w * w - 2.0f * damping * w * velocity) * dt;
    value += velocity * dt;
}

void Juice::reset() { *this = Juice{}; }

void Juice::addLight(Vec3 at, f32 range, Vec3 color, f32 seconds, f64 now) {
    Light l;
    l.light.position = at;
    l.light.range = range;
    l.light.color = color;
    l.start = now;
    l.end = now + seconds;
    m_lights.push_back(l);
}

void Juice::onEvent(const GameEvent& e, const World& world, u8 localPlayer, f64 now) {
    (void)world;
    const JuiceTuning& t = m_t;
    m_local = localPlayer;
    PawnFx* fx = e.player < m_pawns.size() ? &m_pawns[e.player] : nullptr;
    switch (e.kind) {
    case EventKind::Jump:
        if (fx) fx->squash.kick(t.jumpKick);
        break;
    case EventKind::Land:
        if (fx) fx->squash.kick(std::max(t.landKickMax, t.landKick * e.value));
        if (e.player == localPlayer) m_dip.kick(t.landDip * e.value);
        break;
    case EventKind::Tag: {
        if (fx) {
            fx->squash.kick(t.tagTaggerKick);
            fx->flashUntil = now + t.flashSeconds;
        }
        if (e.other < m_pawns.size()) {
            PawnFx& victim = m_pawns[e.other];
            victim.squash.kick(t.tagVictimKick);
            victim.spinVelocity += t.tagVictimSpin;
            victim.flashUntil = now + t.flashSeconds;
        }
        m_flightFrom = e.player;
        m_flightStart = now;
        m_bombOwner = e.other;
        m_markerSlot = e.other;
        m_markerStart = now;
        addLight(e.position + Vec3{0.0f, 0.5f, 0.0f}, t.tagLightRange, t.tagLight, t.tagLightSeconds, now);
        if (e.player == localPlayer || e.other == localPlayer) m_fov.kick(t.fovTagKick);
        break;
    }
    case EventKind::Hide:
    case EventKind::Unhide:
    case EventKind::Found: {
        const u32 spot = static_cast<u32>(e.value);
        if (spot < 1024) { // const-ok: sanity bound on the index
            if (m_spots.size() <= spot) m_spots.resize(spot + 1);
            m_spots[spot].kick(e.kind == EventKind::Found ? t.spotFoundKick : t.spotKick);
        }
        if (fx && e.kind == EventKind::Found) fx->squash.kick(t.holdEndKick);
        break;
    }
    case EventKind::Snared:
        if (fx) {
            fx->squash.kick(t.shoveHitKick);
            fx->flashUntil = now + t.flashSeconds;
        }
        break;
    case EventKind::Pickup:
    case EventKind::ItemUse:
        if (fx) fx->squash.kick(t.shoveKick * 0.6f);
        break;
    case EventKind::Shove:
        if (fx) {
            fx->squash.kick(t.shoveKick);
            fx->shoveTrigger = true;
        }
        break;
    case EventKind::ShoveHit:
        if (fx) {
            fx->squash.kick(t.shoveHitKick);
            fx->spinVelocity += t.shoveHitSpin;
            fx->flashUntil = now + t.flashSeconds;
        }
        if (e.player == localPlayer) m_fov.kick(t.fovTagKick * 0.5f);
        break;
    case EventKind::HoldEnd:
        if (fx) fx->squash.kick(t.holdEndKick);
        break;
    case EventKind::NewIt:
        m_bombOwner = e.player;
        m_flightStart = -1.0;
        m_markerSlot = e.player;
        m_markerStart = now;
        if (fx) fx->squash.kick(t.tagVictimKick * 0.5f);
        break;
    case EventKind::Eliminated:
        addLight(e.position, t.blastLightRange, t.blastLight, t.blastLightSeconds, now);
        m_bloomUntil = now + t.blastBloomSeconds;
        if (fx) {
            fx->squash.kick(-t.tagVictimKick);
            fx->outStart = now;
            const f32 a = e.position.x * 7.0f + e.position.z * 3.0f; // const-ok: any angle that varies per blast
            fx->tumbleAxis = normalize(Vec3{std::cos(a), 0.3f, std::sin(a)}); // const-ok: mostly sideways
        }
        if (m_bombOwner == e.player) m_bombOwner = kNoSlot;
        break;
    case EventKind::FuseDanger:
        if (e.player == localPlayer) m_aberrationUntil = now + 0.3; // const-ok: a short kick when the danger mark is crossed
        break;
    case EventKind::RoundWin:
    case EventKind::MatchStart:
        m_bombOwner = kNoSlot;
        m_flightStart = -1.0;
        break;
    default: break;
    }
}

void Juice::update(const World& world, const std::array<Vec3, net::kMaxPlayers>& positions, const std::array<bool, net::kMaxPlayers>& has,
                   f32 dt, f64 now, const JuiceTuning& t) {
    m_t = t;
    m_now = now;
    m_positions = positions;
    m_has = has;
    const WorldState& s = world.state;
    sparks.clear();
    ticks.clear();
    heartbeat = false;
    if (dt <= 0.0f) return; // hitstop: everything holds still
    dt = std::min(dt, 0.05f); // const-ok: stalls don't explode the springs

    // The it player is the sim's truth; the bomb flight only delays where it is drawn
    m_bombOwner = s.itSlot;
    m_itSlot = s.itSlot;
    m_removeSeconds = static_cast<f32>(world.tuning.blastRemove.value) * world.tickDt;
    m_localIt = m_local < s.playerCount && s.slots[m_local].it;
    m_fuseLeft = s.itSlot != kNoSlot ? fuseFraction(world, s.itSlot) : 1.0f;
    const f32 pulseRate = t.bombPulseSlow + (t.bombPulseFast - t.bombPulseSlow) * (1.0f - m_fuseLeft);
    const f32 before = m_pulsePhase;
    m_pulsePhase = std::fmod(m_pulsePhase + pulseRate * dt, 1.0f);
    if (m_pulsePhase < before && s.itSlot != kNoSlot && has[s.itSlot]) { // a new pulse
        ticks.emplace_back(positions[s.itSlot], m_fuseLeft);
        heartbeat = m_localIt && m_fuseLeft < t.bombFlashBelow;
    }

    for (u32 slot = 0; slot < net::kMaxPlayers; ++slot) {
        PawnFx& fx = m_pawns[slot];
        if (!has[slot]) {
            fx = PawnFx{};
            continue;
        }
        const Vec3 velocity = fx.seen ? (positions[slot] - fx.lastPosition) / dt : Vec3{};
        const Vec3 accel = fx.seen ? (velocity - fx.lastVelocity) / dt : Vec3{};
        fx.lastPosition = positions[slot];
        fx.lastVelocity = velocity;
        fx.seen = true;
        // Afterimages while the escape boost runs (sampled every ghostInterval)
        const bool boosting = (!s.slots[slot].boost.ready(s.tick) || !s.slots[slot].speedy.ready(s.tick)) && !s.slots[slot].out;
        if (boosting && now >= fx.nextTrail) {
            for (u32 k = static_cast<u32>(fx.trail.size()) - 1; k > 0; --k) fx.trail[k] = fx.trail[k - 1];
            fx.trail[0] = positions[slot];
            fx.trailCount = std::min<u32>(fx.trailCount + 1, std::min<u32>(t.ghostCount, static_cast<u32>(fx.trail.size())));
            fx.nextTrail = now + t.ghostInterval;
        }
        if (!boosting) fx.trailCount = 0;
        const f32 speed = std::sqrt(velocity.x * velocity.x + velocity.z * velocity.z);
        const bool grounded = std::fabs(velocity.y) < 0.5f; // const-ok: view guess of being on the ground

        // Squash target: stretched while rising, a little while running, shaking while held
        f32 target = std::clamp(velocity.y * t.airStretch, -t.airStretchMax, t.airStretchMax);
        if (grounded) target = t.runStretch * std::min(speed / t.runSpeed, 1.0f);
        const PlayerSlot& p = s.slots[slot];
        if (p.it && !p.hold.ready(s.tick)) target += t.holdShake * std::sin(static_cast<f32>(now) * t.holdShakeRate * kTau);
        if (!p.stunned.ready(s.tick)) target += t.snaredShake * std::sin(static_cast<f32>(now) * t.holdShakeRate * 1.3f * kTau); // const-ok: a bit off the hold rhythm
        fx.squash.update(target, t.squashFrequency, t.squashDamping, dt);
        fx.squash.value = std::clamp(fx.squash.value, -0.6f, 0.8f); // const-ok: never flip or vanish

        // Lean into horizontal acceleration (tilt toward where it pushes)
        const f32 lx = std::clamp(accel.z * t.leanPerAccel, -t.leanMax, t.leanMax);
        const f32 lz = std::clamp(-accel.x * t.leanPerAccel, -t.leanMax, t.leanMax);
        fx.leanX.update(grounded ? lx : 0.0f, t.leanFrequency, t.leanDamping, dt);
        fx.leanZ.update(grounded ? lz : 0.0f, t.leanFrequency, t.leanDamping, dt);

        if (grounded) fx.bobPhase = std::fmod(fx.bobPhase + speed * t.bobCadence * dt, 1.0f);
        // Tag spin slows down to a stop
        fx.spin += fx.spinVelocity * dt;
        fx.spinVelocity *= std::max(0.0f, 1.0f - 4.0f * dt); // const-ok: spin friction (1/s)
        if (std::fabs(fx.spinVelocity) < 0.5f) fx.spin *= std::max(0.0f, 1.0f - 10.0f * dt); // const-ok: settle back to facing
    }

    // Sparks from the fuse, faster as it burns down
    Vec3 at;
    f32 scale = 0.0f, flash = 0.0f;
    if (bomb(at, scale, flash) && now >= m_nextSpark) {
        const f32 rate = t.sparkRateSlow + (t.sparkRateFast - t.sparkRateSlow) * (1.0f - m_fuseLeft);
        m_nextSpark = now + 1.0 / std::max(rate, 1.0f);
        sparks.push_back(at + Vec3{0.0f, t.bombSize * 0.6f, 0.0f});
    }

    // fov: wider with my speed, plus kicks
    f32 mySpeed = 0.0f;
    if (m_local < net::kMaxPlayers && has[m_local]) {
        const Vec3 v = m_pawns[m_local].lastVelocity;
        mySpeed = std::sqrt(v.x * v.x + v.z * v.z);
    }
    m_dip.update(0.0f, t.dipFrequency, t.dipDamping, dt);
    for (Spring& sp : m_spots) sp.update(0.0f, t.squashFrequency, t.squashDamping, dt);
    m_fov.update(t.fovSpeed * std::min(mySpeed / t.runSpeed, 1.0f), t.fovFrequency, t.fovDamping, dt);

    m_lights.erase(std::remove_if(m_lights.begin(), m_lights.end(), [now](const Light& l) { return now >= l.end; }), m_lights.end());
}

bool Juice::shoved(u32 slot) const {
    if (slot >= m_pawns.size() || !m_pawns[slot].shoveTrigger) return false;
    m_pawns[slot].shoveTrigger = false;
    return true;
}

PawnLook Juice::look(u32 slot) const {
    PawnLook out;
    if (slot >= m_pawns.size()) return out;
    const PawnFx& fx = m_pawns[slot];
    const f32 y = 1.0f + fx.squash.value;
    const f32 xz = 1.0f / std::sqrt(std::max(y, 0.2f)); // const-ok: keep the volume
    out.scale = {xz, y, xz};
    out.rotation = normalize(quatFromAxisAngle({1, 0, 0}, fx.leanX.value) * quatFromAxisAngle({0, 0, 1}, fx.leanZ.value) *
                             quatFromAxisAngle({0, 1, 0}, fx.spin));
    out.offset = {0.0f, m_t.bobHeight * std::fabs(std::sin(fx.bobPhase * kTau * 0.5f)), 0.0f};
    if (fx.outStart >= 0.0) { // blown up: tumble, then shrink away just before the body is removed
        const f32 since = static_cast<f32>(m_now - fx.outStart);
        out.rotation = normalize(quatFromAxisAngle(fx.tumbleAxis, since * m_t.tumbleSpeed) * out.rotation);
        const f32 left = m_removeSeconds - since;
        if (left < m_t.shrinkSeconds) out.scale = out.scale * std::max(0.0f, left / std::max(m_t.shrinkSeconds, 0.01f));
        out.offset = {};
    }
    if (m_now < fx.flashUntil) out.flash = static_cast<f32>((fx.flashUntil - m_now) / std::max(m_t.flashSeconds, 0.01f));
    return out;
}

bool Juice::bomb(Vec3& at, f32& scale, f32& flash) const {
    if (m_bombOwner == kNoSlot || !m_has[m_bombOwner]) return false;
    const JuiceTuning& t = m_t;
    auto head = [&](u32 slot) { return m_positions[slot] + Vec3{0.0f, 0.9f + t.bombHeight, 0.0f}; }; // const-ok: pawn half height
    at = head(m_bombOwner);
    const f32 flightT = m_flightStart >= 0.0 ? static_cast<f32>((m_now - m_flightStart) / std::max(t.bombFlightSeconds, 0.01f)) : 1.0f;
    if (flightT < 1.0f && m_flightFrom < m_has.size() && m_has[m_flightFrom]) {
        const f32 k = ease::outBack(flightT);
        at = head(m_flightFrom) + (at - head(m_flightFrom)) * k + Vec3{0.0f, t.bombArc * 4.0f * flightT * (1.0f - flightT), 0.0f};
    }
    const f32 pulse = std::sin(m_pulsePhase * kTau);
    scale = t.bombSize * (1.0f + t.bombPulseSize * pulse);
    flash = m_fuseLeft < t.bombFlashBelow && pulse > 0.0f ? 0.8f : 0.0f; // const-ok: flash amount
    return true;
}

void Juice::spotLook(u32 spot, Vec3& scale, Quat& rotation) const {
    const f32 v = spot < m_spots.size() ? m_spots[spot].value : 0.0f;
    scale = {1.0f + v * 0.5f, 1.0f - v * 0.3f, 1.0f + v * 0.5f}; // const-ok: wobble shape
    rotation = quatFromAxisAngle({0, 0, 1}, v * 0.15f); // const-ok: a little sway
}

bool Juice::itMarker(Vec3& at, f32& scale, Quat& rotation) const {
    const JuiceTuning& t = m_t;
    if (m_markerSlot == kNoSlot || m_markerSlot != m_itSlot || m_markerStart < 0.0) return false;
    const f32 since = static_cast<f32>(m_now - m_markerStart);
    if (since >= t.markerSeconds) return false;
    f32 bombScale = 0.0f, flash = 0.0f;
    if (!bomb(at, bombScale, flash)) return false;
    constexpr f32 kPop = 0.25f; // const-ok: pop-in and fade-out time (s)
    const f32 grow = ease::outBack(std::min(since / kPop, 1.0f));
    const f32 shrink = std::clamp((t.markerSeconds - since) / kPop, 0.0f, 1.0f);
    scale = t.markerSize * grow * shrink;
    at = at + Vec3{0.0f, t.markerHeight + 0.1f * std::sin(since * 18.0f), 0.0f}; // const-ok: little bounce
    rotation = quatFromAxisAngle({0, 1, 0}, since * 6.0f) * quatFromAxisAngle({0, 0, 1}, 0.785398f); // const-ok: spinning diamond
    return true;
}

void Juice::ghosts(std::vector<Ghost>& out) const {
    for (u32 slot = 0; slot < m_pawns.size(); ++slot) {
        const PawnFx& fx = m_pawns[slot];
        for (u32 k = 1; k < fx.trailCount; ++k) // [0] is about where the body is now
            out.push_back({slot, fx.trail[k], 1.0f - static_cast<f32>(k) / static_cast<f32>(fx.trailCount)});
    }
}

bool Juice::hunterArrow(Vec3& at, Quat& rotation, f32& strength) const {
    if (m_local >= m_has.size() || !m_has[m_local] || m_itSlot == kNoSlot || m_itSlot == m_local || !m_has[m_itSlot]) return false;
    if (m_pawns[m_local].outStart >= 0.0) return false;
    const Vec3 d{m_positions[m_itSlot].x - m_positions[m_local].x, 0.0f, m_positions[m_itSlot].z - m_positions[m_local].z};
    const f32 dist = std::sqrt(d.x * d.x + d.z * d.z);
    if (dist >= m_t.arrowRange || dist < 0.01f) return false; // const-ok: overlapping
    const Vec3 dir = d / dist;
    strength = 1.0f - dist / m_t.arrowRange;
    at = m_positions[m_local] + dir * m_t.arrowDistance + Vec3{0.0f, -0.85f, 0.0f}; // const-ok: just above the floor
    rotation = quatFromAxisAngle({0, 1, 0}, std::atan2(dir.x, dir.z));
    return true;
}

bool Juice::spectateTarget(Vec3& at) const {
    if (m_itSlot != kNoSlot && m_itSlot < m_has.size() && m_has[m_itSlot]) {
        at = m_positions[m_itSlot];
        return true;
    }
    for (u32 slot = 0; slot < m_has.size(); ++slot)
        if (m_has[slot] && m_pawns[slot].outStart < 0.0) {
            at = m_positions[slot];
            return true;
        }
    return false;
}

void Juice::addLights(std::vector<render::PointLight>& lights) const {
    Vec3 at;
    f32 scale = 0.0f, flash = 0.0f;
    if (bomb(at, scale, flash)) {
        render::PointLight l;
        l.position = at + Vec3{0.0f, m_t.bombSize * 0.6f, 0.0f};
        l.range = m_t.bombLightRange * (0.8f + 0.2f * std::sin(static_cast<f32>(m_now) * 37.0f)); // const-ok: flicker
        l.color = m_t.bombLight * (flash > 0.0f ? 2.0f : 1.0f);
        lights.push_back(l);
    }
    for (const Light& l : m_lights) {
        const f32 k = 1.0f - static_cast<f32>((m_now - l.start) / std::max(l.end - l.start, 0.001));
        render::PointLight p = l.light;
        p.color = p.color * (k * k);
        lights.push_back(p);
    }
}

void Juice::applyPost(render::PostParams& post, f32 flashScale) const {
    const JuiceTuning& t = m_t;
    if (m_now < m_bloomUntil) post.bloomIntensity += t.blastBloom * flashScale * static_cast<f32>((m_bloomUntil - m_now) / t.blastBloomSeconds);
    if (!m_localIt) return;
    const f32 beat = 0.5f + 0.5f * std::sin(m_pulsePhase * kTau);
    post.vignette = std::max(post.vignette, t.dangerVignette + t.dangerPulse * (1.0f - m_fuseLeft) * beat * flashScale);
    if (m_fuseLeft < t.bombFlashBelow || m_now < m_aberrationUntil)
        post.chromaticAberration += t.dangerAberration * beat * flashScale;
}

} // namespace fusetag
