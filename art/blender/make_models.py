# FuseTag models, built from code in Blender (background mode) and exported as glTF to assets/models/.
#   "C:\Program Files\Blender Foundation\Blender 5.2\blender.exe" -b -P art/blender/make_models.py
# Saves each model's source next to this script as art/blender/<name>.blend (open and edit by hand; rerunning this script overwrites).
# Style (docs/ROADMAP.md M5): round chunky shapes for squash and stretch, flat white body tinted by the player color in the game,
# toon outline and rim light through MealKit material properties (mealkit_<key>, read as glTF extras).
# Blender is z-up with characters facing -Y; the glTF exporter turns that into y-up facing +Z, and MealKit's import flips z
# (left-handed), so in the game the model faces -Z. view/Draw.cpp turns the character half way round for that.
import math
import os

import bpy

HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.dirname(os.path.dirname(HERE))
MODELS = os.path.join(PROJECT, "assets", "models")
FPS = 30
OUTLINE = 0.02
OUTLINE_COLOR = [0.04, 0.04, 0.06]


def reset():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.context.scene.render.fps = FPS


def material(name, color, outline=OUTLINE, rim=0.0, roughness=0.8, emission=0.0):
    m = bpy.data.materials.new(name)
    m.use_backface_culling = True  # glTF single sided (MealKit culls back faces)
    m.use_nodes = True
    bsdf = m.node_tree.nodes.get("Principled BSDF")
    bsdf.inputs["Base Color"].default_value = (*color, 1.0)
    bsdf.inputs["Roughness"].default_value = roughness
    if emission > 0.0:
        bsdf.inputs["Emission Color"].default_value = (*color, 1.0)
        bsdf.inputs["Emission Strength"].default_value = emission
    if outline > 0.0:
        m["mealkit_outline"] = outline
        m["mealkit_outlineColor"] = OUTLINE_COLOR
    if rim > 0.0:
        m["mealkit_rim"] = rim
    return m


def shade_smooth(obj):
    for p in obj.data.polygons:
        p.use_smooth = True


def sphere(name, location, scale, mat, segments=24, rings=16):
    bpy.ops.mesh.primitive_uv_sphere_add(segments=segments, ring_count=rings, radius=1.0, location=location)
    obj = bpy.context.active_object
    obj.name = name
    obj.scale = scale
    bpy.ops.object.transform_apply(scale=True)
    obj.data.materials.append(mat)
    shade_smooth(obj)
    return obj


def cylinder(name, location, radius, depth, mat, rotation=(0, 0, 0), vertices=16):
    bpy.ops.mesh.primitive_cylinder_add(vertices=vertices, radius=radius, depth=depth, location=location, rotation=rotation)
    obj = bpy.context.active_object
    obj.name = name
    obj.data.materials.append(mat)
    shade_smooth(obj)
    return obj


def box(name, location, size, mat, bevel=0.0):
    bpy.ops.mesh.primitive_cube_add(size=1.0, location=location)
    obj = bpy.context.active_object
    obj.name = name
    obj.scale = size
    bpy.ops.object.transform_apply(scale=True)
    if bevel > 0.0:
        mod = obj.modifiers.new("bevel", "BEVEL")
        mod.width = bevel
        mod.segments = 3
        bpy.ops.object.modifier_apply(modifier=mod.name)
    obj.data.materials.append(mat)
    return obj


def join(objs, name):
    bpy.ops.object.select_all(action="DESELECT")
    for o in objs:
        o.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]
    bpy.ops.object.join()
    obj = bpy.context.active_object
    obj.name = name
    return obj


def export(name, objs, folder=""):
    bpy.ops.object.select_all(action="DESELECT")
    for o in objs:
        o.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]
    os.makedirs(MODELS, exist_ok=True)
    os.makedirs(os.path.join(MODELS, folder), exist_ok=True)
    path = os.path.join(MODELS, folder, name + ".glb")
    bpy.ops.export_scene.gltf(filepath=path, export_format="GLB", use_selection=True, export_extras=True, export_yup=True,
                              export_animation_mode="ACTIONS", export_skins=True, export_def_bones=False,
                              export_force_sampling=True, export_frame_step=1)
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(HERE, name + ".blend"))
    print("wrote", path)


# ----------------------------------------------------------------------------- the runner (every player)
# Bones: root → body (also the head: one round body), arm.L/R from the shoulders down, leg.L/R from the hips down.
BONES = {
    "root": ((0, 0, 0), (0, 0, 0.2), None),
    "body": ((0, 0, 0.3), (0, 0, 1.3), "root"),
    "arm.L": ((0.42, 0, 1.0), (0.56, 0, 0.66), "body"),
    "arm.R": ((-0.42, 0, 1.0), (-0.56, 0, 0.66), "body"),
    "leg.L": ((0.18, 0, 0.42), (0.18, 0, 0.05), "root"),
    "leg.R": ((-0.18, 0, 0.42), (-0.18, 0, 0.05), "root"),
}


