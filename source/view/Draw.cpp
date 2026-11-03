#include "view/Draw.h"

#include "mealkit/render/SceneLights.h"

#include <cmath>

using namespace mealkit;

namespace fusetag {

void applyEnvironment(const scene::Environment& env, asset::ModelCache& models, render::FrameParams& frame) {
    frame.sunDirection = env.sunDirection;
    frame.sunColor = env.sunColor;
    frame.skyColor = env.skyColor;
    frame.groundColor = env.groundColor;
    frame.fogColor = env.fogColor;
    frame.fogDensity = env.fogDensity;
    frame.environment = models.environment(env.hdri);
    frame.environmentIntensity = env.hdriIntensity;
    frame.environmentRotation = env.hdriRotation * (3.14159265f / 180.0f);
    frame.post = models.postParams(env.post);
}

void applyLights(const scene::Scene& scene, std::vector<render::PointLight>& lights, render::FrameParams& frame) {
    lights.clear();
    render::addSceneLights(scene, lights);
    frame.lights = lights.data();
    frame.lightCount = static_cast<u32>(lights.size());
}

void applyProbes(const scene::Scene& scene, asset::ModelCache& models, std::vector<render::ReflectionProbe>& probes,
                 render::FrameParams& frame) {
    models.probes(scene, probes);
    frame.probes = probes.data();
    frame.probeCount = static_cast<u32>(probes.size());
}

void drawScene(const scene::Scene& scene, asset::ModelCache& models, const Vec3& eye, f64 viewTime, render::DrawList& out) {
    const render::MeshId box = models.boxMesh();
    for (const scene::Entity& e : scene.entities) {
        if (!scene::visibleInGame(e, &reflect::moduleObjectTypes())) continue; // spawn points/areas are invisible in the game
        if (e.is(scene::kTypeScatter)) { // view-only instances painted in the editor
            models.addScatter(out, e, eye);
            continue;
        }
        if (e.is(scene::kTypeTerrain)) {
            if (asset::Terrain* terrain = models.terrain(scene::terrainFile(e)))
                models.addTerrain(out, *terrain, e.position, e.scale, e.color, e.flags, eye);
            continue;
        }
        const asset::Model* model = e.model.empty() ? nullptr : models.get(e.model);
        const render::MeshId mesh = model ? models.pickLod(*model, eye, e.position, e.scale) : box;
        if (model) asset::addModel(out, *model, mesh, {e.position, e.color, e.rotation, e.scale, e.flags}, scene::animField(e), viewTime);
        else out.add(mesh, {e.position, e.color, e.rotation, e.scale, e.flags});
    }
}

void drawWorld(const World& world, const WorldState& previous, f32 alpha, const ViewTuning& look, f64 viewTime,
               asset::ModelCache& models, render::DrawList& out) {
    const WorldState& current = world.state;
    const render::MeshId box = models.boxMesh();
    for (auto [id, pawn] : current.pawns) {
        const Pawn* before = previous.pawns.get(id); // nullptr when it appeared this tick
        const Vec3 at = lerp(before ? before->position : pawn.position, pawn.position, alpha);
        const PlayerSlot& slot = current.slots[pawn.player];
        const Color color = slot.out ? look.outColor : look.playerColors[pawn.player % look.playerColors.size()];
        out.add(box, {at, color.value, Quat{}, look.pawnSize, 0});
        if (!slot.it) continue;
        // The bomb pulses faster as the fuse burns down (placeholder for the M2 bomb)
        const f32 left = fuseFraction(world, pawn.player);
        const f32 rate = look.bombPulseSlow + (look.bombPulseFast - look.bombPulseSlow) * (1.0f - left);
        const f32 pulse = 1.0f + look.bombPulseSize * std::sin(static_cast<f32>(viewTime) * rate * 6.2831853f);
        const Vec3 above = at + Vec3{0.0f, look.pawnSize.y * 0.5f + look.bombHeight, 0.0f};
        const u32 flash = left < look.bombFlashBelow && std::sin(static_cast<f32>(viewTime) * rate * 6.2831853f) > 0.0f
                              ? render::instanceFlash(0, 0.8f)
                              : 0u;
        out.add(box, {above, look.bombColor.value, Quat{}, Vec3{look.bombSize, look.bombSize, look.bombSize} * pulse, flash});
    }
}

} // namespace fusetag
