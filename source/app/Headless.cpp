#include "app/Headless.h"

#include "mealkit/data/Tables.h"

#include <cstring>

using namespace mealkit;

namespace fusetag {

void Headless::startWorld(World& world, const game::MatchInfo& match, const Tuning& tuning, const scene::Scene& flatScene,
                          asset::ModelCache& models, u32 physicsThreads) {
    // Tables are read from files every match (per-match loading, engine docs/CODING_MODEL.md "Asset references and loading")
    sim::Table<ItemKind> items;
    data::loadTable(items);
    world.reset(match.seed, match.playerCount, tuning, items, flatScene, models, reflect::tickRate(), physicsThreads);
}

// The bot AI in the simulation drives this player (sim/Bots.cpp), so --bot plays like the bots in empty seats
net::PlayerInput Headless::botInput(u32 tick, u8 player) {
    (void)tick;
    (void)player;
    GameInput g;
    g.buttons = kButtonAuto;
    net::PlayerInput pi;
    std::memcpy(pi.bytes, &g, sizeof(g));
    return pi;
}

void Headless::defaults(game::MatchOptions& options, const ProjectSettings& project) {
    if (options.port == 0) options.port = project.defaultPort;
    if (options.maxPlayers == 0) options.maxPlayers = project.defaultMaxPlayers;
}

} // namespace fusetag

MEALKIT_HEADLESS_GAME(fusetag::Headless)
