// Rule rows (objects module "Rule rows"): RuleZone and RuleTimer scene objects run tables/rules.table rows on the players.
// Effects land in each slot's objects::Effects; sim/Players.cpp reads RuleEffect::Speed as a movement multiplier.
#include "sim/Systems.h"
#include "sim/World.h"

namespace fusetag {

void runRules(World& world) {
    WorldState& s = world.state;
    const u32 count = s.playerCount;
    objects::Effects effects[mealkit::session::kMaxPlayers];
    for (u32 slot = 0; slot < count; ++slot) {
        objects::expire(s.slots[slot].effects, s.tick);
        effects[slot] = s.slots[slot].effects;
    }

    // Players in the round with a character, by their feet (slot order)
    u32 slots[mealkit::session::kMaxPlayers];
    Vec3 feet[mealkit::session::kMaxPlayers];
    u32 inRound = 0;
    u64 roundMask = 0;
    for (u32 slot = 0; slot < count; ++slot) {
        if (!aliveInRound(s.slots[slot])) continue;
        roundMask |= u64{1} << slot;
        const Pawn* pawn = playerPawn(s, slot);
        if (!pawn) continue;
        slots[inRound] = slot;
        feet[inRound] = {pawn->position.x, pawn->position.y - pawn->mover.halfHeight, pawn->position.z};
        ++inRound;
    }

    for (u32 i = 0; i < world.zoneRules.size(); ++i) { // scene order
        const objects::TriggerChange change = objects::update(s.ruleZones[i], slots, feet, inRound);
        const u32 row = world.zoneRules[i];
        objects::runZoneRule(world.ruleRows[row], row, change, effects, count, s.tick);
    }
    for (const World::RuleClock& c : world.ruleTimers)
        objects::runTimerRule(world.ruleRows[c.rule], c.rule, objects::timerEdge(c.timer, s.tick), roundMask, effects, count, s.tick);

    for (u32 slot = 0; slot < count; ++slot) s.slots[slot].effects = effects[slot];
}

void resetRules(World& world) {
    WorldState& s = world.state;
    for (u32 slot = 0; slot < s.playerCount; ++slot) objects::clear(s.slots[slot].effects);
    for (objects::Trigger& t : s.ruleZones) t.inside = 0; // everyone inside counts as entering again
}

} // namespace fusetag
