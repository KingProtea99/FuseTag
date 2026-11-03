# Engine notes

Things found while building FuseTag that belong to the engine (`D:\MealKit-Engine\MealKit`). Fix them there; mark here when done.

- [ ] `tools/new_project.ps1`: the default `-Path` is still `D:\GameEngine\MealKit` (the old engine location). It should default to the engine folder's parent or require `-Path`.
- [ ] Game feel helpers live in FuseTag `view/Juice.*` (easing, springs, squash and stretch). If they stay small and generic, they're a v1.x engine candidate next to the `docs/VISUAL_FEEDBACK.md` draft (event lights, hit flash, glow kick columns), which FuseTag currently writes as view code.
- [ ] Hidden information in lockstep: every peer holds the full state, so hiding/invisibility is only a rendering rule. Worth a line in the docs' genre guidance (hide-and-seek, fog of war).
