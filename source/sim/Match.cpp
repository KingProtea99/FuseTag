// Match flow: the engine's mealkit::sim::advanceMatch decides phases (waiting → countdown → playing → round end → results)
// from tuning rules; this file calls the game's own functions on each transition.
// A round ends when one player is left (checkRoundEnd → finishRound). Rounds are ended with no flow winner so every one of
// `rounds` is played; the match ranking is the placement points in PlayerSlot::score (the results screen sorts by them).
// All peers reach the same phase on the same tick (state field). The screen flow (app/) only reads the phase.
#include "sim/Systems.h"

namespace fusetag {
namespace {

using mealkit::sim::FlowEvent;

std::array<mealkit::sim::MatchPlayer, mealkit::net::kMaxPlayers> matchPlayers(const World& world) {
    std::array<mealkit::sim::MatchPlayer, mealkit::net::kMaxPlayers> players{};
    for (u32 i = 0; i < world.state.playerCount; ++i) {
        players[i].present = world.state.slots[i].connected;
        players[i].alive = true;
        players[i].score = world.state.slots[i].score;
    }
    return players;
}

// Everyone back in place with a full fuse. Round 1 was set up by World::reset; later rounds at their countdown,
// so the countdown already shows everyone standing at the spawn points
void setUpNextRound(World& world) {
    if (world.state.flow.round == 0) return;
    resetRound(world);
    for (auto [id, trap] : world.state.traps) world.state.traps.destroy(id);
    for (auto [id, crate] : world.state.crates) crate.full = true;
    respawnAll(world);
}

void handle(World& world, FlowEvent event) {
    switch (event) {
    case FlowEvent::CountdownStart: setUpNextRound(world); break;
    case FlowEvent::RoundStart:
        if (world.rules.countdown == 0 && world.state.flow.round > 1) setUpNextRound(world); // flow.round is already the new one
        startRound(world); // the first it is picked when play starts, so its hold counts from here
        world.events.push(makeEvent(EventKind::MatchStart, kNoPlayer, {}, 0.0f));
        break;
    case FlowEvent::MatchEnd: world.events.push(makeEvent(EventKind::MatchEnd, kNoPlayer, {}, 0.0f)); break;
    case FlowEvent::RoundEnd:
    case FlowEvent::None: break;
    }
}

} // namespace

mealkit::sim::MatchRules matchRules(const Tuning& t) {
    mealkit::sim::MatchRules r;
    r.minPlayers = t.minPlayers;
    r.countdown = t.countdown.value;
    r.roundEndDelay = t.roundEndDelay.value;
    r.rounds = t.rounds;
    return r;
}

void startMatch(World& world) {
    handle(world, mealkit::sim::startMatchFlow(world.state.flow, world.rules, world.state.tick, matchPlayers(world)));
}

void updateMatch(World& world) {
    WorldState& s = world.state;
    handle(world, mealkit::sim::advanceMatch(s.flow, world.rules, s.tick, matchPlayers(world)));
    if (s.flow.phase == MatchPhase::Countdown) { // 3, 2, 1
        const u32 left = s.flow.phaseEnd.remaining(s.tick);
        if (left > 0 && left % world.tickRate == 0)
            world.events.push(makeEvent(EventKind::Countdown, kNoPlayer, {}, static_cast<f32>(left / world.tickRate)));
    }
}

void checkRoundEnd(World& world) {
    WorldState& s = world.state;
    if (!matchPlaying(world)) return;
    u32 alive = 0, last = kNoSlot;
    for (u32 slot = 0; slot < s.playerCount; ++slot)
        if (aliveInRound(s.slots[slot])) ++alive, last = slot;
    if (alive > 1) return;
    // Placement: the survivor is 1st, then the latest out. Out order 1 (first out) is last among those who played the round
    for (u32 slot = 0; slot < s.playerCount; ++slot) {
        PlayerSlot& p = s.slots[slot];
        if (!p.connected) continue;
        if (p.outOrder == 0 && p.out) { // joined mid-round: sat it out
            p.lastPlace = 0;
            continue;
        }
        const u32 place = p.outOrder == 0 ? 1u : alive + (s.outCount - p.outOrder) + 1u;
        p.lastPlace = place;
        p.score += world.tuning.placementPoints[(place - 1) % mealkit::net::kMaxPlayers];
    }
    if (last != kNoSlot) {
        s.slots[last].it = false;
        const Pawn* pawn = playerPawn(s, last);
        world.events.push(makeEvent(EventKind::RoundWin, static_cast<u8>(last), pawn ? pawn->position : Vec3{}, 0.0f));
    }
    s.roundWinner = last;
    s.itSlot = kNoSlot;
    handle(world, mealkit::sim::finishRound(s.flow, world.rules, s.tick, mealkit::sim::kNoSide));
}

bool matchPlaying(const World& world) { return world.state.flow.phase == MatchPhase::Playing; }

} // namespace fusetag
