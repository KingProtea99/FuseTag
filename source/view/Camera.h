#pragma once

#include "sim/World.h"
#include "view/ViewTuning.h"

#include "mealkit/camera/Brain.h"
#include "mealkit/input/Actions.h"
#include "mealkit/platform/Window.h"
#include "mealkit/render/Renderer.h"

// Game camera: the engine camera brain (mealkit/camera/Brain.h) over the scene's virtual cameras, with the player's look/zoom input.
// Without virtual cameras in the scene, CameraTuning::fallback (third-person spring arm) follows my character. Unrelated to the simulation.
namespace fusetag {

struct GameCamera {
    mealkit::camera::Brain brain;
    bool orbiting = false;
    mealkit::Vec2 look;
    mealkit::f32 zoom = 0.0f;

    void load(const mealkit::scene::Scene& flat, const CameraTuning& tuning);
    // Every frame: actions orbit (rotate with look while held), turn (rotate immediately, pad), zoom (zoom amount)
    void control(mealkit::Window& window, const mealkit::input::Actions& actions, const CameraTuning& tuning);
    void release(mealkit::Window& window); // menus open: no turning
    // Moves the brain. target = my character or the one I spectate (nullptr while waiting to respawn), followed = its body (the spring arm
    // wall rays skip it), world for those rays, shake from the feedback table
    void update(const World& world, const Pawn* followed, const mealkit::Vec3* target, mealkit::f32 dt, mealkit::f32 fov, mealkit::Vec3 shake);
    void apply(mealkit::render::Viewport viewport, const CameraTuning& tuning, mealkit::render::FrameParams& frame) const {
        brain.apply(viewport, tuning.nearPlane, tuning.farPlane, frame);
    }
    mealkit::Vec3 forward() const { return brain.flatForward(); } // horizontal forward (move input → world direction)
};

} // namespace fusetag
