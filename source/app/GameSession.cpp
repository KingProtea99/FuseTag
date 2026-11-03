#include "app/GameSession.h"
#include "app/Headless.h"
#include "sim/Commands.h"

#include "mealkit/sim/Commands.h"

#include "mealkit/audio/Audio.h"
#include "mealkit/feedback/Feedback.h"
#include "mealkit/game/GameData.h"
#include "mealkit/game/Save.h"
#include "mealkit/platform/Window.h"
#include "mealkit/settings/Settings.h"
#include "mealkit/ui/Ui.h"

#include <algorithm>
#include <cstring>

#if MEALKIT_DEBUG_UI
#include <imgui.h>
#endif

using namespace mealkit;

namespace fusetag {

void GameSession::loadSave(const std::string& userFolder) {
    if (m_saveLoaded) return;
    game::readSave(m_save, userFolder, "progress", kSaveVersion);
    m_saveLoaded = true;
}

// Before the match: match start data (mealkit/game/Loadout.h) goes here, e.g. game::setLoadout(session, loadout) from a lobby choice
void GameSession::lobby(net::LockstepSession& session, const game::PlayOptions& options, const std::string& userFolder) {
    (void)session;
    (void)options;
    loadSave(userFolder);
}

void GameSession::start(game::GameContext& ctx, const scene::Scene& scene, const game::PlayOptions& options) {
    m_scene = scene::flattened(scene); // flattens the parent/child hierarchy to world coordinates
    m_sceneSource = &scene;
    m_options = options;
    Tuning tuning;
    game::loadTuning(reflect::typeOf<Tuning>(), &tuning, options.tuningPath);
    const game::MatchInfo match = game::matchInfo(ctx.session); // seed, player count, match data, per-player loadouts
    sim().start(options, game::tickDt(), [&](World& world, u32 physicsThreads) {
        Headless::startWorld(world, match, tuning, m_scene, ctx.models, physicsThreads); // same match start as headless
    });
    m_camera.load(m_scene, ctx.viewTuning<ViewTuning>().camera);
    m_input.clearLatches();
    m_juice.reset();
    m_chars = CharacterView{};
    m_chars.states.load(); // assets/tables/animation.table
    m_frozen = false;
    m_hudVm = {};
    loadSave(ctx.userFolder);
    ++m_save.matchesStarted;
    game::writeSave(m_save, ctx.userFolder, "progress", kSaveVersion);
    ctx.audio.setGlobalParam("tension", 0.0f);
    ctx.audio.playMusic("chase"); // assets/sounds.events: bass first, drums and lead come in with the tension parameter
    if (!ctx.ui.isOpen(m_hud)) m_hud = ctx.ui.open("ui/hud.ui");
    ctx.ui.bind(m_hud, m_hudVm);
}

// Match end (back to lobby, left, disconnected): discard the World and close match screens
void GameSession::stop(game::GameContext& ctx) {
    sim().stop();
    ctx.audio.stopMusic(1.0f);
    if (m_hud) ctx.ui.close(m_hud);
    m_hud = 0;
    m_flow.stop(ctx);
    game::writeSave(m_save, ctx.userFolder, "progress", kSaveVersion);
}

void GameSession::update(game::GameContext& ctx, f64 dt) {
    net::LockstepSession& session = ctx.session;
    const ViewTuning& look = ctx.viewTuning<ViewTuning>();
    loadSave(ctx.userFolder);
    // Screen flow: opens screens matching the session phase and handles buttons (menu/lobby/pause/results/chat)
    FlowGame flow;
    flow.playerColors = look.playerColors;
    flow.resultsSeconds = look.resultsSeconds;
    flow.chatShowSeconds = look.chatShowSeconds;
    if (sim().started()) {
        const WorldState& s = sim().world().state;
        flow.inMatch = true;
        flow.results = s.flow.phase == MatchPhase::Results;
        flow.playerCount = s.playerCount;
        for (u32 i = 0; i < s.playerCount; ++i) {
            flow.score[i] = s.slots[i].score;
            flow.bots[i] = s.slots[i].bot;
        }
    }
    m_flow.update(ctx, flow, m_save, m_hudVm, m_hud, dt);
    if (m_flow.takeSaveChanged()) game::writeSave(m_save, ctx.userFolder, "progress", kSaveVersion);
    if (!sim().started()) {
        if (ctx.scene && ctx.scene != m_sceneSource) { // the scene shown behind the menu
            m_scene = scene::flattened(*ctx.scene);
            m_sceneSource = ctx.scene;
        }
        return;
    }

    if (!m_flow.blocksInput()) { // during pause/chat/results: the character stands still and the camera stays
        m_camera.control(ctx.window, ctx.actions, look.camera);
        m_input.poll(ctx.actions);
        if (ctx.actions.pressed("menu.settings")) ctx.settings.open(); // standard settings screen
    } else
        m_camera.release(ctx.window);
    if (!ctx.feedback.hitStop()) m_viewTime += dt;
    m_chars.states.poll(dt); // edits to animation.table apply while playing // hitstop freezes only the view (the simulation continues)

    // Every confirmed tick: build and send local input, and pass tick events to the feedback table (sound, shake, flash)
    auto sample = [&](u32 tick) {
        GameInput gi = m_input.sample(ctx.actions, m_camera);
        if (m_flow.blocksInput()) gi = GameInput{};
        m_input.clearLatches();
        if (m_options.bot) return Headless::botInput(tick, session.localPlayer());
        net::PlayerInput pi;
        std::memcpy(pi.bytes, &gi, sizeof(gi));
        return pi;
    };
    auto onTick = [&](const World& world) {
        const u8 local = static_cast<u8>(session.localPlayer());
        for (const GameEvent& e : world.events) {
            ctx.feedback.trigger(e, local);
            if (e.kind == EventKind::Tag && e.other == local) ctx.feedback.play("TagVictim", e.position); // rows match only "player"
            m_juice.onEvent(e, world, local, m_viewTime);
        }
    };
    if (!m_flow.pausesSimulation()) sim().update(session, dt, sample, onTick); // pause in solo play halts the match
    SeatNames names;
    for (u32 i = 0; i < sim().world().state.playerCount; ++i) names[i] = Flow::seatName(ctx, flow, i);
    fillHud(sim().world(), session.localPlayer(), names, look.playerColors, ctx.strings, m_hudVm);
}

void GameSession::buildFrame(game::GameContext& ctx, f64 now, render::FrameParams& frame, render::DrawList& out) {
    const ViewTuning& look = ctx.viewTuning<ViewTuning>();
    if (!sim().started()) { // behind the menu/lobby: slowly circles the scene
        camera::Brain& menu = m_menuCamera.brain;
        menu.setFallback(look.camera.menu);
        camera::BrainInput in;
        in.dt = static_cast<f32>(std::clamp(now - m_menuTime, 0.0, 0.1));
        in.defaultFov = ctx.settings.values().display.fov;
        m_menuTime = now;
        menu.update(in);
        menu.apply(ctx.viewport, look.camera.nearPlane, look.camera.farPlane, frame);
    } else {
        // When hitstop starts, hold the picture of that moment, and return to the current state when it ends
        const bool hitStop = ctx.feedback.hitStop();
        if (hitStop && !m_frozen) {
            m_frozenPrevious = sim().previous();
            m_frozenCurrent = sim().world().state;
            m_frozenAlpha = sim().alpha();
        }
        m_frozen = hitStop;
        const WorldState& previous = m_frozen ? m_frozenPrevious : sim().previous();
        const WorldState& current = m_frozen ? m_frozenCurrent : sim().world().state;
        pawnPositions(previous, current, m_frozen ? m_frozenAlpha : sim().alpha(), m_pawnAt, m_pawnHas);
        m_juice.update(sim().world(), m_pawnAt, m_pawnHas, static_cast<f32>(m_viewTime - m_juiceTime), m_viewTime, look.juice);
        m_juiceTime = m_viewTime;
        for (const Vec3& p : m_juice.sparks) ctx.feedback.playEffect("spark", p);
        for (const auto& [at, fuse] : m_juice.ticks) ctx.audio.play("bombTick", at, {{"fuse", fuse}});
        if (m_juice.heartbeat) ctx.audio.play2D("heartbeat");
        // Music tension: the stems come in as the it player's fuse burns down
        const WorldState& ws = sim().world().state;
        const f32 tension = ws.flow.phase == MatchPhase::Playing && ws.itSlot != kNoSlot ? 1.0f - fuseFraction(sim().world(), ws.itSlot) : 0.0f;
        ctx.audio.setGlobalParam("tension", 0.2f + 0.8f * tension);

        // The camera follows my character. While waiting to respawn it stays at the last spot
        const u32 local = ctx.session.localPlayer();
        const Pawn* mine = playerPawn(current, local);
        bool hasTarget = mine && local < m_pawnHas.size() && m_pawnHas[local] && !current.slots[local].out;
        Vec3 target = hasTarget ? m_pawnAt[local] + Vec3{0.0f, m_juice.cameraDip(), 0.0f} : Vec3{};
        // Out: the camera holds on my blast while the body flies, then follows the it player (or anyone still in)
        // The camera's wall rays skip the followed body: spectating, that's the other player (else the arm hits their capsule and goes first person)
        const Pawn* followed = mine;
        u32 watched = kNoSlot;
        if (!hasTarget && !mine && local < current.playerCount && current.slots[local].out && m_juice.spectateTarget(target, watched)) {
            hasTarget = true;
            followed = playerPawn(current, watched);
        }
        const f32 dt = static_cast<f32>(std::clamp(now - m_cameraTime, 0.0, 0.1)); // first frame / stalls: no jumps
        m_cameraTime = now;
        const Vec3 shake = ctx.feedback.cameraShake(hasTarget ? target : m_camera.brain.pose().position);
        m_camera.update(sim().world(), followed, hasTarget ? &target : nullptr, hitStop ? 0.0f : dt,
                        ctx.settings.values().display.fov + m_juice.fovOffset(), shake);
        m_camera.apply(ctx.viewport, look.camera, frame);
    }
    applyEnvironment(m_scene.environment, ctx.models, frame);
    applyProbes(m_scene, ctx.models, m_probes, frame);
    applyLights(m_scene, m_lights, frame);
    if (sim().started()) {
        m_juice.addLights(m_lights); // after the scene lights: the vector may grow, so point the frame at it again
        frame.lights = m_lights.data();
        frame.lightCount = static_cast<u32>(m_lights.size());
        m_juice.applyPost(frame.post, ctx.settings.values().gameplay.flashScale);
    }
    drawScene(m_scene, ctx.models, frame.cameraPos, m_viewTime, out);
    if (sim().started()) {
        const WorldState& shown = m_frozen ? m_frozenCurrent : sim().world().state;
        drawHideSpots(sim().world(), look, m_juice, frame.cameraPos, ctx.models, out);
        drawItems(shown, look, m_viewTime, frame.cameraPos, ctx.models, out);
        drawProps(m_frozen ? m_frozenPrevious : sim().previous(), shown, m_frozen ? m_frozenAlpha : sim().alpha(), ctx.models, out);
        drawWorld(shown, m_pawnAt, m_pawnHas, ctx.session.localPlayer(), look, m_juice, m_chars, m_viewTime, ctx.models, out);
        for (const auto& [row, at] : m_chars.events) ctx.feedback.play(row, at); // footsteps from the animation table
    }
}

// F1: round state and cheats (sent as confirmed commands, so every peer applies them on the same tick)
void GameSession::debugUi(game::GameContext& ctx) {
#if MEALKIT_DEBUG_UI
    if (!sim().started()) return;
    const World& world = sim().world();
    const WorldState& s = world.state;
    ImGui::SetNextWindowPos(ImVec2(ctx.viewport.x + 10.0f, ctx.viewport.y + 200.0f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("FuseTag", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("tick %u | round %u | phase %u | swaps %u | out %u", s.tick, s.flow.round, static_cast<u32>(s.flow.phase), s.swaps,
                    s.outCount);
        auto cheat = [&](CheatKind kind, u32 slot) {
            DebugCheat d;
            d.kind = kind;
            d.slot = slot;
            sim::sendCommand(ctx.session, d);
        };
        for (u32 i = 0; i < s.playerCount; ++i) {
            const PlayerSlot& p = s.slots[i];
            ImGui::PushID(static_cast<int>(i));
            const Pawn* pawn = playerPawn(s, i);
            ImGui::Text("%u%s fuse %4.1fs %s%s score %u", i + 1, p.bot ? " bot" : "    ",
                        static_cast<f32>(p.fuse) / static_cast<f32>(world.tickRate), p.it ? "IT " : "   ", p.out ? "OUT" : "   ", p.score);
            ImGui::SameLine();
            if (ImGui::SmallButton("it")) cheat(CheatKind::MakeIt, i);
            ImGui::SameLine();
            if (ImGui::SmallButton("fill")) cheat(CheatKind::RefillFuse, i);
            ImGui::SameLine();
            if (ImGui::SmallButton("-5s")) cheat(CheatKind::BurnFuse, i);
            ImGui::SameLine();
            if (ImGui::SmallButton("out")) cheat(CheatKind::Eliminate, i);
            if (pawn && p.bot && p.botPathCount > 0) {
                ImGui::SameLine();
                ImGui::TextDisabled("-> %.0f %.0f", p.botGoal.x, p.botGoal.z);
            }
            ImGui::PopID();
        }
        if (ImGui::Button(world.tuning.navDebug ? "Hide bot paths" : "Show bot paths")) cheat(CheatKind::ToggleNavDebug, 0);
    }
    ImGui::End();
#else
    (void)ctx;
#endif
}

} // namespace fusetag