def make_runner():
    reset()
    skin = material("body", (1.0, 1.0, 1.0), rim=0.3)
    eye = material("eye", (1.0, 1.0, 1.0), outline=0.0, emission=0.4)
    pupil = material("pupil", (0.03, 0.03, 0.05), outline=0.0)

    parts = {}  # bone → objects weighted fully to it
    parts["body"] = [
        sphere("body", (0, 0, 0.86), (0.5, 0.46, 0.58), skin),
        sphere("tuft", (0, 0.05, 1.47), (0.09, 0.09, 0.14), skin, 12, 8),
        sphere("eye.L", (0.17, -0.37, 1.1), (0.12, 0.08, 0.14), eye, 16, 10),
        sphere("eye.R", (-0.17, -0.37, 1.1), (0.12, 0.08, 0.14), eye, 16, 10),
        sphere("pupil.L", (0.17, -0.44, 1.09), (0.06, 0.03, 0.08), pupil, 12, 8),
        sphere("pupil.R", (-0.17, -0.44, 1.09), (0.06, 0.03, 0.08), pupil, 12, 8),
    ]
    parts["arm.L"] = [sphere("hand.L", (0.54, 0, 0.72), (0.12, 0.12, 0.16), skin, 16, 10)]
    parts["arm.R"] = [sphere("hand.R", (-0.54, 0, 0.72), (0.12, 0.12, 0.16), skin, 16, 10)]
    parts["leg.L"] = [sphere("foot.L", (0.18, -0.05, 0.09), (0.15, 0.2, 0.09), skin, 16, 10)]
    parts["leg.R"] = [sphere("foot.R", (-0.18, -0.05, 0.09), (0.15, 0.2, 0.09), skin, 16, 10)]

    # Armature
    bpy.ops.object.armature_add(enter_editmode=True, location=(0, 0, 0))
    arm = bpy.context.active_object
    arm.name = "runner_rig"
    ebones = arm.data.edit_bones
    ebones.remove(ebones[0])
    for name, (head, tail, parent) in BONES.items():
        b = ebones.new(name)
        b.head, b.tail = head, tail
        b.roll = 0.0
        if parent:
            b.parent = ebones[parent]
    bpy.ops.object.mode_set(mode="OBJECT")

    # Each part fully weighted to its bone, then one mesh
    meshes = []
    for bone, objs in parts.items():
        for o in objs:
            vg = o.vertex_groups.new(name=bone)
            vg.add(list(range(len(o.data.vertices))), 1.0, "REPLACE")
            meshes.append(o)
    mesh = join(meshes, "runner")
    mod = mesh.modifiers.new("rig", "ARMATURE")
    mod.object = arm
    mesh.parent = arm

    animate_runner(arm)
    export("runner", [arm, mesh], "characters")  # characters/: the engine's character budget (bones, materials)


def pose_key(arm, frame, poses):
    """poses: bone → (rx, ry, rz degrees, optional (lx, ly, lz) location)"""
    for name, p in poses.items():
        pb = arm.pose.bones[name]
        pb.rotation_mode = "XYZ"
        pb.rotation_euler = tuple(math.radians(a) for a in p[:3])
        pb.keyframe_insert("rotation_euler", frame=frame)
        if len(p) > 3:
            pb.location = p[3]
            pb.keyframe_insert("location", frame=frame)


def action(arm, name, frames):
    """frames: [(frame, poses)]; every bone not named in a pose stays at rest"""
    act = bpy.data.actions.new(name)
    act.use_fake_user = True
    arm.animation_data_create()
    arm.animation_data.action = act
    for pb in arm.pose.bones:  # rest keys first, so unnamed bones hold still
        pb.rotation_mode = "XYZ"
        pb.rotation_euler = (0, 0, 0)
        pb.location = (0, 0, 0)
        pb.keyframe_insert("rotation_euler", frame=frames[0][0])
        pb.keyframe_insert("location", frame=frames[0][0])
    for frame, poses in frames:
        pose_key(arm, frame, poses)
    bpy.context.scene.frame_start = int(frames[0][0])
    bpy.context.scene.frame_end = int(frames[-1][0])
    return act


