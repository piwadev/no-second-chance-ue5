"""Imports SourceAssets/ (see SourceAssets/README.md) into /Game/NSC with the names the C++ code loads."""
import glob
import json
import os
import unreal
from nsc_common import (EAL, ROOT, assets_in, exists, import_files, load, log, move, src, warn, obj_path)
import nsc_materials as M

TEX = ROOT + '/Textures'


# ------------------------------------------------------------------------------------------------ textures
def import_texture(path, dest, name, kind='color', flip_green=False):
    """kind: color | normal | masks | hdr"""
    target = dest + '/' + name
    paths = import_files([path], dest, name)
    tex = load(target)
    if tex is None:
        cands = [unreal.load_asset(p) for p in paths]
        cands = [c for c in cands if isinstance(c, unreal.Texture)]
        if not cands:
            warn('texture import failed: ' + path)
            return None
        tex = move(cands[0], target)
    if kind == 'normal':
        tex.set_editor_property('srgb', False)
        tex.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_NORMALMAP)
        tex.set_editor_property('flip_green_channel', flip_green)
    elif kind == 'masks':
        tex.set_editor_property('srgb', False)
        tex.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_MASKS)
    EAL.save_loaded_asset(tex)
    return tex


def import_surfaces():
    """Poly Haven surfaces -> T_<id>_BC / _N / _ORM (arm = AO, roughness, metal = ORM; normals are OpenGL)."""
    out = {}
    for d in sorted(glob.glob(src('Textures', 'PolyHaven', '*'))):
        sid = os.path.basename(d)
        def find(tag):
            m = glob.glob(os.path.join(d, '*_%s_*.*' % tag))
            return m[0] if m else None
        dest = TEX + '/' + sid
        entry = {}
        if find('diff'):
            entry['bc'] = import_texture(find('diff'), dest, 'T_%s_BC' % sid, 'color')
        if find('nor_gl'):
            entry['n'] = import_texture(find('nor_gl'), dest, 'T_%s_N' % sid, 'normal', flip_green=True)
        if find('arm'):
            entry['orm'] = import_texture(find('arm'), dest, 'T_%s_ORM' % sid, 'masks')
        out[sid] = entry
    log('surfaces: %d' % len(out))
    return out


def import_decals():
    res = {}
    for f in sorted(glob.glob(src('Decals', '*.png'))):
        name = os.path.splitext(os.path.basename(f))[0]
        res[name] = import_texture(f, TEX + '/Decals', name, 'color')
    for f in sorted(glob.glob(src('FX', '*.png'))):
        name = os.path.splitext(os.path.basename(f))[0].replace(' ', '_')
        import_texture(f, ROOT + '/FX/Textures', name, 'color')
    return {'bullethole': res.get('T_BulletHole_01'), 'blood': res.get('T_Blood_01')}


def import_hdri():
    res = {}
    for f in sorted(glob.glob(src('HDRI', '*.hdr'))):
        name = 'HDR_' + os.path.splitext(os.path.basename(f))[0]
        paths = import_files([f], ROOT + '/HDRI', name)
        res[name] = load(ROOT + '/HDRI/' + name) or (unreal.load_asset(paths[0]) if paths else None)
    return res


# ------------------------------------------------------------------------------------------------ meshes
def import_static_glb(path, folder, sm_name):
    """Import a single-mesh GLB into <folder>/ and make sure the static mesh is called <sm_name>."""
    target = folder + '/' + sm_name
    paths = import_files([path], folder, sm_name)
    sm = load(target)
    if sm is None:
        meshes = [unreal.load_asset(p) for p in paths]
        meshes = [m for m in meshes if isinstance(m, unreal.StaticMesh)] or assets_in(folder, unreal.StaticMesh, recursive=False)
        if not meshes:
            warn('no static mesh from ' + path)
            return None
        sm = move(meshes[0], target)
    return sm


def set_all_materials(mesh, mat):
    if isinstance(mesh, unreal.StaticMesh):
        for i in range(len(mesh.get_editor_property('static_materials'))):
            mesh.set_material(i, mat)
    EAL.save_loaded_asset(mesh)


