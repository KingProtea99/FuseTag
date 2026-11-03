// Bot brain on the nav grid (also drives players whose input has kButtonAuto: --bot and headless tests).
// Every botThink ticks a bot decides where to go (with some jitter, which is also its reaction time), then follows a nav path there:
//   it:     the nearest runner it may tag; straight at them when close
//   runner: when it is near, the sampled spot farthest from it (and a shove when it's in reach); when it is far, the nearest
//           hide spot, standing still there; otherwise a wander spot nearby
// Bots can't see hidden runners: the it bot chases only runners in sight and finds hidden ones by walking past their spot.
// Paths are searched in slot order, at most botPathsPerTick per tick.
#include "sim/Systems.h"

#include "mealkit/core/DetMath.h"
#include "mealkit/debug/DebugDraw.h"

#include <algorithm>
#include <cmath>

namespace fusetag {
namespace {

f32 flatDistance2(const Vec3& a, const Vec3& b) {
    const f32 dx = a.x - b.x, dz = a.z - b.z;
    return dx * dx + dz * dz;
}

Vec3 feetOf(const Pawn& p) { return {p.position.x, p.position.y - p.mover.halfHeight, p.position.z}; }

Vec3 clampToArena(const World& world, Vec3 p) {
    return {std::clamp(p.x, world.arenaMin.x, world.arenaMax.x), p.y, std::clamp(p.z, world.arenaMin.z, world.arenaMax.z)};
}

// The nearest runner the it bot may tag (not out, not immune from it)
u32 nearestRunner(const World& world, u32 self, const Pawn& pawn) {
    const WorldState& s = world.state;
    u32 best = kNoSlot;
    f32 bestD2 = 0.0f;
    for (u32 slot = 0; slot < s.playerCount; ++slot) {
        const PlayerSlot& p = s.slots[slot];
        if (slot == self || !aliveInRound(p) || p.hidden || (p.immuneFrom == self && !p.immune.ready(s.tick))) continue;
        const Pawn* other = playerPawn(s, slot);
        if (!other) continue;
        const f32 d2 = flatDistance2(other->position, pawn.position);
        if (best == kNoSlot || d2 < bestD2) best = slot, bestD2 = d2;
    }
    return best;
}

// Of botFleeSamples random spots botFleeDistance away (on the nav grid, inside the arena), the one farthest from the hunter
Vec3 fleeSpot(World& world, const Pawn& pawn, const Vec3& hunter) {
    const Tuning& t = world.tuning;
    mealkit::Pcg32& rng = world.state.rng;
    const Vec3 feet = feetOf(pawn);
    Vec3 best = feet;
    f32 bestD2 = -1.0f;
    for (u32 i = 0; i < t.botFleeSamples; ++i) {
        const f32 angle = rng.range(-mealkit::kDetPi, mealkit::kDetPi);
        const Vec3 at = clampToArena(world, feet + Vec3{mealkit::detSin(angle), 0.0f, mealkit::detCos(angle)} * t.botFleeDistance);
        const u32 node = world.nav.empty() ? mealkit::sim::NavGrid::kNone : world.nav.nearest(at);
        const Vec3 spot = node == mealkit::sim::NavGrid::kNone ? at : world.nav.position(node);
        const f32 d2 = flatDistance2(spot, hunter);
        if (d2 > bestD2) bestD2 = d2, best = spot;
    }
    return best;
}

Vec3 wanderSpot(World& world, const Pawn& pawn) {
    mealkit::Pcg32& rng = world.state.rng;
    const f32 angle = rng.range(-mealkit::kDetPi, mealkit::kDetPi);
    const f32 dist = rng.range(0.5f, 1.0f) * world.tuning.botWanderRadius; // const-ok: at least half the radius
    return clampToArena(world, feetOf(pawn) + Vec3{mealkit::detSin(angle), 0.0f, mealkit::detCos(angle)} * dist);
}

// Center (at floor height) of the nearest hide spot within botHideRadius that no other runner is using; false if none
bool nearestHideSpot(const World& world, u32 self, const Pawn& pawn, Vec3& out) {
    const WorldState& s = world.state;
    f32 best = world.tuning.botHideRadius * world.tuning.botHideRadius;
    bool found = false;
    for (u32 i = 0; i < world.hideSpots.size(); ++i) {
        bool taken = false;
        for (u32 slot = 0; slot < s.playerCount; ++slot) taken |= slot != self && s.slots[slot].hideSpot == i && aliveInRound(s.slots[slot]);
        if (taken) continue;
        const World::HideArea& a = world.hideSpots[i];
        const Vec3 c{(a.min.x + a.max.x) * 0.5f, a.min.y, (a.min.z + a.max.z) * 0.5f};
        const f32 d2 = flatDistance2(c, pawn.position);
        if (d2 < best) best = d2, out = c, found = true;
    }
    return found;
}

void think(World& world, u32 self, const Pawn& pawn) {
    const Tuning& t = world.tuning;
    WorldState& s = world.state;
    PlayerSlot& p = s.slots[self];
    p.botThink.start(s.tick, t.botThink.value + s.rng.below(t.botThink.value / 2 + 1));
    p.botSprint = false;
    p.botHiding = false;
    p.botTarget = kNoSlot;
    if (p.it) {
        p.botTarget = nearestRunner(world, self, pawn);
        const Pawn* prey = p.botTarget == kNoSlot ? nullptr : playerPawn(s, p.botTarget);
        if (prey) p.botSearchSpot = kNoSlot;
        // Nobody in sight: check hide spots one by one, standing at each long enough to find someone there
        if (!prey && !world.hideSpots.empty()) {
            if (p.botSearchSpot == kNoSlot || p.botSearchTicks > t.searchTime.value + world.tickRate / 2) {
                p.botSearchSpot = s.rng.below(static_cast<u32>(world.hideSpots.size()));
                p.botSearchTicks = 0;
            }
            const World::HideArea& a = world.hideSpots[p.botSearchSpot];
            p.botGoal = {(a.min.x + a.max.x) * 0.5f, a.min.y, (a.min.z + a.max.z) * 0.5f};
        } else {
            p.botGoal = prey ? feetOf(*prey) : wanderSpot(world, pawn);
        }
        p.botSprint = prey && flatDistance2(prey->position, pawn.position) < t.botSprintRange * t.botSprintRange;
    } else {
        const Pawn* hunter = s.itSlot == kNoSlot ? nullptr : playerPawn(s, s.itSlot);
        const f32 d2 = hunter ? flatDistance2(hunter->position, pawn.position) : 0.0f;
        if (hunter && d2 < t.botFleeRadius * t.botFleeRadius && !p.hidden) {
            p.botGoal = fleeSpot(world, pawn, hunter->position);
            p.botSprint = d2 < t.botSprintRange * t.botSprintRange;
            p.botShove = d2 < t.botShoveRange * t.botShoveRange && p.shove.ready(s.tick);
        } else if (p.hidden || p.hideSpot != kNoSlot) {
            p.botGoal = {pawn.position.x, pawn.position.y - pawn.mover.halfHeight, pawn.position.z}; // stay put
            p.botHiding = true;
        } else if ((!hunter || d2 >= t.botHideFrom * t.botHideFrom) && nearestHideSpot(world, self, pawn, p.botGoal)) {
            p.botHiding = true;
        } else {
            p.botGoal = wanderSpot(world, pawn);
        }
    }
    p.botRepath = true;
}

} // namespace

mealkit::sim::MoveIntent botIntent(World& world, u32 self, const Pawn& pawn, u32& pathBudget, bool& sprint) {
    const Tuning& t = world.tuning;
    WorldState& s = world.state;
    PlayerSlot& p = s.slots[self];
    const Vec3 feet = feetOf(pawn);
    if (p.botThink.ready(s.tick)) think(world, self, pawn);
    // The it bot searching a hide spot: stands next to it until searchTime has passed
    if (p.it && p.botSearchSpot != kNoSlot) {
        const f32 r = t.searchRadius * 0.7f; // const-ok: well inside the search radius
        if (flatDistance2(p.botGoal, pawn.position) < r * r) {
            ++p.botSearchTicks;
            sprint = false;
            return {};
        }
    }
    // A hiding bot that reached its spot stands still (moving would show it); it keeps thinking so it runs when found
    if (p.botHiding && p.hideSpot != kNoSlot) {
        sprint = false;
        return {};
    }
    if (p.botRepath && pathBudget > 0) {
        --pathBudget;
        p.botRepath = false;
        u32 count = 0;
        if (!world.nav.empty())
            mealkit::sim::findPath(world.nav, world.navSearch, feet, p.botGoal, p.botPath.data(), kBotPathPoints, count, t.botPathExpand);
        if (count == 0) { // no grid or no path: head straight for the goal
            p.botPath[0] = p.botGoal;
            count = 1;
        }
        p.botPathCount = count;
        p.botPathNext = 0;
    }

    // It and close to the target: straight at them. Else the next waypoint (reached within half a meter)
    Vec3 goal = feet;
    const Pawn* prey = p.it && p.botTarget != kNoSlot ? playerPawn(s, p.botTarget) : nullptr;
    if (prey && flatDistance2(prey->position, pawn.position) < t.botChaseDirect * t.botChaseDirect) {
        goal = prey->position;
    } else {
        constexpr f32 kReach2 = 0.25f; // const-ok: waypoint reach distance squared
        while (p.botPathNext < p.botPathCount) {
            const Vec3 w = p.botPath[p.botPathNext];
            if (flatDistance2(w, feet) > kReach2) {
                goal = w;
                break;
            }
            ++p.botPathNext;
        }
    }
    if (t.navDebug) {
        Vec3 from = feet;
        for (u32 k = p.botPathNext; k < p.botPathCount; ++k) {
            mealkit::debug::line(from, p.botPath[k], p.it ? mealkit::debug::kRed : mealkit::debug::kGreen);
            from = p.botPath[k];
        }
    }
    mealkit::sim::MoveIntent move;
    const Vec3 to{goal.x - pawn.position.x, 0.0f, goal.z - pawn.position.z};
    const f32 len2 = mealkit::lengthSq(to);
    if (len2 > 0.01f) move.wish = to / std::sqrt(len2); // const-ok: stop distance squared
    // A waypoint well above the feet (a step or ramp the capsule can't walk up) asks for a jump
    if (p.botPathNext < p.botPathCount && p.botPath[p.botPathNext].y - feet.y > t.move.stepUp) move.jump = true;
    sprint = p.botSprint;
    return move;
}

} // namespace fusetag
