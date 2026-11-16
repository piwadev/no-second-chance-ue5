"""Blender 4.x: build first-person arms from the merged soldier GLB.
Poses the body in a two-handed aim (UAL 'Pistol_Aim_Neutral'), makes that pose the rest pose, keeps only the
arms (upper arm to fingertips) and exports a small skinned GLB (SK_Arms in Unreal, no animation needed).
  blender -b -P Tools/blender/make_arms.py -- <Soldier_Male.glb> <out Arms.glb> [action] [frame fraction]"""
import bpy, sys
argv = sys.argv[sys.argv.index('--') + 1:]
src, out = argv[0], argv[1]
action_name = argv[2] if len(argv) > 2 else 'Pistol_Aim_Neutral'
frac = float(argv[3]) if len(argv) > 3 else 0.5

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=src)
arm = next(o for o in bpy.context.scene.objects if o.type == 'ARMATURE')
act = bpy.data.actions.get(action_name) or next(a for a in bpy.data.actions if action_name in a.name)
arm.animation_data_create()
for t in list(arm.animation_data.nla_tracks):
    arm.animation_data.nla_tracks.remove(t)
arm.animation_data.action = act
f0, f1 = act.frame_range
bpy.context.scene.frame_set(int(f0 + (f1 - f0) * frac))

KEEP = ('clavicle', 'upperarm', 'lowerarm', 'hand', 'index', 'middle', 'pinky', 'ring', 'thumb')
for o in [o for o in bpy.context.scene.objects if o.type == 'MESH']:
    if not o.vertex_groups or len(o.data.vertices) < 3000:   # hair, eyes, brows
        bpy.data.objects.remove(o, do_unlink=True); continue
    bpy.context.view_layer.objects.active = o
    # bake the pose into the mesh
    mod = next(m for m in o.modifiers if m.type == 'ARMATURE')
    o.modifiers.new('tmp', 'ARMATURE').object = arm
    bpy.ops.object.modifier_apply(modifier=mod.name)
    o.modifiers['tmp'].name = 'Armature'
    o.modifiers['Armature'].show_viewport = False
    names = {g.index: g.name.lower() for g in o.vertex_groups}
    import bmesh
    bm = bmesh.new(); bm.from_mesh(o.data)
    deform = bm.verts.layers.deform.active
    kill = []
    for v in bm.verts:
        w = v[deform]
        if not w: kill.append(v); continue
        g = max(w.items(), key=lambda kv: kv[1])[0]
        n = names.get(g, '')
        if not n.startswith(KEEP) or n.startswith('clavicle'):
            kill.append(v)
    bmesh.ops.delete(bm, geom=kill, context='VERTS')
    bm.to_mesh(o.data); bm.free()
    o.modifiers['Armature'].show_viewport = True

# the posed skeleton becomes the rest pose
bpy.context.view_layer.objects.active = arm
bpy.ops.object.mode_set(mode='POSE')
bpy.ops.pose.armature_apply(selected=False)
bpy.ops.object.mode_set(mode='OBJECT')
arm.animation_data.action = None
for a in list(bpy.data.actions):
    bpy.data.actions.remove(a)
arm.name = 'Arms'
bpy.ops.export_scene.gltf(filepath=out, export_format='GLB', export_animations=False, export_yup=True)
print('NSC_ARMS', out)
