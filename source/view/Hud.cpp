#include "view/Hud.h"

using namespace mealkit;

namespace fusetag {

void fillHud(const World& world, u32 local, const SeatNames& names, const PlayerColors& colors, const loc::Strings& strings,
             HudVm& vm) {
    const WorldState& state = world.state;
    const u32 rate = reflect::tickRate();
    const u32 left = state.flow.phaseEnd.remaining(state.tick);
    vm.countdown = state.flow.phase == MatchPhase::Countdown ? (left + rate - 1) / rate : 0;
    vm.time = state.flow.phase == MatchPhase::Playing ? static_cast<f32>(state.tick - state.roundStart) / static_cast<f32>(rate) : 0.0f;
    vm.round = state.flow.round > 0 ? state.flow.round : 1u;
    vm.rounds = world.tuning.rounds;

    vm.fusesCount = state.playerCount;
    for (u32 slot = 0; slot < state.playerCount; ++slot) {
        const PlayerSlot& p = state.slots[slot];
        FuseRowVm& row = vm.fuses[slot];
        row.name = names[slot];
        row.color = colors[slot % colors.size()];
        row.fuse = fuseFraction(world, slot);
        row.it = p.it;
        row.out = p.out;
        row.you = slot == local;
    }

    const bool playing = state.flow.phase == MatchPhase::Playing;
    const bool mine = local < state.playerCount;
    vm.meIt = playing && mine && state.slots[local].it;
    vm.meOut = playing && mine && state.slots[local].out;
    vm.otherIt = playing && state.itSlot != kNoSlot && !vm.meIt && !vm.meOut;
    vm.itName = state.itSlot != kNoSlot ? names[state.itSlot] : std::string{};
    vm.itColor = state.itSlot != kNoSlot ? colors[state.itSlot % colors.size()] : Color{};
    vm.myFuse = mine ? fuseFraction(world, local) : 0.0f;
    const u32 staminaFull = world.tuning.staminaMax.value * world.tuning.staminaRecover.value;
    vm.stamina = mine && staminaFull ? static_cast<f32>(state.slots[local].stamina) / static_cast<f32>(staminaFull) : 1.0f;
    vm.winded = mine && state.slots[local].winded;
    vm.hidden = playing && mine && state.slots[local].hidden;
    vm.inSpot = playing && mine && !vm.hidden && state.slots[local].hideSpot != kNoSlot && !state.slots[local].it;
    const u32 cooldown = world.tuning.shoveCooldown.value;
    const u32 shoveLeft = mine ? state.slots[local].shove.remaining(state.tick) : 0u;
    vm.shoveReady = cooldown ? 1.0f - static_cast<f32>(shoveLeft) / static_cast<f32>(cooldown) : 1.0f;
    const PlayerSlot* me = mine ? &state.slots[local] : nullptr;
    vm.hasItem = playing && me && me->item < world.items.size();
    vm.itemName = vm.hasItem ? strings.get("item." + world.items.ids[me->item]) : std::string{};
    vm.effects.clear();
    if (playing && me) {
        auto add = [&](bool on, const char* key) {
            if (!on) return;
            if (!vm.effects.empty()) vm.effects += "  ";
            vm.effects += strings.get(key);
        };
        add(!me->speedy.ready(state.tick), "hud.fx.speedy");
        add(!me->vanish.ready(state.tick), "hud.fx.vanish");
        add(!me->scope.ready(state.tick), "hud.fx.scope");
    }
    vm.snared = playing && me && !me->stunned.ready(state.tick);
    vm.roundOver = state.flow.phase == MatchPhase::RoundEnd && state.roundWinner != kNoSlot;
    vm.winnerName = state.roundWinner != kNoSlot ? names[state.roundWinner] : std::string{};
}

} // namespace fusetag
