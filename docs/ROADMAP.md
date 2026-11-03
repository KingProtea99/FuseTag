# FuseTag roadmap

Order of work for the first shippable version. Rules are in `docs/DESIGN.md`; engine problems found on the way go in `docs/ENGINE_NOTES.md`.

## How to work

- **Done means**: `build.bat` OK, `mp_test.ps1 -Clients 3 -Ticks 900` PASS (and `-Headless -Clients 3 -Ticks 3000` for rule changes), scenario tests in `tests/` pass, and a screen capture checked by eye. A milestone is done when all its items are and a short play session with bots feels right.
- **Commits**: one local commit per item. Changing `sim/`, tuning, scenes or tables changes replays and scenarios: re-record and update them in the same commit.
- **Where things go** (engine `docs/CODING_MODEL.md` "Where to put numbers"):
  - Rules and anything that decides who wins → `sim/` + `Tuning` (`assets/game.tuning`). Deterministic, in the content hash.
  - Per-map values (areas, timers of a map gimmick) → scene objects (`sim/Objects.h`).
  - **All game feel and visuals** (squash and stretch, springs, camera kicks, lights, colors, particle counts) → `view/` + `ViewTuning` (`assets/view.tuning`) or `assets/feedback.table`. View-only, not in the hash, tweakable live during multiplayer. The simulation only reports *what happened* through `world.events`; it never waits on an animation.
- **Engine is frozen for v1.0** (fix-only since v0.9). Game feel is built in `view/` with what the renderer already exposes: feedback table (sound, shake, flash, hit stop, particles), `render::instanceFlash`, view-side `PointLight`s in `FrameParams::lights`, instance transforms (non-uniform scale), material rim/emissive/dissolve, Toon + outline, post (exposure, bloom, vignette, chromatic aberration, LUT), camera fov. Anything that turns out generally useful (tweens, squash and stretch, event lights, hit flash rows) is written up in `ENGINE_NOTES.md` as a v1.x candidate, matching the engine's `docs/VISUAL_FEEDBACK.md` draft.
- **Budget**: minimum spec is Intel 11th-gen Xe at 1280×720. Every visual milestone ends with `FuseTag.exe --benchmark 30` on the busiest scene: ≥ 60 fps, GPU within the project budget, ≤ 4 point lights per object.
- **Accessibility**: every shake, flash, hit stop and strobe goes through the feedback table or reads the player settings (`gameplay.shakeScale`, `flashScale`, `hitStop`), so the standard settings screen turns them down. Blackout and low-fuse pulses stay under 3 big brightness changes per second.

---

## M1. Core loop on a graybox map

Goal: 4–8 capsules (players + bots) on a box-and-ramp test map, playing complete rounds of fuse tag.

**State and tuning**
- [x] `PlayerSlot`: `fuse` (ticks left), `isIt`, `holdUntil`, `immuneFrom`/`immuneUntil`, `eliminated`, `outOrder`, `roundPoints`. `WorldState`: `itSlot`, `roundStartTick`.
- [x] `Tuning`: `fuseLength`, `fuseRampAfter`, `fuseRampRate`, `tagRange`, `tagHold`, `tagBackImmunity`, `escapeBoost`, `escapeBoostTime`, `itSpeedScale`, `placementPoints[8]`, `rounds`.
- [x] Test map `scenes/graybox.scene`: floor, walls, ramps, a few pillars to circle around, 8 spawn points.

**Rules (`sim/`, one file per system)**
- [x] `sim/Tag.cpp`: overlap check it → nearest runner within `tagRange` (fixed slot order on ties), swap, hold, immunity, boost.
- [x] `sim/Fuse.cpp`: burn the it player's fuse each tick, ramp after `fuseRampAfter`; at 0 → eliminate, pick a new it with `Pcg32` among the alive, hold them.
- [x] Eliminated players: capsule launched up and removed after `blastRemove`; slot becomes a spectator. Ragdoll launch moves to M2 with the explosion.
- [x] `sim/Match.cpp`: round ends when one is left; placement points by `outOrder`; `MatchFlow` scores; next round resets fuses and positions.
- [x] Sprint with stamina (`sprintSpeed`, `staminaMax`, `staminaRegen`) so chases have rhythm.

