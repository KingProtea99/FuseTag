#pragma once

#include "mealkit/asset/ModelCache.h"
#include "mealkit/core/Math.h"
#include "mealkit/core/Random.h"
#include "mealkit/net/Lockstep.h"
#include "mealkit/physics/PhysicsWorld.h"
#include "mealkit/scene/Scene.h"
#include "mealkit/sim/MatchFlow.h"
#include "mealkit/sim/Movement.h"
#include "mealkit/sim/NavGrid.h"
#include "mealkit/sim/Pool.h"
#include "mealkit/sim/TickEvents.h"
#include "mealkit/sim/TickTimer.h"
#include "sim/Objects.h"
#include "sim/Tables.h"
#include "sim/Tuning.h"

#include <vector>

// Deterministic simulation of fuse tag (physics: Jolt). Whoever is "it" carries a burning fuse; tagging someone hands it over,
// running out of fuse knocks you out of the round, and the last one standing wins (docs/DESIGN.md).
// Empty seats are bots: their slots are driven by sim/Bots.cpp, so every peer runs the same bots.
// sim/ folder rules (checked by SimCheck at build time): no CRT trig, real time, rand, renderer/platform headers, or mutable global state.
// State is declared with MEALKIT_FIELD: the hash (desync detection), snapshots (rewind/shadow simulation), and the editor [State] panel use it as is.
namespace fusetag {

using mealkit::f32;
using mealkit::i8;
using mealkit::u32;
using mealkit::u64;
using mealkit::u8;
using mealkit::Vec3;
using mealkit::sim::Id;
using mealkit::sim::TickTimer;

inline constexpr u8 kButtonJump = 1;
inline constexpr u8 kButtonSprint = 2;
inline constexpr u8 kButtonShove = 4;
inline constexpr u8 kButtonItem = 8;
inline constexpr u8 kButtonAuto = 0x80; // the bot AI drives this player (--bot, headless tests); part of the input, so deterministic

inline constexpr u32 kNoSlot = 0xFFFFFFFFu;
inline constexpr u32 kBotPathPoints = 12; // waypoints kept per bot (a longer path is searched again later)

// Player input sent over the network (8 bytes). The client turns the camera-relative stick into a quantized world direction.
struct GameInput {
    i8 moveX = 0; // -127..127
    i8 moveZ = 0;
    u8 buttons = 0;
    u8 reserved[5] = {};
};
static_assert(sizeof(GameInput) == mealkit::net::kInputBytes);

// A character in the world. Removed when it falls (back after a respawn wait) or some time after its player blew up.
MEALKIT_STRUCT(Pawn) {
    MEALKIT_FIELD(u32, player, 0, "플레이어 슬롯");
    MEALKIT_FIELD(Vec3, position, {}, "위치");
    MEALKIT_FIELD(Vec3, velocity, {}, "속도");
    MEALKIT_FIELD(mealkit::sim::Mover, mover, {}, "이동 (모드, 물리 바디, 땅에 닿음)");
    MEALKIT_FIELD(f32, fallSpeed, 0.0f, "공중에서 가장 빠른 낙하 속도 (m/s, 착지 이벤트)");
};

// A loose physics box from a scene Prop object (the body's state is in the physics hash; position/rotation are copied for the view)
MEALKIT_STRUCT(PropBox) {
    MEALKIT_FIELD(Vec3, position, {}, "위치");
    MEALKIT_FIELD(mealkit::Quat, rotation, {}, "회전");
    MEALKIT_FIELD(Vec3, halfExtents, {}, "반 크기");
    MEALKIT_FIELD(u32, color, 0, "색 (씬 물체 색)");
    MEALKIT_FIELD(f32, mass, 0.0f, "질량 (kg)");
    MEALKIT_FIELD(mealkit::physics::BodyId, body, mealkit::physics::kInvalidBody, "물리 바디");
};

// An item crate from the scene (fixed place); empty while waiting to come back
MEALKIT_STRUCT(Crate) {
    MEALKIT_FIELD(Vec3, position, {}, "위치");
    MEALKIT_FIELD(bool, full, true, "아이템 있음");
    MEALKIT_FIELD(TickTimer, refill, {}, "다시 나타나는 때");
    MEALKIT_FIELD(u32, respawn, 0, "다시 나타나는 시간 (틱)");
};

// A snare on the floor: whoever steps on it (except the one who set it) is stuck for a moment
MEALKIT_STRUCT(Trap) {
    MEALKIT_FIELD(Vec3, position, {}, "위치 (바닥)");
    MEALKIT_FIELD(u32, owner, kNoSlot, "놓은 사람");
    MEALKIT_FIELD(TickTimer, armed, {}, "작동 시작");
    MEALKIT_FIELD(TickTimer, expires, {}, "사라지는 때");
    MEALKIT_FIELD(u32, duration, 0, "묶는 시간 (틱)");
};

inline constexpr u32 kNoItem = 0xFFFFFFFFu;

// A seat in the match: a person or a bot. Persists through the match (a person who leaves is replaced by a bot).
MEALKIT_STRUCT(PlayerSlot) {
    MEALKIT_FIELD(bool, connected, false, "참가");
    MEALKIT_FIELD(bool, bot, false, "봇");
    MEALKIT_FIELD(u32, score, 0, "점수 (라운드 순위 점수 합계)");
    MEALKIT_FIELD(u32, lastPlace, 0, "지난 라운드 순위 (1부터, 0이면 없음)");
    MEALKIT_FIELD(Id<Pawn>, pawn, {}, "캐릭터 (없으면 리스폰 대기)");
    MEALKIT_FIELD(TickTimer, respawn, {}, "리스폰");
    // Round
    MEALKIT_FIELD(u32, fuse, 0, "남은 퓨즈 (틱)");
    MEALKIT_FIELD(bool, it, false, "술래");
    MEALKIT_FIELD(bool, out, false, "탈락");
    MEALKIT_FIELD(u32, outOrder, 0, "탈락 순서 (1이 처음, 0이면 살아 있음)");
    MEALKIT_FIELD(TickTimer, hold, {}, "묶임 (새 술래)");
    MEALKIT_FIELD(TickTimer, immune, {}, "되잡기 면역");
    MEALKIT_FIELD(u32, immuneFrom, kNoSlot, "면역 대상 (넘겨받은 술래)");
    MEALKIT_FIELD(TickTimer, boost, {}, "넘겨준 뒤 빨라짐");
    MEALKIT_FIELD(TickTimer, remove, {}, "탈락한 캐릭터가 사라지는 때");
    MEALKIT_FIELD(u32, stamina, 0, "스태미나 (단위: staminaMax×staminaRecover 가 가득)");
    MEALKIT_FIELD(bool, winded, false, "지침 (가득 찰 때까지 못 달림)");
    MEALKIT_FIELD(bool, sprinting, false, "달리는 중");
    MEALKIT_FIELD(u32, tags, 0, "이번 매치에 잡은 횟수");
    MEALKIT_FIELD(bool, hidden, false, "숨음");
    MEALKIT_FIELD(u32, hideSpot, kNoSlot, "있는 숨는 곳 (World::hideSpots 번호)");
    MEALKIT_FIELD(u32, stillTicks, 0, "숨는 곳에서 가만히 있은 틱");
    MEALKIT_FIELD(u32, searched, 0, "술래가 옆에 있은 틱 (숨었을 때)");
    MEALKIT_FIELD(TickTimer, shove, {}, "밀치기 재사용");
    MEALKIT_FIELD(TickTimer, stunned, {}, "밀려서 조종 못 함 (덫에 걸려도)");
    MEALKIT_FIELD(u32, item, kNoItem, "가진 아이템 (items.table 행 번호)");
    MEALKIT_FIELD(TickTimer, speedy, {}, "달리기 신발");
    MEALKIT_FIELD(f32, speedyScale, 1.0f, "달리기 신발 속도 배율");
    MEALKIT_FIELD(TickTimer, vanish, {}, "투명");
    MEALKIT_FIELD(TickTimer, scope, {}, "투시");
    MEALKIT_FIELD(u32, itemsUsed, 0, "이번 매치에 쓴 아이템");
    // Bot brain (bots and players with kButtonAuto)
    MEALKIT_FIELD(TickTimer, botThink, {}, "봇 다음 판단");
    MEALKIT_FIELD(bool, botRepath, false, "봇 길찾기 대기");
    MEALKIT_FIELD(Vec3, botGoal, {}, "봇 목표 지점");
    MEALKIT_FIELD(u32, botTarget, kNoSlot, "봇 술래의 추격 대상");
    MEALKIT_FIELD(bool, botSprint, false, "봇 달리기");
    MEALKIT_FIELD(bool, botHiding, false, "봇이 숨는 곳으로 가는 중");
    MEALKIT_FIELD(bool, botShove, false, "봇이 밀치려 함");
    MEALKIT_FIELD(u32, botSearchSpot, kNoSlot, "봇 술래가 뒤지는 숨는 곳");
    MEALKIT_FIELD(u32, botSearchTicks, 0, "그 숨는 곳 옆에 있은 틱");
    MEALKIT_FIELD(bool, botUseItem, false, "봇이 아이템을 쓰려 함");
    MEALKIT_ARRAY(Vec3, kBotPathPoints, botPath, "봇 경로 지점");
    MEALKIT_FIELD(u32, botPathCount, 0, "봇 경로 지점 수");
    MEALKIT_FIELD(u32, botPathNext, 0, "봇 다음 경로 지점");
};

// Simulation → view notifications (sound/effects). sim/ only writes them. The feedback table (assets/feedback.table) chooses feedback by kind name.
// These are the contract the game feel work builds on (docs/ROADMAP.md M2): add kinds rather than reading state differences in view/.
MEALKIT_ENUM(EventKind, u8, Spawn, Jump, Land, Fall, Tag, HoldEnd, FuseWarn, FuseDanger, Eliminated, NewIt, SprintStart,
             StaminaEmpty, RoundWin, Countdown, MatchStart, MatchEnd, Hide, Unhide, Found, Shove, ShoveHit, Impact, Pickup,
             ItemUse, TrapSet, Snared, CrateBack);
inline constexpr u8 kNoPlayer = 0xFF;

MEALKIT_STRUCT(GameEvent) {
    MEALKIT_FIELD(EventKind, kind, EventKind::Spawn, "종류");
    MEALKIT_FIELD(u8, player, kNoPlayer, "플레이어 슬롯 (잡기: 넘겨준 쪽)");
    MEALKIT_FIELD(u8, other, kNoPlayer, "상대 슬롯 (잡기: 넘겨받은 새 술래)");
    MEALKIT_FIELD(Vec3, position, {}, "위치");
    MEALKIT_FIELD(f32, value, 0.0f, "카운트다운: 남은 초, 착지: 낙하 속도, 퓨즈 경고: 남은 비율, 숨기: 숨는 곳 번호, 밀치기: 범위, 부딪힘: 속도");
};

inline GameEvent makeEvent(EventKind kind, u8 player, Vec3 position, f32 value, u8 other = kNoPlayer) {
    GameEvent e;
    e.kind = kind;
    e.player = player;
    e.other = other;
    e.position = position;
    e.value = value;
    return e;
}
inline constexpr u32 kMaxEventsPerTick = 64;

// Match phase (engine mealkit/sim/MatchFlow.h, driven by sim/Match.cpp). The screen flow opens the results screen at Results
using MatchPhase = mealkit::sim::FlowPhase;

MEALKIT_STRUCT(WorldState) {
    MEALKIT_FIELD(u32, tick, 0, "틱");
    MEALKIT_FIELD(mealkit::sim::MatchFlow, flow, {}, "매치 흐름 (단계, 라운드)");
    MEALKIT_FIELD(u32, playerCount, 0, "참가자 수 (사람 + 봇)");
    MEALKIT_FIELD(u32, humanCount, 0, "입력을 보내는 사람 수 (앞쪽 슬롯)");
    MEALKIT_ARRAY(PlayerSlot, mealkit::net::kMaxPlayers, slots, "플레이어 슬롯");
    MEALKIT_FIELD(mealkit::sim::Pool<Pawn>, pawns, {}, "캐릭터");
    MEALKIT_FIELD(mealkit::sim::Pool<PropBox>, props, {}, "물리 물체");
    MEALKIT_FIELD(mealkit::sim::Pool<Crate>, crates, {}, "아이템 상자");
    MEALKIT_FIELD(mealkit::sim::Pool<Trap>, traps, {}, "덫");
    MEALKIT_FIELD(u32, pickups, 0, "이번 매치 주운 아이템 수");
    MEALKIT_FIELD(u32, snares, 0, "이번 매치 덫에 걸린 수");
    MEALKIT_FIELD(u32, itSlot, kNoSlot, "술래 슬롯");
    MEALKIT_FIELD(u32, roundStart, 0, "라운드 시작 틱");
    MEALKIT_FIELD(u32, outCount, 0, "이번 라운드 탈락 수");
    MEALKIT_FIELD(u32, roundWinner, kNoSlot, "지난 라운드 마지막 생존자");
    MEALKIT_FIELD(u32, swaps, 0, "이번 매치 술래 교체 수");
    MEALKIT_FIELD(u32, hides, 0, "이번 매치 숨은 수");
    MEALKIT_FIELD(u32, founds, 0, "이번 매치 찾아낸 수");
    MEALKIT_FIELD(u32, shoves, 0, "이번 매치 밀치기 수 (누군가 밀린 것만)");
    MEALKIT_FIELD(mealkit::Pcg32, rng, {}, "난수 (첫 술래, 봇)");
};

// The whole simulation. sim/ system functions (Systems.h) take only this (World&).
struct World {
    World() = default;
    World(const World&) = delete;
    World& operator=(const World&) = delete;