def swing(amount, lift=0.0):
    """Run cycle poses: legs and arms swing on their local X (forward/back), arms opposite to legs"""
    a = amount
    return [
        (0, {"leg.L": (a, 0, 0), "leg.R": (-a, 0, 0), "arm.L": (-a * 1.2, 0, 0), "arm.R": (a * 1.2, 0, 0), "body": (0, 0, 0, (0, 0, 0))}),
        (5, {"body": (0, 0, 0, (0, lift, 0))}),
        (10, {"leg.L": (-a, 0, 0), "leg.R": (a, 0, 0), "arm.L": (a * 1.2, 0, 0), "arm.R": (-a * 1.2, 0, 0), "body": (0, 0, 0, (0, 0, 0))}),
        (15, {"body": (0, 0, 0, (0, lift, 0))}),
        (20, {"leg.L": (a, 0, 0), "leg.R": (-a, 0, 0), "arm.L": (-a * 1.2, 0, 0), "arm.R": (a * 1.2, 0, 0), "body": (0, 0, 0, (0, 0, 0))}),
    ]


def animate_runner(arm):
    bpy.context.view_layer.objects.active = arm
    bpy.ops.object.mode_set(mode="POSE")
    # Bone local Y runs along the bone; for arms/legs pointing down, local X swings forward/back and local Z sideways.
    action(arm, "idle", [
        (0, {"body": (0, 0, 0, (0, 0, 0)), "arm.L": (0, 0, 8), "arm.R": (0, 0, -8)}),
        (20, {"body": (0, 0, 0, (0, 0.03, 0)), "arm.L": (0, 0, 14), "arm.R": (0, 0, -14)}),
        (40, {"body": (0, 0, 0, (0, 0, 0)), "arm.L": (0, 0, 8), "arm.R": (0, 0, -8)}),
    ])
    action(arm, "run", swing(38, 0.05))
    sprint = swing(55, 0.08)
    for f, poses in sprint:
        poses.setdefault("body", (0, 0, 0, (0, 0, 0)))
        b = poses["body"]
        poses["body"] = (-14, 0, 0, b[3])  # lean into the sprint
    action(arm, "sprint", [(round(f * 0.7), p) for f, p in sprint])
    action(arm, "jump", [
        (0, {"leg.L": (0, 0, 0), "leg.R": (0, 0, 0), "arm.L": (0, 0, 0), "arm.R": (0, 0, 0)}),
        (6, {"leg.L": (35, 0, 0), "leg.R": (25, 0, 0), "arm.L": (0, 0, 120), "arm.R": (0, 0, -120)}),
    ])
    action(arm, "fall", [
        (0, {"arm.L": (0, 0, 130), "arm.R": (0, 0, -130), "leg.L": (15, 0, 0), "leg.R": (-15, 0, 0)}),
        (6, {"arm.L": (0, 0, 160), "arm.R": (0, 0, -100), "leg.L": (-15, 0, 0), "leg.R": (15, 0, 0)}),
        (12, {"arm.L": (0, 0, 130), "arm.R": (0, 0, -130), "leg.L": (15, 0, 0), "leg.R": (-15, 0, 0)}),
    ])
    action(arm, "fume", [  # held after being tagged: fists up and shaking
        (0, {"arm.L": (-60, 0, 40), "arm.R": (-60, 0, -40), "body": (0, 0, 0, (0, 0, 0))}),
        (3, {"arm.L": (-80, 0, 55), "arm.R": (-50, 0, -30), "body": (0, 0, 4, (0, 0, 0))}),
        (6, {"arm.L": (-60, 0, 40), "arm.R": (-60, 0, -40), "body": (0, 0, 0, (0, 0, 0))}),
        (9, {"arm.L": (-50, 0, 30), "arm.R": (-80, 0, -55), "body": (0, 0, -4, (0, 0, 0))}),
        (12, {"arm.L": (-60, 0, 40), "arm.R": (-60, 0, -40), "body": (0, 0, 0, (0, 0, 0))}),
    ])
    action(arm, "shove", [
        (0, {"arm.L": (30, 0, 0), "arm.R": (30, 0, 0), "body": (8, 0, 0, (0, 0, 0))}),
        (3, {"arm.L": (-95, 0, 10), "arm.R": (-95, 0, -10), "body": (-12, 0, 0, (0, 0, 0))}),
        (12, {"arm.L": (0, 0, 0), "arm.R": (0, 0, 0), "body": (0, 0, 0, (0, 0, 0))}),
    ])
    action(arm, "crouch", [  # hiding
        (0, {"body": (0, 0, 0, (0, 0, 0)), "leg.L": (0, 0, 0), "leg.R": (0, 0, 0), "arm.L": (0, 0, 0), "arm.R": (0, 0, 0)}),
        (6, {"body": (10, 0, 0, (0, -0.25, 0)), "leg.L": (40, 0, 0), "leg.R": (40, 0, 0), "arm.L": (-40, 0, -10), "arm.R": (-40, 0, 10)}),
    ])
    action(arm, "cheer", [
        (0, {"arm.L": (0, 0, 150), "arm.R": (0, 0, -150), "body": (0, 0, 0, (0, 0, 0))}),
        (6, {"arm.L": (0, 0, 170), "arm.R": (0, 0, -130), "body": (0, 0, 0, (0, 0.12, 0))}),
        (12, {"arm.L": (0, 0, 150), "arm.R": (0, 0, -150), "body": (0, 0, 0, (0, 0, 0))}),
        (18, {"arm.L": (0, 0, 130), "arm.R": (0, 0, -170), "body": (0, 0, 0, (0, 0.12, 0))}),
        (24, {"arm.L": (0, 0, 150), "arm.R": (0, 0, -150), "body": (0, 0, 0, (0, 0, 0))}),
    ])
    arm.animation_data.action = bpy.data.actions["idle"]
    bpy.ops.object.mode_set(mode="OBJECT")


