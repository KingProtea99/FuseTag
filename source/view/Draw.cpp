#include "view/Draw.h"

#include "mealkit/render/SceneLights.h"

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
        if (e.is<Prop>() || e.is<ItemCrate>()) continue; // drawn from the simulation (drawProps, drawItems)
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

void pawnPositions(const WorldState& previous, const WorldState& current, f32 alpha, std::array<Vec3, net::kMaxPlayers>& at,
                   std::array<bool, net::kMaxPlayers>& has) {
    has = {};
    for (auto [id, pawn] : current.pawns) {
        if (pawn.player >= net::kMaxPlayers) continue;
        const Pawn* before = previous.pawns.get(id); // nullptr when it appeared this tick
        at[pawn.player] = lerp(before ? before->position : pawn.position, pawn.position, alpha);
        has[pawn.player] = true;
    }
}

void drawHideSpots(const World& world, const ViewTuning& look, const Juice& juice, asset::ModelCache& models, render::DrawList& out) {
    const render::MeshId box = models.boxMesh();
    for (u32 i = 0; i < world.hideSpots.size(); ++i) {
        const World::HideArea& a = world.hideSpots[i];
        const Vec3 size = a.max - a.min;
        Vec3 wobble;
        Quat tilt;
        juice.spotLook(i, wobble, tilt);
        const Vec3 drawn{size.x * wobble.x, size.y * wobble.y, size.z * wobble.z};
        const Vec3 center{(a.min.x + a.max.x) * 0.5f, a.min.y + drawn.y * 0.5f, (a.min.z + a.max.z) * 0.5f};
        const Color c = a.kind == HideKind::Bush ? look.juice.bushColor : a.kind == HideKind::Locker ? look.juice.lockerColor : look.juice.pileColor;
        out.add(box, {center, c.value, tilt, drawn, 0});
    }
}

void drawItems(const WorldState& current, const ViewTuning& look, f64 viewTime, asset::ModelCache& models, render::DrawList& out) {
    const render::MeshId box = models.boxMesh();
    const f32 time = static_cast<f32>(viewTime);
    for (auto [id, crate] : current.crates) {
        if (!crate.full) continue;
        const f32 phase = time + static_cast<f32>(id.slot) * 0.7f; // const-ok: crates out of step
        const Vec3 at = crate.position + Vec3{0.0f, look.juice.crateBob * std::sin(phase * 2.0f), 0.0f}; // const-ok: bob rate
        const Quat rot = quatFromAxisAngle({0, 1, 0}, phase * look.juice.crateSpin) * quatFromAxisAngle(normalize(Vec3{1, 0, 1}), 0.4f); // const-ok: tilted
        const f32 glint = std::sin(phase * 3.0f) > 0.95f ? 0.6f : 0.0f; // const-ok: a short glint now and then
        const f32 sz = look.juice.crateSize;
        out.add(box, {at, look.juice.crateColor.value, rot, Vec3{sz, sz, sz}, render::instanceFlash(0, glint)});
    }
    for (auto [id, trap] : current.traps) {
        const bool armed = trap.armed.ready(current.tick);
        out.add(box, {trap.position + Vec3{0.0f, look.juice.trapSize.y * 0.5f, 0.0f}, look.juice.trapColor.value, Quat{},
                      look.juice.trapSize * (armed ? 1.0f : 0.6f), 0}); // const-ok: smaller until armed
    }
}

void drawProps(const WorldState& previous, const WorldState& current, f32 alpha, asset::ModelCache& models, render::DrawList& out) {
    const render::MeshId box = models.boxMesh();
    for (auto [id, prop] : current.props) {
        const PropBox* before = previous.props.get(id);
        const Vec3 at = before ? lerp(before->position, prop.position, alpha) : prop.position;
        const Quat rot = before ? nlerp(before->rotation, prop.rotation, alpha) : prop.rotation;
        out.add(box, {at, prop.color, rot, prop.halfExtents * 2.0f, 0});
    }
}

void drawWorld(const WorldState& current, const std::array<Vec3, net::kMaxPlayers>& at, const std::array<bool, net::kMaxPlayers>& has,
               u32 local, const ViewTuning& look, const Juice& juice, asset::ModelCache& models, render::DrawList& out) {
    const render::MeshId box = models.boxMesh();
    for (u32 slot = 0; slot < net::kMaxPlayers; ++slot) {
        if (!has[slot]) continue;
        const PlayerSlot& p = current.slots[slot];
        const Color color = p.out ? look.outColor : look.playerColors[slot % look.playerColors.size()];
        const bool scoping = local < current.playerCount && !current.slots[local].scope.ready(current.tick);
        const bool hiddenNow = unseen(current, slot);
        if (hiddenNow && slot != local && !scoping) continue; // others don't see a hidden or invisible runner
        PawnLook l = juice.look(slot);
        if (hiddenNow && slot != local) l.flash = std::max(l.flash, look.juice.scopeFlash); // seen through the scope
        if (hiddenNow && slot == local) { // my own hidden character: smaller and pale so I know I'm hidden
            l.scale = l.scale * look.juice.hiddenSelfScale;
            l.flash = std::max(l.flash, look.juice.hiddenSelfFlash);
        }
        const Vec3 size{look.pawnSize.x * l.scale.x, look.pawnSize.y * l.scale.y, look.pawnSize.z * l.scale.z};
        // Squash about the feet: the box center moves down as it squashes
        const Vec3 center = at[slot] + l.offset + Vec3{0.0f, (size.y - look.pawnSize.y) * 0.5f, 0.0f};
        out.add(box, {center, color.value, l.rotation, size, render::instanceFlash(0, l.flash)});
    }
    // Afterimages: smaller, fading copies behind the one who passed the bomb on
    static thread_local std::vector<Juice::Ghost> ghosts;
    ghosts.clear();
    juice.ghosts(ghosts);
    for (const Juice::Ghost& g : ghosts) {
        const Color c = look.playerColors[g.slot % look.playerColors.size()];
        const f32 k = 0.4f + 0.5f * g.fade; // const-ok: oldest is 40% size
        out.add(box, {g.position, c.value, Quat{}, look.pawnSize * k, render::instanceFlash(0, 0.5f * (1.0f - g.fade))});
    }
    // "It!" marker and the hunter arrow
    Vec3 markAt;
    f32 markScale = 0.0f;
    Quat markRot;
    if (juice.itMarker(markAt, markScale, markRot))
        out.add(box, {markAt, look.juice.markerColor.value, markRot, Vec3{markScale, markScale, markScale * 0.4f}, render::instanceFlash(0, 0.3f)});
    Vec3 arrowAt;
    Quat arrowRot;
    f32 near = 0.0f;
    if (juice.hunterArrow(arrowAt, arrowRot, near) && current.itSlot != kNoSlot) {
        const Color c = look.playerColors[current.itSlot % look.playerColors.size()];
        out.add(box, {arrowAt, c.value, arrowRot, look.juice.arrowSize * (0.5f + 0.5f * near), render::instanceFlash(0, near * 0.5f)});
    }
    Vec3 bombAt;
    f32 bombScale = 0.0f, bombFlash = 0.0f;
    if (juice.bomb(bombAt, bombScale, bombFlash))
        out.add(box, {bombAt, look.juice.bombColor.value, Quat{}, Vec3{bombScale, bombScale, bombScale}, render::instanceFlash(0, bombFlash)});
}

} // namespace fusetag
