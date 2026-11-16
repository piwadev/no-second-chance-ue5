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

bpy.ops.export_scene.gltf(filepath=out_path, export_format='GLB', export_animations=True,
                          export_animation_mode='NLA_TRACKS', export_yup=True, export_apply=False)
print('WROTE', out_path)
