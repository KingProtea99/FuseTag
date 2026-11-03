# FuseTag assets: sources and licenses

Everything under `assets/` and `art/` is made for this game from code:

| Files | Made by |
|---|---|
| `assets/models/**/*.glb` | `art/blender/make_models.py` in Blender 5.2 (`blender -b -P art/blender/make_models.py`); the `.blend` files next to it are its output, editable by hand |
| `assets/sounds/*.wav` | `art/sounds/make_sounds.py` (Python standard library, fixed seed) |
| `assets/effects/*.fx`, `assets/scenes/*.scene`, `assets/tables/*.table` | written by hand / the editor |

Exception: `assets/fonts/Pretendard-Medium.otf` is [Pretendard](https://github.com/orioncactus/pretendard) under the SIL Open Font License 1.1 (`assets/fonts/Pretendard-OFL.txt`, ship it next to the font).