def check_weapon_axes(sm, wid):
    """Normalised weapons have the barrel on +X (origin 30% from the rear) and the top on +Z (origin 30% below it)."""
    try:
        box = sm.get_bounding_box()
        mn, mx = box.min, box.max
        ext = [mx.x - mn.x, mx.y - mn.y, mx.z - mn.z]
        if wid.startswith('Grenade') or wid == 'Charge':
            return
        if not (ext[0] >= ext[1] and ext[0] >= ext[2] and mx.x > -mn.x and mx.z < -mn.z):
            warn('%s: unexpected orientation after import (bounds %s .. %s) - adjust MeshRotation in DT_Weapons' % (wid, mn, mx))
    except Exception as e:
        warn('bounds check %s: %s' % (wid, e))


def import_weapons():
    res = {}
    for d in sorted(glob.glob(src('Weapons', '*'))):
        wid = os.path.basename(d)
        glb = os.path.join(d, 'SM_%s.glb' % wid)
        if not os.path.exists(glb):
            continue
        sm = import_static_glb(glb, ROOT + '/Weapons/' + wid, 'SM_' + wid)
        if sm is None:
            continue
        sm = move(sm, ROOT + '/Weapons/SM_' + wid)    # the C++ loads /Game/NSC/Weapons/SM_<Id>
        sm.set_editor_property('light_map_resolution', 64)
        check_weapon_axes(sm, wid)
        EAL.save_loaded_asset(sm)
        res[wid] = sm
    log('weapons: ' + ', '.join(res))
    return res


GEAR_COLORS = {'Helmet': (0.09, 0.1, 0.075, 1), 'PlateCarrier': (0.16, 0.15, 0.11, 1)}
# 3dassets.dev props ship untextured: give them a sensible surface (map key -> (surface id, tint))
UNTEXTURED = {
    'Pallet': ('weathered_planks', (1, 1, 1)), 'PalletJack': ('painted_metal_shutter', (1.0, 0.55, 0.15)),
    'ShippingContainer': ('rusty_corrugated_iron', (1, 1, 1)), 'Forklift': ('painted_metal_shutter', (1.0, 0.7, 0.1)),
    'Bollard': ('rusty_painted_metal', (1, 1, 1)), 'DockCrane': ('rusty_painted_metal', (1.0, 0.75, 0.3)),
    'Ladder': ('metal_plate', (1, 1, 1)), 'Catwalk': ('metal_grate_rusty', (1, 1, 1)), 'RollerDoor': ('rusted_shutter', (1, 1, 1)),
    'SandbagWall': ('denim_fabric', (0.75, 0.65, 0.45)), 'ConcreteBarrier': ('rough_concrete', (1, 1, 1)),
    'IBCTank': ('metal_plate_02', (1, 1, 1)), 'Workbench': ('plywood', (1, 1, 1)), 'Locker': ('green_metal_rust', (1, 1, 1)),
    'OfficeDesk': ('metal_plate', (0.6, 0.6, 0.6)), 'Generator': ('green_metal_rust', (1, 1, 1)), 'FloodLight': ('metal_plate', (1, 1, 1)),
    'CableReel': ('plywood', (1, 1, 1)), 'AmmoCrate': ('green_metal_rust', (1, 1, 1)), 'Radio': ('metal_plate', (0.3, 0.32, 0.28)),
    'SecurityCamera': ('metal_plate', (0.9, 0.9, 0.9)), 'Backpack': ('denim_fabric', (0.45, 0.42, 0.3)),
}
SOLID = {'Medkit': (0.55, 0.05, 0.04, 1), 'Bandage': (0.85, 0.83, 0.78, 1), 'Tourniquet': (0.05, 0.05, 0.05, 1),
         'DefuseKit': (0.08, 0.08, 0.09, 1), 'BreachingCharge': (0.3, 0.27, 0.18, 1), 'Magazine': (0.04, 0.04, 0.04, 1),
         'RedDot': (0.03, 0.03, 0.03, 1), 'Scope': (0.03, 0.03, 0.03, 1), 'Suppressor': (0.04, 0.04, 0.04, 1),
         'Foregrip': (0.04, 0.04, 0.04, 1), 'Flashlight': (0.04, 0.04, 0.04, 1)}


