#pragma once

#include "sim/World.h"
#include "view/ViewTuning.h"

#include "mealkit/core/Reflect.h"
#include "mealkit/loc/Strings.h"

#include <array>
#include <string>

// View models for game screen UI (mealkit/ui/Ui.h). The view side fills state → view model every frame, and assets/ui/*.ui binds by field name.
namespace fusetag {

MEALKIT_STRUCT(ChatLineVm) {
    MEALKIT_FIELD(std::string, name, {}, "보낸 사람");
    MEALKIT_FIELD(std::string, text, {}, "내용");
};

// One player's fuse bar
MEALKIT_STRUCT(FuseRowVm) {
    MEALKIT_FIELD(std::string, name, {}, "이름");
    MEALKIT_FIELD(mealkit::Color, color, {}, "플레이어 색");
    MEALKIT_FIELD(mealkit::f32, fuse, 1.0f, "남은 퓨즈 (0~1)");
    MEALKIT_FIELD(bool, it, false, "술래");
    MEALKIT_FIELD(bool, out, false, "탈락");
    MEALKIT_FIELD(bool, you, false, "나");
};

// assets/ui/hud.ui. The chat fields are filled by the screen flow (app/Flow.cpp)
MEALKIT_VIEWMODEL(HudVm) {
    MEALKIT_FIELD(mealkit::f32, time, 0.0f, "라운드 경과 시간 (초)");
    MEALKIT_FIELD(mealkit::u32, countdown, 0, "시작 카운트다운 (초, 0이면 없음)");
    MEALKIT_FIELD(mealkit::u32, round, 1, "라운드");
    MEALKIT_FIELD(mealkit::u32, rounds, 1, "라운드 수");
    MEALKIT_LIST(FuseRowVm, fuses, 8, "퓨즈 (참가자 순서)");
    MEALKIT_FIELD(bool, meIt, false, "내가 술래");
    MEALKIT_FIELD(bool, meOut, false, "나는 탈락");
    MEALKIT_FIELD(bool, otherIt, false, "다른 사람이 술래");
    MEALKIT_FIELD(std::string, itName, {}, "술래 이름");
    MEALKIT_FIELD(mealkit::Color, itColor, {}, "술래 색");
    MEALKIT_FIELD(mealkit::f32, myFuse, 1.0f, "내 퓨즈 (0~1)");
    MEALKIT_FIELD(mealkit::f32, stamina, 1.0f, "내 스태미나 (0~1)");
    MEALKIT_FIELD(bool, winded, false, "지침");
    MEALKIT_FIELD(bool, hidden, false, "나는 숨음");
    MEALKIT_FIELD(bool, inSpot, false, "숨는 곳 안 (가만히 있으면 숨는다)");
    MEALKIT_FIELD(mealkit::f32, shoveReady, 1.0f, "밀치기 준비 (0~1)");
    MEALKIT_FIELD(bool, hasItem, false, "아이템 있음");
    MEALKIT_FIELD(std::string, itemName, {}, "아이템 이름");
    MEALKIT_FIELD(std::string, effects, {}, "켜진 효과 (달리기, 투명, 투시)");
    MEALKIT_FIELD(bool, snared, false, "묶임 (덫, 밀림)");
    MEALKIT_FIELD(bool, roundOver, false, "라운드 결과 중");
    MEALKIT_FIELD(std::string, winnerName, {}, "라운드 생존자 이름");
    MEALKIT_LIST(ChatLineVm, chat, 5, "채팅 (최근, 잠시 보인다)");
    MEALKIT_FIELD(bool, chatOpen, false, "채팅을 치는 중");
    MEALKIT_FIELD(std::string, chatDraft, {}, "보낼 채팅 (글자 입력)");
};

using SeatNames = std::array<std::string, mealkit::session::kMaxPlayers>;

// State → HUD (every frame). local = my player slot
void fillHud(const World& world, u32 local, const SeatNames& names, const PlayerColors& colors, const mealkit::loc::Strings& strings,
             HudVm& vm);

} // namespace fusetag
