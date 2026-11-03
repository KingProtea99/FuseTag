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

// Game feel (view/Juice.cpp). Spring impulses are in "scale per second": a kick of 4 squashes by about 0.3 at the bottom.
MEALKIT_STRUCT(JuiceTuning) {
    // Squash and stretch (y scale; x and z keep the volume)
    MEALKIT_FIELD(f32, squashFrequency, 5.0f, "스쿼시 스프링 진동수 (Hz)");
    MEALKIT_FIELD(f32, squashDamping, 0.3f, "스쿼시 스프링 감쇠 (1이면 출렁임 없음)");
    MEALKIT_FIELD(f32, jumpKick, -3.0f, "점프 순간 찌그러짐 (충격)");
    MEALKIT_FIELD(f32, airStretch, 0.025f, "공중 상승 속도당 늘어남 (m/s당)");
    MEALKIT_FIELD(f32, airStretchMax, 0.25f, "공중 늘어남 최대");
    MEALKIT_FIELD(f32, landKick, -0.45f, "착지 낙하 속도당 찌그러짐 (m/s당 충격)");
    MEALKIT_FIELD(f32, landKickMax, -7.0f, "착지 찌그러짐 최대 (충격)");
    MEALKIT_FIELD(f32, runStretch, 0.04f, "달릴 때 늘어남 (최고 속도에서)");
    MEALKIT_FIELD(f32, runSpeed, 9.0f, "늘어남과 시야각이 최대가 되는 속도 (m/s)");
    MEALKIT_FIELD(f32, bobHeight, 0.07f, "달릴 때 통통 튀는 높이 (m)");
    MEALKIT_FIELD(f32, bobCadence, 1.6f, "1m당 튀는 횟수");
    // Lean into acceleration
    MEALKIT_FIELD(f32, leanPerAccel, 0.02f, "가속도당 기울기 (rad, m/s²당)");
    MEALKIT_FIELD(f32, leanMax, 0.35f, "최대 기울기 (rad)");
    MEALKIT_FIELD(f32, leanFrequency, 4.0f, "기울기 스프링 진동수 (Hz)");
    MEALKIT_FIELD(f32, leanDamping, 0.45f, "기울기 스프링 감쇠");
    // Tag and hold
    MEALKIT_FIELD(f32, tagVictimKick, -7.0f, "잡힌 쪽 찌그러짐 (충격)");
    MEALKIT_FIELD(f32, tagVictimSpin, 25.0f, "잡힌 쪽 회전 (rad/s)");
    MEALKIT_FIELD(f32, tagTaggerKick, 5.0f, "넘겨준 쪽 늘어남 (충격)");
    MEALKIT_FIELD(f32, holdShake, 0.07f, "묶인 술래가 씩씩대는 크기");
    MEALKIT_FIELD(f32, holdShakeRate, 9.0f, "씩씩대는 빠르기 (회/초)");
    MEALKIT_FIELD(f32, holdEndKick, 6.0f, "묶임이 풀릴 때 튀어 오름 (충격)");
    MEALKIT_FIELD(f32, flashSeconds, 0.12f, "잡힐 때 하얗게 번쩍이는 시간 (초)");
    // The bomb
    MEALKIT_FIELD(Color, bombColor, (Color::rgb(30, 30, 34)), "폭탄 색");
    MEALKIT_FIELD(f32, bombSize, 0.5f, "폭탄 크기 (m)");
    MEALKIT_FIELD(f32, bombHeight, 0.45f, "머리 위 폭탄 높이 (m)");
    MEALKIT_FIELD(f32, bombPulseSlow, 1.0f, "퓨즈가 가득일 때 폭탄 맥박 (회/초)");
    MEALKIT_FIELD(f32, bombPulseFast, 6.0f, "퓨즈가 거의 없을 때 폭탄 맥박 (회/초)");
    MEALKIT_FIELD(f32, bombPulseSize, 0.12f, "폭탄 맥박 크기 (비율)");
    MEALKIT_FIELD(f32, bombFlashBelow, 0.3f, "이 비율 아래로 퓨즈가 남으면 폭탄이 번쩍인다");
    MEALKIT_FIELD(f32, bombFlightSeconds, 0.2f, "잡을 때 폭탄이 날아가는 시간 (초)");
    MEALKIT_FIELD(f32, bombArc, 1.4f, "폭탄이 날아가는 포물선 높이 (m)");
    MEALKIT_FIELD(f32, sparkRateSlow, 6.0f, "불꽃 수 (초당, 퓨즈 가득)");
    MEALKIT_FIELD(f32, sparkRateFast, 30.0f, "불꽃 수 (초당, 퓨즈 거의 없음)");
    MEALKIT_FIELD(f32, bombLightRange, 3.0f, "퓨즈 불빛 범위 (m)");
    MEALKIT_FIELD(Vec3, bombLight, (Vec3{3.0f, 1.6f, 0.4f}), "퓨즈 불빛 (선형 색 × 세기)");
    // Short lights and screen kicks
    MEALKIT_FIELD(f32, tagLightRange, 5.0f, "잡기 불빛 범위 (m)");
    MEALKIT_FIELD(Vec3, tagLight, (Vec3{5.0f, 4.5f, 3.0f}), "잡기 불빛 (선형 색 × 세기)");
    MEALKIT_FIELD(f32, tagLightSeconds, 0.15f, "잡기 불빛 시간 (초)");
    MEALKIT_FIELD(f32, blastLightRange, 12.0f, "폭발 불빛 범위 (m)");
    MEALKIT_FIELD(Vec3, blastLight, (Vec3{14.0f, 7.0f, 2.0f}), "폭발 불빛 (선형 색 × 세기)");
    MEALKIT_FIELD(f32, blastLightSeconds, 0.35f, "폭발 불빛 시간 (초)");
    MEALKIT_FIELD(f32, blastBloom, 1.2f, "폭발 때 블룸 추가");
    MEALKIT_FIELD(f32, blastBloomSeconds, 0.3f, "폭발 블룸 시간 (초)");
    MEALKIT_FIELD(f32, fovSpeed, 6.0f, "최고 속도에서 넓어지는 시야각 (도)");
    MEALKIT_FIELD(f32, fovTagKick, 60.0f, "잡기에 끼었을 때 시야각 충격 (도/초)");
    MEALKIT_FIELD(f32, fovFrequency, 4.0f, "시야각 스프링 진동수 (Hz)");
    MEALKIT_FIELD(f32, fovDamping, 0.5f, "시야각 스프링 감쇠");
    // "It!" marker over a new it, afterimages of the one who passed the bomb on
    MEALKIT_FIELD(Color, markerColor, (Color::rgb(255, 70, 50)), "새 술래 표시 색");
    MEALKIT_FIELD(f32, markerSize, 0.35f, "새 술래 표시 크기 (m)");
    MEALKIT_FIELD(f32, markerHeight, 0.75f, "폭탄 위 표시 높이 (m)");
    MEALKIT_FIELD(f32, markerSeconds, 0.9f, "새 술래 표시 시간 (초)");
    MEALKIT_FIELD(f32, ghostInterval, 0.05f, "잔상 간격 (초)");
    MEALKIT_FIELD(mealkit::u32, ghostCount, 5, "잔상 수");
    // Blown up: tumble, then shrink away before the body is removed
    MEALKIT_FIELD(f32, tumbleSpeed, 9.0f, "탈락한 몸이 도는 빠르기 (rad/s)");
    MEALKIT_FIELD(f32, shrinkSeconds, 0.35f, "사라지기 전 줄어드는 시간 (초)");
    // Hunter direction (an arrow on the ground around me) and camera
    MEALKIT_FIELD(f32, arrowRange, 12.0f, "술래 방향 화살표가 보이는 거리 (m)");
    MEALKIT_FIELD(f32, arrowDistance, 1.2f, "화살표가 놓이는 거리 (m, 내 중심에서)");
    MEALKIT_FIELD(Vec3, arrowSize, (Vec3{0.3f, 0.06f, 1.1f}), "화살표 크기 (m)");
    MEALKIT_FIELD(f32, landDip, -0.12f, "착지 낙하 속도당 카메라 내려감 (m/s당 충격)");
    MEALKIT_FIELD(f32, dipFrequency, 3.0f, "카메라 내려감 스프링 진동수 (Hz)");
    MEALKIT_FIELD(f32, dipDamping, 0.55f, "카메라 내려감 스프링 감쇠");
    // Danger when I'm it
    MEALKIT_FIELD(f32, dangerVignette, 0.35f, "내가 술래일 때 비네트");
    MEALKIT_FIELD(f32, dangerPulse, 0.25f, "폭탄 맥박에 맞춘 비네트 추가 (퓨즈가 적을수록)");
    MEALKIT_FIELD(f32, dangerAberration, 0.006f, "퓨즈 위험 때 맥박마다 색수차");
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
    MEALKIT_FIELD(JuiceTuning, juice, {}, "손맛 (스쿼시, 폭탄, 빛, 화면 효과)");
    MEALKIT_FIELD(f32, resultsSeconds, 20.0f, "결과 화면 뒤 방장이 자동으로 로비로 돌아가기까지 (초)");
    MEALKIT_FIELD(f32, chatShowSeconds, 8.0f, "매치 중 새 채팅 줄을 보여 주는 시간 (초)");
};

} // namespace fusetag
