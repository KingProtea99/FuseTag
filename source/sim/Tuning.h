#pragma once

#include "mealkit/core/Math.h"
#include "mealkit/core/Reflect.h"
#include "mealkit/net/Lockstep.h"
#include "mealkit/sim/Movement.h"
#include "mealkit/sim/NavGrid.h"

#include <array>

// Game tuning values: assets/game.tuning (edited in the editor [Tuning] panel). The World copies it at match start and reads it as world.tuning.
// In multiplayer it's fixed at match start and goes into the content hash (peers using different values are rejected).
// Values that differ per map go in scene objects (sim/Objects.h), display-only values in view/ViewTuning.h (engine docs/CODING_MODEL.md "Where to put numbers").
namespace fusetag {

// Round points by placement: [0] = last one standing, [1] = second, ...
using PlacementPoints = std::array<mealkit::u32, mealkit::net::kMaxPlayers>;

MEALKIT_STRUCT(Tuning) {
    // Match (engine mealkit/sim/MatchFlow.h). A round ends when one player is left, so there is no time limit
    MEALKIT_FIELD(mealkit::Ticks, countdown, mealkit::Ticks::seconds(3.0f), "시작 카운트다운 (초, 0이면 없음)");
    MEALKIT_FIELD(mealkit::Ticks, roundEndDelay, mealkit::Ticks::seconds(4.0f), "라운드 결과를 보여 주는 시간 (초)");
    MEALKIT_FIELD(mealkit::u32, rounds, 3, "라운드 수 (모두 끝나면 점수 합계로 순위)");
    MEALKIT_FIELD(mealkit::u32, minPlayers, 1, "이만큼 모일 때까지 대기");
    MEALKIT_FIELD(mealkit::u32, botFill, 6, "봇으로 채울 인원 (사람 포함, 최대 8)");
    MEALKIT_FIELD(PlacementPoints, placementPoints, (PlacementPoints{10, 7, 5, 4, 3, 2, 1, 0}), "순위별 점수 (1등부터)");
    // Fuse: burns only while you're it, and what burned stays burned for the round
    MEALKIT_FIELD(mealkit::Ticks, fuseLength, mealkit::Ticks::seconds(30.0f), "퓨즈 길이 (초)");
    MEALKIT_FIELD(mealkit::Ticks, fuseRampAfter, mealkit::Ticks::seconds(90.0f), "라운드가 이만큼 지나면 퓨즈가 빨리 탄다 (초)");
    MEALKIT_FIELD(mealkit::u32, fuseRampBurn, 2, "빨리 탈 때 틱당 타는 양 (배)");
    MEALKIT_FIELD(mealkit::f32, fuseWarn, 0.3f, "퓨즈 경고 1 (남은 비율)");
    MEALKIT_FIELD(mealkit::f32, fuseDanger, 0.1f, "퓨즈 경고 2 (남은 비율)");
    // Tag
    MEALKIT_FIELD(mealkit::f32, tagRange, 1.3f, "술래가 잡는 수평 거리 (m, 캐릭터 중심 사이)");
    MEALKIT_FIELD(mealkit::f32, tagHeight, 1.5f, "술래가 잡는 높이 차 (m)");
    MEALKIT_FIELD(mealkit::Ticks, tagHold, mealkit::Ticks::seconds(1.5f), "새 술래가 묶여 있는 시간 (초)");
    MEALKIT_FIELD(mealkit::Ticks, startHold, mealkit::Ticks::seconds(3.0f), "라운드 첫 술래가 묶여 있는 시간 (초)");
    MEALKIT_FIELD(mealkit::Ticks, tagBackImmunity, mealkit::Ticks::seconds(3.0f), "넘겨준 술래에게 바로 다시 잡히지 않는 시간 (초)");
    MEALKIT_FIELD(mealkit::f32, escapeBoost, 1.35f, "넘겨준 직후 이동 속도 배율");
    MEALKIT_FIELD(mealkit::Ticks, escapeBoostTime, mealkit::Ticks::seconds(1.5f), "넘겨준 직후 빨라지는 시간 (초)");
    MEALKIT_FIELD(mealkit::f32, itSpeedScale, 1.15f, "술래 이동 속도 배율");
    // Elimination
    MEALKIT_FIELD(mealkit::f32, blastLaunch, 14.0f, "탈락할 때 위로 날아가는 속도 (m/s)");
    MEALKIT_FIELD(mealkit::f32, blastSpin, 4.0f, "탈락할 때 옆으로 날아가는 속도 (m/s)");
    MEALKIT_FIELD(mealkit::Ticks, blastRemove, mealkit::Ticks::seconds(2.0f), "탈락한 캐릭터가 사라지기까지 (초)");
    // Sprint (stamina)
    MEALKIT_FIELD(mealkit::f32, sprintScale, 1.4f, "달리기 속도 배율");
    MEALKIT_FIELD(mealkit::Ticks, staminaMax, mealkit::Ticks::seconds(3.0f), "스태미나 (달릴 수 있는 초)");
    MEALKIT_FIELD(mealkit::Ticks, staminaRecover, mealkit::Ticks::seconds(4.0f), "빈 스태미나가 다 차는 시간 (초)");
    // Characters (engine movement modes, mealkit/sim/Movement.h)
    MEALKIT_FIELD(mealkit::sim::MoveTuning, move, {}, "이동");
    MEALKIT_FIELD(mealkit::Vec3, gravity, (mealkit::Vec3{0.0f, -20.0f, 0.0f}), "중력 (m/s²)");
    MEALKIT_FIELD(mealkit::f32, pawnHalfHeight, 0.9f, "캐릭터 반 높이 (m)");
    MEALKIT_FIELD(mealkit::f32, pawnRadius, 0.45f, "캐릭터 반지름 (m)");
    MEALKIT_FIELD(mealkit::f32, pawnMass, 80.0f, "캐릭터 질량 (kg)");
    MEALKIT_FIELD(mealkit::f32, spawnSpacing, 1.2f, "스폰 지점보다 많을 때 옆으로 비켜 서는 거리 (m)");
    MEALKIT_FIELD(mealkit::f32, fallHeight, -20.0f, "이 높이 아래로 떨어지면 리스폰 (m)");
    MEALKIT_FIELD(mealkit::Ticks, respawn, mealkit::Ticks::seconds(1.5f), "리스폰 대기 (초)");
    MEALKIT_FIELD(mealkit::f32, landSpeed, 6.0f, "착지 이벤트를 내는 최소 낙하 속도 (m/s)");
    // Bots (nav grid paths)
    MEALKIT_FIELD(mealkit::Ticks, botThink, mealkit::Ticks::seconds(0.4f), "봇 판단 간격 (초, 반응 지연)");
    MEALKIT_FIELD(mealkit::f32, botSpeedScale, 0.95f, "봇 이동 속도 배율");
    MEALKIT_FIELD(mealkit::f32, botChaseDirect, 3.0f, "봇 술래가 길 없이 곧장 쫓는 거리 (m)");
    MEALKIT_FIELD(mealkit::f32, botSprintRange, 6.0f, "봇이 달리기 시작하는 술래와의 거리 (m)");
    MEALKIT_FIELD(mealkit::f32, botFleeRadius, 14.0f, "봇이 도망치기 시작하는 술래와의 거리 (m)");
    MEALKIT_FIELD(mealkit::f32, botFleeDistance, 8.0f, "봇이 도망칠 지점까지 거리 (m)");
    MEALKIT_FIELD(mealkit::u32, botFleeSamples, 8, "도망칠 지점 후보 수");
    MEALKIT_FIELD(mealkit::f32, botWanderRadius, 8.0f, "술래가 멀 때 돌아다니는 거리 (m)");
    MEALKIT_FIELD(mealkit::sim::NavBakeSettings, nav, {}, "길찾기 그리드");
    MEALKIT_FIELD(mealkit::u32, botPathsPerTick, 2, "틱당 길찾기 수 (슬롯 순서)");
    MEALKIT_FIELD(mealkit::u32, botPathExpand, 4096, "길찾기 한 번에 살펴보는 최대 칸 수");
    MEALKIT_FIELD(bool, navDebug, false, "봇 경로 그리기 (디버그)");
    // Scene
    MEALKIT_FIELD(mealkit::f32, solidFriction, 0.8f, "씬 충돌 물체 마찰");
};

} // namespace fusetag
