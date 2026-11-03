#pragma once

#include "sim/World.h"
#include "view/ViewTuning.h"

#include "mealkit/render/Renderer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

// Game feel on the view side (docs/ROADMAP.md M2): squash and stretch, lean, bob, the bomb's flight between players, short lights,
// and the post-effect / fov kicks. Driven by tick events (onEvent) and interpolated state (update); never feeds back into sim/.
// All numbers are in ViewTuning::juice, so they are tuned live from the [Tuning] panel's View tab.
namespace fusetag {

using mealkit::f64;

namespace ease {
inline f32 outCubic(f32 t) { const f32 u = 1.0f - t; return 1.0f - u * u * u; }
inline f32 outBack(f32 t) { // overshoots a little, then settles
    constexpr f32 c1 = 1.70158f, c3 = c1 + 1.0f; // const-ok: standard back easing constants
    const f32 u = t - 1.0f;
    return 1.0f + c3 * u * u * u + c1 * u * u;
}
inline f32 inOutSine(f32 t) { return 0.5f - 0.5f * std::cos(t * 3.14159265f); }
} // namespace ease

// Damped spring toward a target (frequency in Hz, damping 1 = critical, below 1 wobbles). Semi-implicit Euler on view time
struct Spring {
    f32 value = 0.0f, velocity = 0.0f;
    void update(f32 target, f32 frequency, f32 damping, f32 dt);
    void kick(f32 impulse) { velocity = std::clamp(velocity + impulse, -kMaxVelocity, kMaxVelocity); } // repeated kicks don't pile up
    static constexpr f32 kMaxVelocity = 8.0f; // const-ok: about the biggest single kick
};

// How one character is drawn this frame
struct PawnLook {
    Vec3 scale{1, 1, 1}; // multiplied into the body size
    mealkit::Quat rotation;
    Vec3 offset;         // bob, added to the position
    f32 flash = 0.0f;    // 0..1 toward white
};

class Juice {
public:
    void reset();
    void onEvent(const GameEvent& e, const World& world, u8 localPlayer, f64 now);
    // Every frame. positions = interpolated character centers (valid where has[slot]); now = view clock (frozen in hitstop)
    void update(const World& world, const std::array<Vec3, mealkit::net::kMaxPlayers>& positions,
                const std::array<bool, mealkit::net::kMaxPlayers>& has, f32 dt, f64 now, const JuiceTuning& t);

    PawnLook look(u32 slot) const;
    Vec3 velocity(u32 slot) const { return slot < m_pawns.size() ? m_pawns[slot].lastVelocity : Vec3{}; } // from interpolated motion
    bool shoved(u32 slot) const; // true once after this player shoved (animation trigger)
    // Where the bomb is drawn (above the it player, or flying between two players after a tag). false = no bomb
    bool bomb(Vec3& at, f32& scale, f32& flash) const;
    void addLights(std::vector<mealkit::render::PointLight>& lights) const;
    // Post and camera additions for this window's player
    void applyPost(mealkit::render::PostParams& post, f32 flashScale) const;
    f32 fovOffset() const { return m_fov.value; }
    f32 cameraDip() const { return m_dip.value; } // added to the camera target height (landing)

    // "It!" marker popping over the new it. false = none now
    bool itMarker(Vec3& at, f32& scale, mealkit::Quat& rotation) const;
    // Afterimages of the player who just passed the bomb on (while their escape boost runs)
    struct Ghost {
        u32 slot = 0;
        Vec3 position;
        f32 fade = 1.0f; // 1 newest .. 0 oldest
    };
    void ghosts(std::vector<Ghost>& out) const;
    // Arrow on the ground around me toward the it player when they're near (I'm a runner). strength 0..1 = how near
    bool hunterArrow(Vec3& at, mealkit::Quat& rotation, f32& strength) const;
    // Who to watch while I'm out: the it player, else anyone still in (slot = whose body it is)
    bool spectateTarget(Vec3& at, u32& slot) const;
    // Hide spot wobble (scale and tilt) by World::hideSpots index
    void spotLook(u32 spot, Vec3& scale, mealkit::Quat& rotation) const;

private:
    struct PawnFx {
        Spring squash, leanX, leanZ;
        Vec3 lastVelocity;
        Vec3 lastPosition;
        bool seen = false;
        f32 bobPhase = 0.0f;
        f64 flashUntil = 0.0, holdUntil = 0.0;
        f32 spin = 0.0f, spinVelocity = 0.0f;
        f64 outStart = -1.0;     // when this player blew up (tumble, shrink)
        Vec3 tumbleAxis{1, 0, 0};
        std::array<Vec3, 8> trail{}; // afterimage positions, newest first
        u32 trailCount = 0;
        f64 nextTrail = 0.0;
        mutable bool shoveTrigger = false; // consumed by shoved() during drawing
    };
    struct Light {
        mealkit::render::PointLight light;
        f64 start = 0.0, end = 0.0;
    };
    void addLight(Vec3 at, f32 range, Vec3 color, f32 seconds, f64 now);

    std::array<PawnFx, mealkit::net::kMaxPlayers> m_pawns{};
    std::array<Vec3, mealkit::net::kMaxPlayers> m_positions{};
    std::array<bool, mealkit::net::kMaxPlayers> m_has{};
    std::vector<Light> m_lights;
    JuiceTuning m_t;
    f64 m_now = 0.0;
    // The bomb
    u32 m_bombOwner = kNoSlot;
    u32 m_flightFrom = kNoSlot;
    f64 m_flightStart = -1.0;
    f32 m_fuseLeft = 1.0f;
    f32 m_pulsePhase = 0.0f;
    f64 m_nextSpark = 0.0;
    // This window's player
    u8 m_local = kNoPlayer;
    bool m_localIt = false;
    Spring m_fov, m_dip;
    std::vector<Spring> m_spots; // wobble per hide spot
    u32 m_markerSlot = kNoSlot;
    f64 m_markerStart = -1.0;
    f32 m_removeSeconds = 2.0f; // tuning.blastRemove in seconds
    u32 m_itSlot = kNoSlot;
    f64 m_bloomUntil = 0.0;
    f64 m_aberrationUntil = 0.0;

public:
    // Sparks to spawn this frame at the bomb's fuse (GameSession plays them through ctx.feedback.playEffect)
    std::vector<Vec3> sparks;
    // Bomb pulses this frame (position, fuse left 0..1) for the tick sound, and whether my heartbeat beats this frame
    std::vector<std::pair<Vec3, f32>> ticks;
    bool heartbeat = false;
};

} // namespace fusetag
