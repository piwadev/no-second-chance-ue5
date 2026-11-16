"""Blender (4.x) script: merge a Quaternius Universal Base Character with all Universal Animation Library
(1 + 2) clips into one GLB, so a single Unreal import gives skeletal mesh + skeleton + every animation.
blender -b -P merge_character.py -- <character.gltf> <UAL1.glb> <UAL2.glb> <out.glb>"""
import bpy, sys

argv = sys.argv[sys.argv.index('--') + 1:]
char_path, anim_paths, out_path = argv[0], argv[1:-1], argv[-1]

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=char_path)
char_objs = list(bpy.context.scene.objects)
arm = next(o for o in char_objs if o.type == 'ARMATURE')
arm.name = 'Soldier'
if arm.animation_data:
    arm.animation_data.action = None

for p in anim_paths:
    before = set(bpy.context.scene.objects)
    bpy.ops.import_scene.gltf(filepath=p)
    new = [o for o in bpy.context.scene.objects if o not in before]
    for o in new:
        bpy.data.objects.remove(o, do_unlink=True)

KEEP = {'Idle_Loop', 'Pistol_Idle_Loop', 'Walk_Loop', 'Jog_Fwd_Loop', 'Sprint_Loop', 'Crouch_Idle_Loop', 'Crouch_Fwd_Loop',
        'Death01', 'LayToIdle', 'Fixing_Kneeling', 'Walk_Carry_Loop', 'Push_Loop', 'OverhandThrow', 'Pistol_Reload',
        'Jump_Start', 'Jump_Loop', 'Jump_Land', 'Hit_Chest', 'Hit_Head', 'Hit_Knockback', 'Interact', 'Roll',
        'Pistol_Aim_Up', 'Pistol_Aim_Neutral', 'Pistol_Aim_Down', 'Pistol_Shoot', 'Sitting_Idle_Loop',
        'Zombie_Walk_Fwd_Loop', 'Idle_FoldArms_Loop', 'PickUp_Table', 'Consume'}
arm.animation_data_create()
names = []
for act in list(bpy.data.actions):
    clean = act.name.replace('_Armature', '')
    if clean not in KEEP or clean in names:
        bpy.data.actions.remove(act)
        continue
    act.name = clean
    act.use_fake_user = True
    tr = arm.animation_data.nla_tracks.new()
    tr.name = act.name
    st = tr.strips.new(act.name, int(act.frame_range[0]), act)
    tr.mute = True
    names.append(act.name)
print('ACTIONS', len(names), sorted(names))

# ---- uniform mask as vertex colours (the UBC body texture is bare skin): R = clothing, G = trousers, B = gloves/boots.
# M_NSC_Soldier in Unreal turns the masked areas into a team-coloured combat uniform.
def zone(bone):
    b = bone.lower()
    if b.startswith(('head', 'neck')): return (0.0, 0.0, 0.0)
    if b.startswith(('hand', 'index', 'middle', 'pinky', 'ring', 'thumb', 'foot', 'ball')): return (1.0, 0.0, 1.0)
    if b.startswith(('pelvis', 'thigh', 'calf')): return (1.0, 1.0, 0.0)
    return (1.0, 0.0, 0.0)
for o in [o for o in bpy.context.scene.objects if o.type == 'MESH']:
    me = o.data
    for i, ca in enumerate(list(me.color_attributes)):   # Unreal only reads COLOR_0: drop the source colour layers
        if not ca.name: ca.name = f'old_col_{i}'
    for n in [ca.name for ca in me.color_attributes]:
        me.attributes.remove(me.attributes[n])
    attr = me.color_attributes.new('Col', 'BYTE_COLOR', 'POINT')
    groups = {g.index: g.name for g in o.vertex_groups}
    for v in me.vertices:
        c = (0.0, 0.0, 0.0)
        if groups and v.groups:
            g = max(v.groups, key=lambda e: e.weight)
            c = zone(groups.get(g.group, 'Head'))
        attr.data[v.index].color = (c[0], c[1], c[2], 1.0)
    me.color_attributes.active_color = attr
    me.color_attributes.render_color_index = me.color_attributes.find('Col')

kw = dict(filepath=out_path, export_format='GLB', export_animations=True, export_animation_mode='NLA_TRACKS', export_yup=True, export_apply=False)
try:
    bpy.ops.export_scene.gltf(export_vertex_color='ACTIVE', **kw)
except TypeError:
    bpy.ops.export_scene.gltf(export_colors=True, **kw)
print('WROTE', out_path)