def import_props(mats, surfaces):
    """Baked props (origin bottom-centre) -> /Game/NSC/Props/<Name>/SM_<Name>; gear -> /Game/NSC/Characters/Gear."""
    res = {}
    for f in sorted(glob.glob(src('Props', 'Baked', 'SM_*.glb'))):
        name = os.path.splitext(os.path.basename(f))[0][3:]
        folder = ROOT + '/Props/' + name
        sm = import_static_glb(f, folder, 'SM_' + name)
        if sm is None:
            continue
        if name in UNTEXTURED:
            sid, tint = UNTEXTURED[name]
            s = surfaces.get(sid, {})
            mi = M.make_mi('MI_Prop_' + name, mats['surface'], scalars={'TileSize': 120.0},
                           vectors={'Tint': tint + (1,)}, textures={'T_BaseColor': s.get('bc'), 'T_Normal': s.get('n'), 'T_ORM': s.get('orm')})
            set_all_materials(sm, mi)
        elif name in SOLID or name in GEAR_COLORS:
            mi = M.make_mi('MI_Prop_' + name, mats['solid'], scalars={'Roughness': 0.75},
                           vectors={'BaseColor': (SOLID.get(name) or GEAR_COLORS[name])})
            set_all_materials(sm, mi)
        res[name] = sm
    # gear the character attaches (C++: /Game/NSC/Characters/Gear/SM_Helmet, SM_PlateCarrier)
    for name in ('Helmet', 'PlateCarrier'):
        f = src('Props', 'Baked', 'SM_%s.glb' % name)
        if os.path.exists(f):
            sm = import_static_glb(f, ROOT + '/Characters/Gear', 'SM_' + name)
            if sm:
                sm.set_editor_property('light_map_resolution', 32)
                mi = M.make_mi('MI_Gear_' + name, mats['solid'], scalars={'Roughness': 0.8}, vectors={'BaseColor': GEAR_COLORS[name]})
                set_all_materials(sm, mi)
                body = sm.get_editor_property('body_setup')
                if body:
                    body.set_editor_property('collision_trace_flag', unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AS_COMPLEX)
    sizes_path = src('Props', 'Baked', 'sizes.json')
    log('props: %d' % len(res))
    return res


# ------------------------------------------------------------------------------------------------ characters
ANIM_MAP = {
    'A_Idle': 'Idle_Loop', 'A_PistolIdle': 'Pistol_Idle_Loop', 'A_Walk': 'Walk_Loop', 'A_Jog': 'Jog_Fwd_Loop',
    'A_Sprint': 'Sprint_Loop', 'A_CrouchIdle': 'Crouch_Idle_Loop', 'A_CrouchWalk': 'Crouch_Fwd_Loop',
    'A_Downed': 'Sitting_Idle_Loop', 'A_Crawl': 'Crouch_Fwd_Loop', 'A_Death': 'Death01', 'A_Revive': 'Fixing_Kneeling',
    'A_Drag': 'Walk_Carry_Loop', 'A_Throw': 'OverhandThrow', 'A_Reload': 'Pistol_Reload', 'A_Jump': 'Jump_Loop',
    'A_Aim': 'Pistol_Aim_Neutral', 'A_Shoot': 'Pistol_Shoot', 'A_HitChest': 'Hit_Chest', 'A_HitHead': 'Hit_Head',
    'A_Interact': 'Interact', 'A_GetUp': 'LayToIdle',
}


def soldier_material(mats, folder, mi_name):
    """MI of M_NSC_Soldier using the body textures the glTF import created in <folder>."""
    texs = assets_in(folder, unreal.Texture)
    def pick(*keys):
        for t in texs:
            n = t.get_name().lower()
            if all(k in n for k in keys):
                return t
        return None
    bc = pick('superhero', 'dark') or pick('superhero') or pick('basecolor')
    nm = pick('superhero', 'normal') or pick('normal')
    if nm:
        nm.set_editor_property('srgb', False)
        nm.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_NORMALMAP)
    return M.make_mi(mi_name, mats['soldier'], textures={'T_BaseColor': bc, 'T_Normal': nm})


def retarget_body_slots(skm, mi):
    mats = skm.get_editor_property('materials')
    changed = False
    for sm in mats:
        slot = str(sm.get_editor_property('material_slot_name')).lower()
        cur = sm.get_editor_property('material_interface')
        cur_name = cur.get_name().lower() if cur else ''
        if 'superhero' in slot or 'superhero' in cur_name or 'body' in slot:
            sm.set_editor_property('material_interface', mi)
            changed = True
    if not changed and mats:     # single-slot meshes (arms)
        mats[-1].set_editor_property('material_interface', mi)
    skm.set_editor_property('materials', mats)
    EAL.save_loaded_asset(skm)