# ----------------------------------------------------------------------------- props
def make_bomb():
    reset()
    black = material("bomb", (0.05, 0.05, 0.07), rim=0.5, roughness=0.35)
    rope = material("fuse", (0.8, 0.65, 0.4), outline=0.01)  # props budget: 2 materials (cap and fuse share)
    parts = [
        sphere("bomb", (0, 0, 0), (0.5, 0.5, 0.5), black),
        cylinder("cap", (0, 0, 0.5), 0.14, 0.12, rope),
        cylinder("fuse", (0.05, 0, 0.66), 0.035, 0.24, rope, rotation=(0, math.radians(20), 0), vertices=8),
    ]
    export("bomb", [join(parts, "bomb")])


def make_item_crate():
    reset()
    gold = material("crate", (1.0, 0.75, 0.15), rim=0.4, roughness=0.5)
    mark = material("mark", (1.0, 1.0, 1.0), outline=0.0, emission=0.3)
    parts = [box("crate", (0, 0, 0), (1, 1, 1), gold, bevel=0.12)]
    for i, (axis, loc) in enumerate([((0, 0, 0), (0, -0.51, 0.08)), ((0, 0, 0), (0, 0.51, 0.08)),
                                     ((0, 0, math.pi / 2), (0.51, 0, 0.08)), ((0, 0, math.pi / 2), (-0.51, 0, 0.08))]):
        q = box(f"q{i}", loc, (0.2, 0.02, 0.36), mark)  # a bold "!" on every side
        q.rotation_euler = axis
        dot = box(f"d{i}", (loc[0], loc[1], -0.26), (0.2, 0.02, 0.14), mark)
        dot.rotation_euler = axis
        parts += [q, dot]
    export("item_crate", [join(parts, "item_crate")])


def make_bush():
    reset()
    leaf = material("leaf", (0.3, 0.62, 0.28), rim=0.2)
    dark = material("leafDark", (0.2, 0.46, 0.22), rim=0.2)
    blobs = [((0, 0, 0.45), (0.5, 0.5, 0.45), leaf), ((0.28, 0.12, 0.38), (0.34, 0.32, 0.34), dark),
             ((-0.26, -0.1, 0.36), (0.36, 0.34, 0.33), dark), ((0.05, -0.28, 0.62), (0.3, 0.28, 0.28), leaf),
             ((-0.1, 0.25, 0.68), (0.28, 0.26, 0.26), leaf)]
    parts = [sphere(f"b{i}", loc, sc, m, 16, 10) for i, (loc, sc, m) in enumerate(blobs)]
    export("bush", [join(parts, "bush")])  # unit footprint about 1 m, height 0.95 m: scaled to the hide spot


def make_locker():
    reset()
    metal = material("locker", (0.48, 0.55, 0.66), rim=0.2, roughness=0.45)
    slot = material("slots", (0.12, 0.13, 0.16), outline=0.0)
    parts = [box("locker", (0, 0, 0.5), (1, 1, 1), metal, bevel=0.03)]
    for i in range(4):
        parts.append(box(f"vent{i}", (0, -0.505, 0.82 - i * 0.05), (0.5, 0.01, 0.018), slot))
    parts.append(box("handle", (0.3, -0.51, 0.5), (0.04, 0.02, 0.12), slot))
    export("locker", [join(parts, "locker")])


def make_pile():
    reset()
    snow = material("pile", (0.93, 0.95, 1.0), rim=0.35, roughness=0.9)
    blobs = [((0, 0, 0.2), (0.5, 0.5, 0.35)), ((0.25, 0.1, 0.25), (0.3, 0.3, 0.3)), ((-0.2, -0.15, 0.28), (0.3, 0.3, 0.32))]
    parts = [sphere(f"p{i}", loc, sc, snow, 16, 10) for i, (loc, sc) in enumerate(blobs)]
    export("pile", [join(parts, "pile")])


make_runner()
make_bomb()
make_item_crate()
make_bush()
make_locker()
make_pile()