**Events (the contract the feel milestones build on)**
- [x] `GameEvent` kinds: `Tag` (from, to, position), `HoldEnd`, `FuseLow` (slot, fraction; at 30% and 10%), `FuseOut`, `Eliminated` (slot, position, launch velocity), `NewIt`, `Jump`, `Land` (fall speed), `SprintStart`, `StaminaEmpty`, `RoundWin` (slot).

**Bots (`sim/Bots.cpp`, nav grid like the coop sample's chasers)**
- [x] It: path to the nearest runner, re-target every N ticks, give up on someone out of reach.
- [x] Runner: flee from it (pick the nav cell farthest from it within a radius), circle pillars, sprint when close.
- [x] Bots get a small reaction delay (`botReaction`) so humans can win.

**Screens (bare)**
- [x] HUD: fuse bar per player (color, name), "it" marker, round timer, "You're it!" text. World-space name tags.
- [x] Results: placement per round, total points (`FlowGame` `score`).
- [x] Debug UI (F1): make it, refill/burn fuse, eliminate, bot goals, bot paths (cheats are confirmed commands)

**Tests**
- [x] `tests/round_flow.scenario`, `tests/fuse_rules.scenario` (bot match: swaps, burn, placement points, rounds)
- [x] Focused scenarios on the `scenes/tests/cage.scene` test arena: `tag_rules` (hold, swap, immunity, burn carry-over), `elimination` (out order, ramp, placement)
- [x] Replay `replays/graybox_3p_seed7.rec`, `mp_test` with lag `-Lag 120 -Jitter 30 -Loss 3`.

---

## M2. Game feel pass 1: characters and the bomb

Goal: with graybox art still, a muted video of a round reads clearly: who is it, how close they are to blowing up, when a tag lands. All view-side.

**Juice toolkit (`view/Juice.h`, `view/Juice.cpp`)**
- [x] Easing functions (out-back, out-elastic, in-out-sine, …) and a critically damped / underdamped `Spring` (value, velocity, frequency, damping) on view time.
- [x] Per-pawn view state kept between frames: scale spring (x/z and y separately), lean angle spring, bounce offset, last-frame velocity. Driven by events and by interpolated sim state; never fed back into `sim/`.
- [x] Respect hit stop (view time freezes) and pause.
- [x] Values in `ViewTuning` (`juice.*`) so they are tuned live from the [Tuning] panel.

**Squash and stretch**
- [x] Jump: anticipation squash (0.06 s) → stretch on takeoff along velocity → settle.
- [x] Land: squash scaled by fall speed (`Land` event), with a small dust puff.
- [x] Run: subtle bob and stretch along speed; volume preserved (x·y·z ≈ 1).
- [x] Turning: lean into the turn from angular velocity; sharp stop leans back.
- [x] Tagged: big squash + spin wobble on the new it; old it pops up with a stretch
- [x] Afterimages behind the old it during the escape boost
- [x] Hold (new it can't move): shaking "fuming" squash loop until `HoldEnd`, then a launch stretch.

**The bomb (the main readability tool)**
- [x] The it player carries a bomb model on their head with a burning fuse (spark particles at the tip, short point light).
- [x] Bomb pulses (scale spring + emissive) at a rate that rises as the fuse burns; tick sound pitch rises with it (`soundParam` from fuse fraction).
- [x] On `Tag`: the bomb flies from the old it to the new one in an arc (0.15–0.2 s, out-back ease), landing with a squash on both.
- [x] Low fuse: bomb flashes (`instanceFlash`), sparks speed up, red screen flash at `FuseDanger`
- [ ] Alarm beep and fuse hiss (no sounds yet: M5)

**Tag impact**
- [x] Feedback rows for `Tag`: hit stop (0.06 s, local players involved only), camera shake by distance, impact sound, star burst particles.
- [x] Both players flash white (`instanceFlash`) for 0.1 s; short point light at the contact point.
- [x] Camera punch: brief fov kick and a small push toward the hit for the two involved.
- [x] Spinning marker popping over the new it (out-back scale, bounce, fade). A text "IT!" popup waits for world-space UI text in M5

**Explosion (elimination)**
- [x] `Eliminated`: flash for the one out, shake by distance, blast and smoke particles, point light, bloom kick
- [ ] Boom sound (M5)
- [x] Blown body: launched, dark, tumbles and shrinks away before removal (view side)
- [ ] Ragdoll launch and dissolve edge need real character models (M5)
- [x] Eliminated: camera holds on the blast while the body flies, then follows the it player

**Local danger cues**
- [x] When I'm it: red vignette that pulses with the bomb, heartbeat that speeds up below 30% fuse, chromatic aberration kick on each pulse below 10% (scaled by flash setting).
- [x] When it is near me: arrow on the ground toward it, bigger and brighter as it gets closer
- [ ] Footsteps (M5 sounds)

**Camera**
- [x] Third-person spring-arm camera with wall avoidance (engine virtual cameras), fov widens slightly with speed and sprint.
- [x] Landing camera dip
- [ ] Shove recoil (with the shove in M3)

**Done check**: record a 3-minute bot round with `--capture` frames; every tag and explosion is visible without sound.

---

## M3. Hiding and props

**Rules**
- [ ] `HideSpot` scene object (area, kind: bush/locker/pile), hide when still inside, reveal on move/item/tag, tagger search after `searchTime`.
- [ ] Physics props (crates, chairs, barrels, carts) with mass from a table; shove action (cooldown) that pushes players and props.
- [ ] Bots: runners use hide spots when it is far, it checks spots it passes.
- [ ] Scenarios: hide/reveal, search pull-out, shove knockback.

**Feel**
- [ ] Entering a hide spot: bush wobble spring (scale + rotation), leaves particles, rustle sound; locker door slam with squash on the door.
- [ ] While hidden: my own character drawn faded with an outline so I know I'm hidden; a calm breathing loop.
- [ ] Pulled out: "found!" popup, hider pops out with a stretch, bush bursts leaves.
- [ ] Props: impact sound and dust scaled by impulse; small hit stop when a shove hits a player; tumbling props squash a little on landing.
- [ ] Shove: wind-up squash, swoosh arc particles, recoil.

---

## M4. Items

**Rules**
- [ ] `tables/items.table` (id, weight, duration, strength, icon), `ItemCrate` scene object with respawn timer, one item slot, `item` action.
- [ ] Sprint shoes, Snare, Vanish, Scope, Coolant, Shove+ (bigger shove). Each with a scenario test.
- [ ] Bots use items with simple rules (Coolant below 40% fuse, Snare when chased, etc.).

**Feel**
- [ ] Crate: idle bob and spin, glint; on pickup it bursts into confetti, the item icon flies into the HUD slot with an arc and the slot bounces.
- [ ] Item use: each item has a cast squash, a particle signature and a sound; HUD slot shakes when empty-pressed.
- [ ] Sprint shoes: speed lines, trailing afterimages (fading copies of the character drawn a few frames behind), fov kick.
- [ ] Snare: trap snaps shut with a hit stop and squash on the victim; struggle wobble while stuck.
- [ ] Vanish: dissolve out/in; others see a faint shimmer only when very close.
- [ ] Scope: screen pulse ring, hidden players outlined for the duration.
- [ ] Coolant: blue steam burst from the bomb, fuse bar fills with a bounce.

---

## M5. Art direction and characters

Goal: replace capsules and boxes with the final look. Decided before maps so all three maps share it.

- [ ] Style sheet (`art/style/`): Toon lighting with outline, chunky rounded characters, bright player colors on muted environments, one accent color for "danger" (bomb, fuse, it marker).
- [ ] Character model (Blender, glTF): round body, big eyes, short limbs so squash and stretch read well; 8 color variants; simple face states (normal, scared when it is near, smug after a tag, panicked when it).
- [ ] Animations (engine animation states): idle, run, sprint, jump, fall, land, hold-fuming, carry-bomb overlay, shove, hide, celebrate, eliminated pose. Squash and stretch from M2 is layered on top.
- [ ] Bomb, crate, item models; particle textures (spark, smoke, star, leaf, snow, dust).
- [ ] UI style: chunky rounded panels, bold numbers, fuse bars that look like burning fuses; Penpot sources in `art/ui/`.
- [ ] Sounds: footsteps per surface, fuse hiss, tag, boom, UI clicks; a short upbeat match loop and a tense last-two loop (switch on `NewIt` when two are left).
- [ ] Benchmark with 8 characters + all effects on minimum spec.

---

## M6. Map 1: Museum at night

- [ ] Layout: a loop of galleries around a central atrium, side corridors, two floors joined by stairs; sightlines broken by display cases. Graybox first, play with bots, then art.
- [ ] Hide spots: lockers (staff rooms), curtains, large display crates.
- [ ] **Blackout gimmick** (`Blackout` scene object: interval, duration): sim decides the timing (event `BlackoutStart`/`End`); view drops scene lights and exposure, emergency lights stay as red point lights; characters keep a faint rim so nearby players are silhouettes; power-down and power-up sounds with lights flickering back (staggered).
- [ ] Atmosphere: moonlight through windows (sun with cool LUT), dust motes, glowing exhibit cases (emissive), polished floor reflections from probes.
- [ ] Lobby: map choice by host (`FlowGame` choices), map preview image.
- [ ] Save records: matches, wins, best survival time.

## M7. Map 2: Snowed-in village

- [ ] Layout: a main street with houses, alleys between them, a frozen pond in the square.
- [ ] **Ice**: `IcePatch` areas switch the movement tuning (low friction, slow turning); view: sliding pose, stretched along velocity, spray of ice shards when braking.
- [ ] Snow piles to hide in (pop-out burst of snow), footstep puffs, falling snow particles near the camera only.
- [ ] Warm windows (emissive + point lights) against a cold blue LUT.

## M8. Map 3: Construction site

- [ ] Layout: half-built building with scaffolding levels, ladders, a yard with stacked materials.
- [ ] **Collapsing scaffolding** (`Scaffold` objects): a shove or prop impact over a threshold drops planks (physics, sim); dust burst, creak warning sound before it gives way.
- [ ] **Crane cargo** (`Crane` object: path, period): a swinging load that knocks players over; shadow marker on the ground ahead of it.
- [ ] Hide spots: concrete pipes, tarps, portable toilets (door slam).
- [ ] Warm sunset LUT, light shafts faked with additive cards.

---

## M9. Screens polish

- [ ] Title: characters running around a bomb in the background (a small bot match in the menu scene).
- [ ] Lobby: characters standing on a stage, ready pop and wave animation, color picker.
- [ ] Round end: winner's camera with confetti and the celebrate animation; others' faces in the corner.
- [ ] Results: podium with characters, points counting up with ticks, confetti for first place.
- [ ] Transitions: bomb-shaped wipe between screens; countdown "3, 2, 1, RUN!" with scale pops.

## M10. Ship

- [ ] Balance pass with recorded bot matches (round length, fuse length, item weights) and real playtests.
- [ ] Network: 8 players over the internet, `mp_test` with lag/jitter/loss, late join to spectator.
- [ ] Performance: benchmark every map on minimum spec, trim particles/lights to budget.
- [ ] `build.bat Export`, `check_layout.ps1`, run the exported build on a clean PC.
- [ ] Store/readme text, screenshots and a short gameplay clip captured from the game.

---

## Later candidates

- 2v2 teams; more maps; cosmetic hats unlocked by save records.
- Engine v1.x (see `ENGINE_NOTES.md`): feedback table event lights, hit flash and glow kick columns; a shared squash-and-stretch / spring helper; decals for scorch marks and footprints; screen distortion for the explosion shockwave.
