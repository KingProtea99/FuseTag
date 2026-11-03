# Engine notes

Things found while building FuseTag that belong to the engine (`D:\MealKit-Engine\MealKit`). Fix them there; mark here when done.

- [ ] `tools/new_project.ps1`: the default `-Path` is still `D:\GameEngine\MealKit` (the old engine location). It should default to the engine folder's parent or require `-Path`.
- [ ] Hidden information in lockstep: every peer holds the full state, so hiding/invisibility is only a rendering rule. Worth a line in the docs' genre guidance (hide-and-seek, fog of war).
