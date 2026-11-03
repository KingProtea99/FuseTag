#include "view/Draw.h"

#include "mealkit/render/SceneLights.h"

#include <algorithm>
#include <cmath>

using namespace mealkit;

namespace fusetag {
namespace {

// A static model, or a box when the model is missing (first run before the art exists, or a bad path)
void addModelOrBox(asset::ModelCache& models, const std::string& file, const Vec3& eye, const render::InstanceData& instance,
                   render::DrawList& out) {
    const asset::Model* model = file.empty() ? nullptr : models.get(file);
    if (!model || model->skinned()) {
        out.add(models.boxMesh(), instance);
        return;
    }
    asset::addModel(out, *model, models.pickLod(*model, eye, instance.position, instance.scale), instance);
}

} // namespace

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

void drawHideSpots(const World& world, const ViewTuning& look, const Juice& juice, const Vec3& eye, asset::ModelCache& models,
                   render::DrawList& out) {
    for (u32 i = 0; i < world.hideSpots.size(); ++i) {
        const World::HideArea& a = world.hideSpots[i];
        const Vec3 size = a.max - a.min;
        Vec3 wobble;
        Quat tilt;
        juice.spotLook(i, wobble, tilt);
        const Vec3 drawn{size.x * wobble.x, size.y * wobble.y, size.z * wobble.z};
        const Vec3 floor{(a.min.x + a.max.x) * 0.5f, a.min.y, (a.min.z + a.max.z) * 0.5f};
        const std::string& file = a.kind == HideKind::Bush ? look.bushModel : a.kind == HideKind::Locker ? look.lockerModel : look.pileModel;
        const asset::Model* model = file.empty() ? nullptr : models.get(file);
        if (!model) { // no art: a tinted box of the area
            const Color c = a.kind == HideKind::Bush ? look.juice.bushColor : a.kind == HideKind::Locker ? look.juice.lockerColor : look.juice.pileColor;
            out.add(models.boxMesh(), {floor + Vec3{0.0f, drawn.y * 0.5f, 0.0f}, c.value, tilt, drawn, 0});
            continue;
        }
        // Models are about 1 m wide with their origin on the floor: scale the footprint and height to the area
        const Vec3 extent = model->boundsMax - model->boundsMin;
        const Vec3 scale{drawn.x / std::max(extent.x, 0.01f), drawn.y / std::max(extent.y, 0.01f), drawn.z / std::max(extent.z, 0.01f)};
        addModelOrBox(models, file, eye, {floor - Vec3{0.0f, model->boundsMin.y * scale.y, 0.0f}, 0xFFFFFFFFu, tilt, scale, 0}, out);
    }
}

void drawItems(const WorldState& current, const ViewTuning& look, f64 viewTime, const Vec3& eye, asset::ModelCache& models,
               render::DrawList& out) {
    const render::MeshId box = models.boxMesh();
    const f32 time = static_cast<f32>(viewTime);
    for (auto [id, crate] : current.crates) {
        if (!crate.full) continue;
        const f32 phase = time + static_cast<f32>(id.slot) * 0.7f; // const-ok: crates out of step
        const Quat rot = quatFromAxisAngle({0, 1, 0}, phase * look.juice.crateSpin) * quatFromAxisAngle(normalize(Vec3{1, 0, 1}), 0.4f); // const-ok: tilted
        const f32 glint = std::sin(phase * 3.0f) > 0.95f ? 0.6f : 0.0f; // const-ok: a short glint now and then
        const f32 sz = look.juice.crateSize;
        // The tilted crate reaches lower than an upright one: lift it so its lowest corner stays at the bottom of an upright crate
        // at the placed position, and bob upward only (otherwise the corners dip into the floor)
        const f32 reach = 0.5f * sz * (std::fabs(rotate(rot, {1, 0, 0}).y) + std::fabs(rotate(rot, {0, 1, 0}).y) + std::fabs(rotate(rot, {0, 0, 1}).y));
        const f32 bob = look.juice.crateBob * (0.5f + 0.5f * std::sin(phase * 2.0f)); // const-ok: bob rate
        const Vec3 at = crate.position + Vec3{0.0f, reach - 0.5f * sz + bob, 0.0f};
        addModelOrBox(models, look.crateModel, eye, {at, look.juice.crateColor.value, rot, Vec3{sz, sz, sz}, render::instanceFlash(0, glint)}, out);
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
               u32 local, const ViewTuning& look, const Juice& juice, CharacterView& chars, f64 viewTime, asset::ModelCache& models,
               render::DrawList& out) {
    const render::MeshId box = models.boxMesh();
    const asset::Model* figure = look.pawnModel.empty() ? nullptr : models.get(look.pawnModel);
    if (figure && !figure->skinned()) figure = nullptr;
    const i32 set = figure ? chars.states.findSet(look.animSet) : -1;
    // Animation advances by view time (stops during hitstop like the rest of the view)
    const f32 dt = chars.time < 0.0 ? 0.0f : static_cast<f32>(std::clamp(viewTime - chars.time, 0.0, 0.1));
    chars.time = viewTime;
    chars.events.clear();
    thread_local std::vector<std::string> fired;
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
        if (!figure) { // no art: a box, squashed about the feet
            const Vec3 size{look.pawnSize.x * l.scale.x, look.pawnSize.y * l.scale.y, look.pawnSize.z * l.scale.z};
            const Vec3 center = at[slot] + l.offset + Vec3{0.0f, (size.y - look.pawnSize.y) * 0.5f, 0.0f};
            out.add(box, {center, color.value, l.rotation, size, render::instanceFlash(0, l.flash)});
            continue;
        }
        // Face the way the character moves (from this frame's interpolated motion), turning smoothly
        f32& yaw = chars.yaw[slot];
        const Vec3 move = juice.velocity(slot);
        const f32 speed = std::sqrt(move.x * move.x + move.z * move.z);
        if (!chars.seen[slot]) chars.anims[slot] = {};
        chars.seen[slot] = true;
        if (speed > 0.5f) { // const-ok: below this the character keeps facing where it was going
            const f32 want = std::atan2(move.x, move.z);
            f32 d = want - yaw;
            while (d > 3.14159265f) d -= 6.28318531f;
            while (d < -3.14159265f) d += 6.28318531f;
            yaw += d * std::min(1.0f, look.turnSpeed * dt);
        }
        asset::AnimParams& params = chars.params[slot];
        params.set("speed", speed);
        params.set("air", std::fabs(move.y) > 1.5f); // const-ok: rising or falling fast
        params.set("fuming", p.it && !p.hold.ready(current.tick));
        params.set("hiding", p.hidden);
        params.set("cheer", current.flow.phase == MatchPhase::RoundEnd && current.roundWinner == slot);
        if (juice.shoved(slot)) params.trigger("shove");
        fired.clear();
        asset::updateAnim(chars.states, set, figure->clips, chars.anims[slot], params, dt, &fired);
        for (std::string& e : fired) chars.events.emplace_back(std::move(e), at[slot]);
        thread_local std::vector<asset::JointPose> pose;
        thread_local std::vector<asset::BoneMatrix> palette;
        asset::animPose(chars.states, figure->skeleton, figure->clips, chars.anims[slot], pose);
        asset::skinPalette(figure->skeleton, pose, palette);
        // The model faces glTF +Z, which the importer's z flip (glTF is right-handed) turns into -Z: turn it half way round
        const Quat facing = quatFromAxisAngle({0, 1, 0}, yaw + 3.14159265f);
        const Vec3 feet = at[slot] + l.offset - Vec3{0.0f, look.footOffset, 0.0f};
        out.addSkinned(figure->lods[0], {feet, color.value, normalize(facing * l.rotation), l.scale, render::instanceFlash(0, l.flash)},
                       palette.data(), static_cast<u32>(palette.size()));
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
    if (juice.bomb(bombAt, bombScale, bombFlash)) {
        const bool model = !look.bombModel.empty() && models.get(look.bombModel);
        const u32 tint = model ? 0xFFFFFFFFu : look.juice.bombColor.value; // the model has its own colors
        addModelOrBox(models, look.bombModel, bombAt, {bombAt, tint, Quat{}, Vec3{bombScale, bombScale, bombScale}, render::instanceFlash(0, bombFlash)},
                      out);
    }
}

} // namespace fusetag
