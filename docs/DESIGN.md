# FuseTag design

An online tag game for 4–8 players (bots fill empty seats). Whoever is "it" carries a burning fuse; tagging someone hands it over. Run out of fuse and you're out. Last one standing wins.

Genre references: hot-potato tag where being caught swaps roles and the tagger's HP drains (mobile tag games), hide-and-chase in 3D. The rules, names, items and art here are our own.

## Core rules

- **Start**: everyone spawns at the map's spawn points; one random player (`Pcg32`, match seed) is it.
- **Tag**: it touches a runner (capsule overlap within `tagRange`) → roles swap on that tick.
  - The new tagger is held in place for `tagHold` (no instant tag-back), and the old tagger gets `escapeBoost` speed for a moment.
  - The old tagger can't be tagged by the new one for `tagBackImmunity`.
- **Fuse**: every player has a personal fuse (`fuseLength`, e.g. 60 s). It burns only while that player is it, and what's burned stays burned across turns.
  - After `fuseRampAfter` into a round the burn rate goes up by `fuseRampRate` so rounds don't drag.
- **Out**: fuse reaches 0 → that player blows up (ragdoll launch, then spectator camera). A new tagger is picked at random among the remaining players and is held for `tagHold`.
- **Round end**: one player left. Placement points by elimination order (last = most).
- **Match**: `rounds` rounds on the same map (map is chosen in the lobby); most points wins. Uses the engine's `MatchFlow` (`rounds`, scores).

## Hiding

- Each map places `HideSpot` areas (bushes, lockers, snow piles). A runner standing still inside one is hidden: not drawn and no name tag for others.
- Moving, using an item, or being tagged reveals you. It can't hide (it must chase), and a tagger standing at a hide spot's edge for `searchTime` pulls the hider out.
- **Known limit**: lockstep gives every peer the full state, so hiding is only a rendering rule. A modified client could see hidden players. Accepted for this game; noted in `docs/ENGINE_NOTES.md`.

## Items

Crates (`ItemCrate` scene objects) respawn on a timer; touching one gives a random item from `tables/items.table` (weight per row). One item slot; use with the item action.

| Item | Effect |
|---|---|
| Sprint shoes | Speed up for a few seconds |
| Snare | Drop a trap; whoever steps on it is stuck for a moment |
| Vanish | Invisible to others for a few seconds (still collides) |
| Scope | Briefly shows all hidden/invisible players to you |
| Coolant | Restores some fuse |
| Shove | Short-range push that knocks players and props over |

Physics props (crates, chairs, barrels) can be knocked over to block corridors.

## Maps

Every map has its own setting and one gimmick. All gimmick values are scene objects or tuning.

| Map | Setting | Gimmick |
|---|---|---|
| Museum at night | Closed museum halls, display cases | Lockers to hide in; periodic blackout (all lights out for a few seconds, only silhouettes of nearby players) |
| Snowed-in village | Small village street in snow | Ice patches (slippery movement), snow piles to hide in |
| Construction site | Half-built building, scaffolding | Scaffolding collapses when shoved, crane swings cargo across the yard (physics) |

## Camera and controls

- Everyone uses a third-person follow camera (roles swap too often for a split first/third person).
- Actions: `move`, `look`, `jump`, `sprint` (stamina), `item`, `crouch` (needed to hide in low spots).

## Screens

Shared engine flow (title → menu → lobby → loading → match → results → lobby). Game parts:

- Lobby: map choice (host), color.
- HUD: everyone's fuse bars, who's it, my item, round timer, "you're it!" banner.
- Results: placement per round and total points.
- Save: matches played, wins, total time survived.

## Bots

Bots fill empty seats and drive the tests. Built on the nav grid like the coop sample's chasers:

- It: chase the nearest visible runner; check hide spots it passes.
- Runner: keep distance from it, head for hide spots and crates, use items when it's close.

## Out of scope (later candidates)

- 2v2 teams (teammates can't be tagged)
- Joining mid-round as a spectator who enters next round (engine late join already exists)