    // humans = people in the match (input senders); bots fill up to tuning.botFill. physicsThreads = 0 matches the core count
    void reset(u64 seed, u32 humans, const Tuning& tuning, const mealkit::sim::Table<ItemKind>& items, const mealkit::scene::Scene& scene,
               mealkit::asset::ModelCache& models, u32 tickRate, u32 physicsThreads = 0);
    // Changes tuning mid-match (editor solo play only)
    void setTuning(const Tuning& value);
    void step(const mealkit::net::TickInputs& inputs);
    u64 hash() const;
    u32 activeBodies() const { return physics.activeBodyCount(); }

    // Data decided at match start
    u32 tickRate = 30;
    f32 tickDt = 1.0f / 30.0f; // const-ok: default (reset sets it from the project tick rate)
    std::vector<Vec3> spawns;  // the scene's spawn points
    Vec3 arenaMin, arenaMax;   // MatchArea
    struct HideArea {
        Vec3 min, max;
        HideKind kind = HideKind::Bush;
    };
    std::vector<HideArea> hideSpots; // HideSpot objects in scene order
    Tuning tuning;
    mealkit::sim::MatchRules rules;    // from tuning (sim/Match.cpp matchRules)
    mealkit::sim::Table<ItemKind> items; // tables/items.table (content hash)
    mealkit::sim::NavGrid nav;         // baked from the static scene at reset, same on every peer (not state)
    mealkit::sim::NavSearch navSearch; // A* scratch memory (not state)

    // State
    WorldState state;
    GameInput inputs[mealkit::net::kMaxPlayers] = {}; // this tick's inputs (filled at the start of step, zero for bot seats)
    mealkit::physics::PhysicsWorld physics;

    // This tick's view notifications (cleared at the start of step, not hashed)
    mealkit::sim::TickEvents<GameEvent, kMaxEventsPerTick> events;
};

// Lookups also used by the view side (read only)
const Pawn* playerPawn(const WorldState& state, u32 slot); // nullptr while waiting to respawn or after being removed
bool aliveInRound(const PlayerSlot& slot);                 // connected and not out
f32 fuseFraction(const World& world, u32 slot);            // 0..1 of the fuse left
bool botDriven(const World& world, u32 slot);              // a bot seat, or a player whose input asks for the bot AI
u32 hideSpotAt(const World& world, const Vec3& feet);      // index of the hide spot the feet are in (kNoSlot if none)
bool unseen(const WorldState& state, u32 slot);            // hidden or invisible: the view doesn't draw them for others

} // namespace fusetag
