# Engine notes

Things found while building FuseTag that belong to the engine (`D:\MealKit-Engine\MealKit`). Fix them there; mark here when done.

- [ ] `tools/new_project.ps1`: the default `-Path` is still `D:\GameEngine\MealKit` (the old engine location). It should default to the engine folder's parent or require `-Path`.
- [ ] `templates/empty`: `GameSession` doesn't pass `ctx.feedback.cameraShake()` to the camera or freeze the view during `hitStop()`, so a new empty-template project's `shake` and `hitStop` feedback columns do nothing (sandbox does both). FuseTag copied the sandbox code.
- [ ] Feedback rows named `Kind.suffix` all fire for `Kind`, and `players` only compares the event's `player` field. A reaction for the event's *other* player (the one who got tagged) has to be played by name from view code; a `playersFrom <field>` column would make it data.
- [ ] Game feel helpers live in FuseTag `view/Juice.*` (easing, springs, squash and stretch). If they stay small and generic, they're a v1.x engine candidate next to the engine ROADMAP "Visual feedback proposals" (event lights, hit flash and glow kick columns are in the engine since 2026-11-04; FuseTag still writes them as view code).
- [ ] Hidden information in lockstep: every peer holds the full state, so hiding/invisibility is only a rendering rule. Worth a line in the docs' genre guidance (hide-and-seek, fog of war).
- [ ] Model facing: the glTF import flips z (right-handed → left-handed), so a character built facing glTF +Z (Blender -Y, the usual export) faces -Z in the game, while yaw 0 is +Z. FuseTag turns the runner half way round in `view/Draw.cpp`. The docs should state which way models face after import (or the importer should keep glTF +Z as +Z forward).
