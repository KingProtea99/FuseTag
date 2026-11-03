#pragma once

#include "sim/World.h"

#include "mealkit/core/Reflect.h"
#include "mealkit/scene/Scene.h"

#include <array>

// View tuning: assets/view.tuning (edited in the "View" tab of the editor [Tuning] panel). Holds display-only values (camera, colors).
// Not in the content hash, so it changes immediately even during multiplayer play. The view side reads it with ctx.viewTuning<ViewTuning>().
// Can't be read from sim/ (SimCheck blocks view/ headers). Game rule values are in sim/Tuning.h.
namespace fusetag {

using mealkit::Color;

// Third-person camera when the scene has no active virtual camera (scene virtual_camera objects override it)
inline mealkit::scene::VirtualCameraFields defaultFollowCamera() {
    mealkit::scene::VirtualCameraFields f;
    f.yaw = 45.0f;
    f.pitch = 30.0f;
    f.collisionRadius = 0.3f;
    return f;
}

// Camera behind the menu/lobby: slowly circles the scene center
inline mealkit::scene::VirtualCameraFields defaultMenuCamera() {
    mealkit::scene::VirtualCameraFields f;
    f.body = mealkit::scene::CameraBody::Orbit;
    f.distance = 20.0f;
    f.yaw = 45.0f;
    f.pitch = 30.0f;
    f.orbitSpeed = 4.0f;
    return f;
}

MEALKIT_STRUCT(CameraTuning) {
    MEALKIT_FIELD(f32, orbitSensitivity, 0.005f, "회전 감도 (rad/픽셀)");
    MEALKIT_FIELD(f32, zoomStep, 2.0f, "휠 한 칸 줌 (m)");
    MEALKIT_FIELD(f32, nearPlane, 0.1f, "가까운 자르기 면 (m)");
    MEALKIT_FIELD(f32, farPlane, 1000.0f, "먼 자르기 면 (m)");
    MEALKIT_FIELD(mealkit::scene::VirtualCameraFields, fallback, defaultFollowCamera(), "기본 카메라 (씬에 가상 카메라가 없을 때)");
    MEALKIT_FIELD(mealkit::scene::VirtualCameraFields, menu, defaultMenuCamera(), "메뉴 뒤 카메라");
};

using PlayerColors = std::array<Color, mealkit::net::kMaxPlayers>;

MEALKIT_STRUCT(ViewTuning) {
    MEALKIT_FIELD(CameraTuning, camera, {}, "카메라");
    MEALKIT_FIELD(Vec3, pawnSize, (Vec3{0.8f, 1.8f, 0.8f}), "캐릭터 크기 (m, 상자로 그린다)");
    MEALKIT_FIELD(PlayerColors, playerColors,
                  (PlayerColors{Color::rgb(66, 135, 245), Color::rgb(235, 64, 52), Color::rgb(52, 199, 89),
                                Color::rgb(250, 204, 21), Color::rgb(168, 85, 247), Color::rgb(236, 72, 153),
                                Color::rgb(20, 184, 166), Color::rgb(249, 115, 22)}),
                  "플레이어 색");
    MEALKIT_FIELD(Color, outColor, (Color::rgb(40, 40, 44)), "탈락한 캐릭터 색");
    MEALKIT_FIELD(Color, bombColor, (Color::rgb(30, 30, 34)), "폭탄 색");
    MEALKIT_FIELD(f32, bombSize, 0.5f, "폭탄 크기 (m)");
    MEALKIT_FIELD(f32, bombHeight, 0.45f, "머리 위 폭탄 높이 (m)");
    MEALKIT_FIELD(f32, bombPulseSlow, 1.0f, "퓨즈가 가득일 때 폭탄 맥박 (회/초)");
    MEALKIT_FIELD(f32, bombPulseFast, 6.0f, "퓨즈가 거의 없을 때 폭탄 맥박 (회/초)");
    MEALKIT_FIELD(f32, bombPulseSize, 0.12f, "폭탄 맥박 크기 (비율)");
    MEALKIT_FIELD(f32, bombFlashBelow, 0.3f, "이 비율 아래로 퓨즈가 남으면 폭탄이 번쩍인다");
    MEALKIT_FIELD(f32, resultsSeconds, 20.0f, "결과 화면 뒤 방장이 자동으로 로비로 돌아가기까지 (초)");
    MEALKIT_FIELD(f32, chatShowSeconds, 8.0f, "매치 중 새 채팅 줄을 보여 주는 시간 (초)");
};

} // namespace fusetag
