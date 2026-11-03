# FuseTag roadmap

Order of work. Each milestone ends with: `build.bat` + `mp_test.ps1 -Clients 3 -Ticks 900` PASS + scenario tests pass + a screen capture checked. Rules are in `docs/DESIGN.md`.

## M1. Core loop on a graybox map
- [ ] Tuning and state: per-slot fuse, it player, hold/immunity timers, eliminated flag
- [ ] Tag and swap, fuse burn and ramp, elimination, new tagger pick
- [ ] Round/match flow: last one standing ends the round, placement points, `rounds`
- [ ] Bots: chase and flee on the nav grid
- [ ] HUD: fuse bars, who's it, banner; results with points
- [ ] Scenario tests: swap, burn carry-over, elimination, round end

## M2. Hiding and props
- [ ] `HideSpot` areas, hide/reveal rules, tagger search
- [ ] Physics props and the shove

## M3. Items
- [ ] `tables/items.table`, `ItemCrate` respawn, item slot and use action
- [ ] The six items, HUD item slot

## M4. Map 1: Museum at night (first full map)
- [ ] Models, materials, lighting, lockers
- [ ] Blackout gimmick
- [ ] Sounds (footsteps, fuse hiss, tag, blast) and feedback table
- [ ] Lobby map choice, save records

## M5. Map 2: Snowed-in village
- [ ] Ice movement, snow piles, setting art

## M6. Map 3: Construction site
- [ ] Collapsing scaffolding, crane cargo, setting art

## M7. Ship
- [ ] Export, playtest over the internet (lag test `-Lag 120 -Jitter 30 -Loss 3`), balance pass
