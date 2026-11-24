# Code review notes (2026-11-24)

Static scan of `source/` against the engine's `docs/CODING_MODEL.md`. Done with `grep` only: no build, no `check.ps1`, no SimCheck run. No rule violations were found; the items below are things to look at.

## Result

- No `new`/`delete`/smart pointers. `sim/` pointers are short-lived `const Pawn*` lookups from `Pool` (`playerPawn(s, slot)`); nothing stores an object pointer.
- `sim/` includes nothing from `view/` or `app/`; `view/` never writes `World` and never includes `app/`.
- The only inheritance is `GameSession` over the engine's `SimulationGame<World>`. No mutable global state (`static` is `constexpr` strings only).

## Things to look at

1. **`thread_local` buffers in `view/Draw.cpp`** (lines 155, 198, 199, 209: `fired`, `pose`, `palette`, `ghosts`). View side, so no determinism effect, but it is hidden mutable state against "no mutable global state". Move them to members of the drawing object or `GameSession`.
2. **`std::sqrt` / `std::clamp` / `std::min` / `std::max` in `sim/Props.cpp` and `sim/Bots.cpp`.** `sqrt` is IEEE-exact so the CRT trig ban likely doesn't apply; SimCheck at build time decides. Confirm with a build.
3. **`std::vector` members in `World`** (`spawns`, `hideSpots`, `zoneRules`, `ruleTimers`, `sim/World.h`). Filled once in `reset()`, not state (commented as such). No action; keep them out of `WorldState`.
4. **Korean `kObjectLabel` strings in `sim/Objects.h`** ("경기 영역", "숨는 곳", "물리 물체", "아이템 상자", "규칙 구역", "규칙 타이머"). Project rule is English source text plus a Korean translation. Check whether object labels go through `tr()`; if so, switch to English and add the Korean to `strings/`.

## Structure

- `sim/`: one file per system, signatures listed in `sim/Systems.h`, order visible in `World::step()`.
- Largest files: `app/Flow.cpp` (398 lines, screen flow plus chat handling mixed) and `view/Juice.cpp` (356). Check whether more of `Flow.cpp` can move to the engine's `screens` module.
