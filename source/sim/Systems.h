#pragma once

#include "sim/World.h"

#include "mealkit/sim/Commands.h"

// sim/ system functions. World::step() calls them directly in a fixed order (World.cpp).
// When systems need to notify each other, they call functions directly. New systems go in their own file and are declared here.
namespace fusetag {

// Players.cpp
void respawnPawns(World& world);      // a new character for players in the round whose respawn wait has ended
void movePawns(World& world);         // input or bot brain → movement mode (velocity, jump, sprint)
void removeFallenPawns(World& world); // below tuning.fallHeight: removed, respawn wait
void removeBlownPawns(World& world);  // out players' characters some time after the blast
void sensePawns(World& world);        // physics results → position, velocity, landing
void respawnAll(World& world);        // round start: everyone back to the spawn points
void setPresence(World& world, const mealkit::sim::Presence& presence); // connect/disconnect mid-match (engine command)

// Hide.cpp
void updateHiding(World& world);       // still in a hide spot → hidden; moving → seen; it standing close → found
void reveal(World& world, u32 slot);   // seen again (moved, shoved, tagged, found)

// Props.cpp
void createProps(World& world, const mealkit::scene::Scene& scene); // Prop objects → dynamic boxes (after the scene, before characters)
void senseProps(World& world);         // physics → prop position/rotation (for the view)
void shovePlayers(World& world);       // shove button: pushes players and props around the shover

// Items.cpp
void createCrates(World& world, const mealkit::scene::Scene& scene); // ItemCrate objects (World::reset)
void updateItems(World& world);   // crates refill and get picked up, items are used, traps catch people
void shoveFrom(World& world, u32 slot, f32 scale); // one shove (Props.cpp; BigShove uses scale > 1)

// Tag.cpp
void tagPlayers(World& world); // it touches a runner → roles swap

// Fuse.cpp
void resetRound(World& world); // before a round: full fuses, nobody out or it
void startRound(World& world); // play starts: a random first it (held for startHold)
void burnFuse(World& world);   // it's fuse burns; at 0 that player is out and a new it is picked
void eliminate(World& world, u32 slot);

// Bots.cpp
mealkit::sim::MoveIntent botIntent(World& world, u32 slot, const Pawn& pawn, u32& pathBudget, bool& sprint);

// Match.cpp
mealkit::sim::MatchRules matchRules(const Tuning& tuning); // tuning → flow rules (World::reset, setTuning)
void startMatch(World& world);         // match start: waiting/countdown (World::reset)
void updateMatch(World& world);        // phase changes (at the start of step, after commands)
void checkRoundEnd(World& world);      // one player left: placement points and the round ends (after eliminations)
bool matchPlaying(const World& world); // in play (input only then)

// Commands.cpp
void applyCommands(World& world, const mealkit::net::TickInputs& inputs); // confirmed commands (at the start of step)

} // namespace fusetag
