#include "sim/World.h"

#include "sim/Systems.h"

#include "mealkit/core/Hash.h"
#include "mealkit/core/Log.h"
#include "mealkit/core/Profile.h"

#include <algorithm>
#include <cstring>

namespace fusetag {
namespace {

using mealkit::physics::BodyDesc;
using mealkit::physics::Motion;
namespace scene = mealkit::scene;

// The scene's collision objects (static: box, or the model's convex hull), terrain, spawn points, match area.
// Game object kinds (sim/Objects.h) are read here with scene::readObject<T>(entity).
void loadScene(World& world, const scene::Scene& scene, mealkit::asset::ModelCache& models) {
    bool arena = false;
    for (const scene::Entity& e : scene.entities) {
        if (e.is(scene::kTypeSolid)) {
            BodyDesc d;
            d.motion = Motion::Static;
            d.position = e.position;
            d.rotation = e.rotation;
            d.friction = world.tuning.solidFriction;
            const mealkit::asset::Model* model = e.model.empty() ? nullptr : models.get(e.model);
            if (model) {
                std::vector<Vec3> points;
                points.reserve(model->hullPoints.size());
                for (const Vec3& p : model->hullPoints)
                    points.push_back({p.x * e.scale.x, p.y * e.scale.y, p.z * e.scale.z});
                world.physics.createConvexHull(points.data(), static_cast<u32>(points.size()), d);
            } else {
                world.physics.createBox(e.scale * 0.5f, d);
            }
        } else if (e.is(scene::kTypeTerrain)) { // heightmap (rotation ignored: terrain is axis-aligned)
            const mealkit::asset::Terrain* terrain = models.terrain(scene::terrainFile(e));
            if (!terrain) continue;
            const mealkit::asset::Heightmap& map = terrain->map;
            std::vector<f32> heights;
            mealkit::asset::terrainHeights(map, e.scale.y, heights);
            BodyDesc d;
            d.position = e.position;
            d.friction = world.tuning.solidFriction;
            const f32 cells = static_cast<f32>(map.size - 1);
            world.physics.createHeightField(heights.data(), map.size, e.scale * -0.5f, e.scale.x / cells, e.scale.z / cells, d);
        } else if (e.is(scene::kTypeSpawn)) {
            world.spawns.push_back(e.position);
        } else if (e.is<HideSpot>()) {
            const scene::Bounds b = scene::areaBounds(e);
            world.hideSpots.push_back({b.min, b.max, scene::readObject<HideSpot>(e).kind});
        } else if (e.is<MatchArea>() && !arena) {
            const scene::Bounds b = scene::areaBounds(e);
            world.arenaMin = b.min;
            world.arenaMax = b.max;
            arena = true;
        }
    }
    if (!arena) {
        MEALKIT_WARN("Scene has no MatchArea - using a default 60 m area");
        world.arenaMin = {-30.0f, -10.0f, -30.0f}; // const-ok: safe value for scenes without an area
        world.arenaMax = {30.0f, 20.0f, 30.0f};    // const-ok: safe value for scenes without an area
    }
    world.physics.optimize();
}

} // namespace

void World::reset(u64 seed, u32 humans, const Tuning& tuningValue, const scene::Scene& scene, mealkit::asset::ModelCache& models,
                  u32 tickRateValue, u32 physicsThreads) {
    physics.init(4096, physicsThreads);
    tickRate = tickRateValue ? tickRateValue : 30u;
    tickDt = 1.0f / static_cast<f32>(tickRate);
    tuning = tuningValue;
    rules = matchRules(tuning);
    physics.setGravity(tuning.gravity);
    physics.setContactEvents(tuning.impactSpeed, tuning.impactsPerTick);
    spawns.clear();
    hideSpots.clear();
    state = WorldState{};
    events.clear();

    // Fixed body creation order: scene → characters (slot order)
    loadScene(*this, scene, models);
    nav = mealkit::sim::bakeNavGrid(physics, arenaMin, arenaMax, tuning.nav); // only static scene bodies exist now
    createProps(*this, scene);
    state.humanCount = std::min(humans, mealkit::net::kMaxPlayers);
    state.playerCount = std::max(state.humanCount, std::min(tuning.botFill, mealkit::net::kMaxPlayers));
    for (u32 i = 0; i < state.playerCount; ++i) {
        state.slots[i].connected = true;
        state.slots[i].bot = i >= state.humanCount;
    }
    state.rng = mealkit::Pcg32(seed ^ 0x9E3779B97F4A7C15ull, 11); // const-ok: stream constants
    state.pawns.init(mealkit::net::kMaxPlayers * 2); // a removed character and its replacement can coexist within one tick
    resetRound(*this);
    respawnPawns(*this);
    state.pawns.endTick();
    sensePawns(*this);
    startMatch(*this);
    events.clear(); // spawn events of the first characters aren't sent
}

void World::setTuning(const Tuning& value) {
    tuning = value;
    rules = matchRules(tuning);
    physics.setGravity(tuning.gravity);
}

void World::step(const mealkit::net::TickInputs& tickInputs) {
    MEALKIT_ZONE("World::step");
    for (u32 i = 0; i < mealkit::net::kMaxPlayers; ++i) inputs[i] = GameInput{};
    for (u32 i = 0; i < state.humanCount; ++i) std::memcpy(&inputs[i], tickInputs.players[i].bytes, sizeof(GameInput));
    events.clear();

    applyCommands(*this, tickInputs); // confirmed commands: player index → send order
    updateMatch(*this);               // waiting → countdown → playing → round end → results
    respawnPawns(*this);
    movePawns(*this);
    physics.step(tickDt, 2); // splits a tick into 2 physics substeps for stability
    for (const mealkit::physics::ContactEvent& c : physics.contacts())
        events.push(makeEvent(EventKind::Impact, kNoPlayer, c.point, c.speed));
    removeFallenPawns(*this);
    sensePawns(*this);
    senseProps(*this);
    shovePlayers(*this);
    updateHiding(*this);
    tagPlayers(*this);
    burnFuse(*this);
    checkRoundEnd(*this);
    removeBlownPawns(*this);
    // End of tick: delete bodies of characters removed this tick, and put ones created this tick into iteration from the next tick
    state.pawns.endTick([this](Pawn& pawn) { mealkit::sim::destroyMover(physics, pawn.mover); });
    state.props.endTick([this](PropBox& prop) { physics.destroy(prop.body); });
    ++state.tick;
}

u64 World::hash() const {
    mealkit::Hasher h;
    mealkit::reflect::hashStruct(mealkit::reflect::typeOf<WorldState>(), &state, h); // all declared state fields
    physics.hashState(h);
    return h.value;
}

bool aliveInRound(const PlayerSlot& slot) { return slot.connected && !slot.out; }

f32 fuseFraction(const World& world, u32 slot) {
    const u32 length = world.tuning.fuseLength.value;
    return length ? static_cast<f32>(world.state.slots[slot].fuse) / static_cast<f32>(length) : 0.0f;
}

u32 hideSpotAt(const World& world, const Vec3& feet) {
    constexpr f32 kAbove = 0.3f; // const-ok: feet may sit a little above the area bottom (floor contact)
    for (u32 i = 0; i < world.hideSpots.size(); ++i) {
        const World::HideArea& a = world.hideSpots[i];
        if (feet.x >= a.min.x && feet.x <= a.max.x && feet.z >= a.min.z && feet.z <= a.max.z && feet.y >= a.min.y - kAbove &&
            feet.y <= a.max.y)
            return i;
    }
    return kNoSlot;
}

bool botDriven(const World& world, u32 slot) {
    return world.state.slots[slot].bot || (world.inputs[slot].buttons & kButtonAuto) != 0;
}

} // namespace fusetag