def import_character(glb, folder, mesh_name, anim_folder, mats):
    if not os.path.exists(glb):
        warn('missing ' + glb)
        return None
    paths = import_files([glb], folder, mesh_name)
    skms = [unreal.load_asset(p) for p in paths]
    skms = [s for s in skms if isinstance(s, unreal.SkeletalMesh)] or assets_in(folder, unreal.SkeletalMesh)
    if not skms:
        warn('no skeletal mesh from ' + glb)
        return None
    skm = move(skms[0], folder + '/' + mesh_name)
    mi = soldier_material(mats, folder, 'MI_' + mesh_name)
    retarget_body_slots(skm, mi)
    if anim_folder:
        anims = assets_in(folder, unreal.AnimSequence)
        by_clip = {}
        for a in anims:
            n = a.get_name()
            for clip in set(ANIM_MAP.values()) | {'Jump_Start', 'Jump_Land', 'Roll', 'Push_Loop', 'Idle_FoldArms_Loop', 'PickUp_Table',
                                                  'Pistol_Aim_Up', 'Pistol_Aim_Down', 'Zombie_Walk_Fwd_Loop', 'Consume', 'Hit_Knockback'}:
                if n == clip or n.endswith('_' + clip) or n.endswith(clip):
                    if clip not in by_clip or len(n) < len(by_clip[clip].get_name()):
                        by_clip[clip] = a
        made = []
        for target, clip in ANIM_MAP.items():
            a = by_clip.get(clip)
            if a is None:
                warn('%s: animation %s not found (have: %s)' % (mesh_name, clip, ', '.join(sorted(x.get_name() for x in anims))[:400]))
                continue
            dest = anim_folder + '/' + target
            if exists(dest):
                EAL.delete_asset(dest)
            EAL.duplicate_asset(a.get_path_name().split('.')[0], dest)
            seq = load(dest)
            if seq and target not in ('A_Death', 'A_Throw', 'A_Reload', 'A_Revive', 'A_Shoot', 'A_HitChest', 'A_HitHead', 'A_Interact', 'A_GetUp'):
                try:
                    seq.set_editor_property('enable_root_motion', False)
                except Exception:
                    pass
            made.append(target)
        log('%s anims: %s' % (mesh_name, ', '.join(made)))
    return skm


def import_characters(mats):
    C = ROOT + '/Characters'
    import_character(src('Characters', 'Soldier_Male.glb'), C + '/Soldier', 'SK_Soldier', C + '/Anims', mats)
    import_character(src('Characters', 'Soldier_Female.glb'), C + '/SoldierF', 'SK_SoldierF', C + '/AnimsF', mats)
    import_character(src('Characters', 'Arms.glb'), C + '/Arms', 'SK_Arms', None, mats)


# ------------------------------------------------------------------------------------------------ audio
def import_audio():
    n = 0
    for f in sorted(glob.glob(src('Audio', '*', '*.wav'))):
        sub = os.path.basename(os.path.dirname(f))
        name = 'SW_' + os.path.splitext(os.path.basename(f))[0]
        dest = ROOT + '/Audio/' + sub
        import_files([f], dest, name)
        sw = load(dest + '/' + name)
        if sw:
            if sub in ('Weapons', 'Grenades', 'Impacts', 'Footsteps'):
                # 3D sounds: default attenuation tuned for a 90 m warehouse
                pass
            n += 1
    log('sounds: %d' % n)


def import_fonts():
    f = src('Fonts', 'Rajdhani-SemiBold.ttf')
    if not os.path.exists(f):
        return
    dest = ROOT + '/UI/Fonts'
    import_files([f], dest, 'FF_Rajdhani')
    face = load(dest + '/FF_Rajdhani')
    if face is None:
        faces = assets_in(dest, unreal.FontFace)
        face = faces[0] if faces else None
    if face is None:
        warn('font face import failed')
        return
    try:
        from nsc_common import create_asset
        font = create_asset('F_NSC', dest, unreal.Font, unreal.FontFactory())
        font.set_editor_property('font_cache_type', unreal.FontCacheType.RUNTIME)
        comp = font.get_editor_property('composite_font')
        tf = comp.get_editor_property('default_typeface')
        fd = unreal.FontData()
        fd.set_editor_property('font_face_asset', face)
        entry = unreal.TypefaceEntry()
        entry.set_editor_property('name', 'Regular')
        entry.set_editor_property('font', fd)
        tf.set_editor_property('fonts', [entry])
        comp.set_editor_property('default_typeface', tf)
        font.set_editor_property('composite_font', comp)
        EAL.save_loaded_asset(font)
    except Exception as e:
        warn('F_NSC not created (%s); the UI falls back to the engine font' % e)
