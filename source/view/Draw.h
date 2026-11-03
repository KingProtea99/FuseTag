#pragma once

#include "sim/World.h"
#include "view/Juice.h"
#include "view/ViewTuning.h"

#include "mealkit/asset/AnimStates.h"
#include "mealkit/asset/ModelCache.h"
#include "mealkit/render/DrawList.h"
#include "mealkit/render/Renderer.h"
#include "mealkit/scene/Scene.h"

#include <array>
#include <vector>

// Scene drawing. Simulation state is only read.
namespace fusetag {

// Scene environment → view (sun, sky, fog, HDRI, post effects)
void applyEnvironment(const mealkit::scene::Environment& env, mealkit::asset::ModelCache& models, mealkit::render::FrameParams& frame);
// Scene point lights and reflection probes → view (frame points at the vectors: they must live for the frame)
void applyLights(const mealkit::scene::Scene& scene, std::vector<mealkit::render::PointLight>& lights, mealkit::render::FrameParams& frame);
void applyProbes(const mealkit::scene::Scene& scene, mealkit::asset::ModelCache& models,
                 std::vector<mealkit::render::ReflectionProbe>& probes, mealkit::render::FrameParams& frame);
// Static scene objects (spawn points and areas aren't drawn)
void drawScene(const mealkit::scene::Scene& scene, mealkit::asset::ModelCache& models, const Vec3& eye, mealkit::f64 viewTime,
               mealkit::render::DrawList& out);
// Character animation state kept between frames (skinned model + tables/animation.table)
struct CharacterView {
    mealkit::asset::AnimStates states; // loaded by GameSession, reloaded when the file changes
    std::array<mealkit::asset::AnimInstance, mealkit::net::kMaxPlayers> anims{};
    std::array<mealkit::asset::AnimParams, mealkit::net::kMaxPlayers> params{};
    std::array<f32, mealkit::net::kMaxPlayers> yaw{};
    std::array<bool, mealkit::net::kMaxPlayers> seen{};
    mealkit::f64 time = -1.0; // view time of the last update
    std::vector<std::pair<std::string, Vec3>> events; // animation events (footsteps) for ctx.feedback
};

// Interpolated character centers by player slot (has = the slot has a character)
void pawnPositions(const WorldState& previous, const WorldState& current, f32 alpha, std::array<Vec3, mealkit::net::kMaxPlayers>& at,
                   std::array<bool, mealkit::net::kMaxPlayers>& has);
// Hide spots (from World::hideSpots, with juice wobble) and loose props (interpolated)
void drawHideSpots(const World& world, const ViewTuning& look, const Juice& juice, const Vec3& eye, mealkit::asset::ModelCache& models,
                   mealkit::render::DrawList& out);
// Item crates (bobbing, spinning) and snares on the floor
void drawItems(const WorldState& current, const ViewTuning& look, mealkit::f64 viewTime, const Vec3& eye, mealkit::asset::ModelCache& models,
               mealkit::render::DrawList& out);
void drawProps(const WorldState& previous, const WorldState& current, f32 alpha, mealkit::asset::ModelCache& models,
               mealkit::render::DrawList& out);
// Characters (with juice's squash, lean and flash) and the bomb. Hidden runners are drawn only for their own player (local)
void drawWorld(const WorldState& current, const std::array<Vec3, mealkit::net::kMaxPlayers>& at,
               const std::array<bool, mealkit::net::kMaxPlayers>& has, u32 local, const ViewTuning& look, const Juice& juice,
               CharacterView& chars, mealkit::f64 viewTime, mealkit::asset::ModelCache& models, mealkit::render::DrawList& out);

} // namespace fusetag
