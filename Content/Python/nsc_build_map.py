"""Builds /Game/NSC/Maps/L_Warehouse from warehouse_layout.json (exported from the browser game's js/map.js by
Tools/layout/export_warehouse_layout.mjs) and /Game/NSC/Maps/L_MainMenu."""
import json
import math
import os
import random
import unreal
from nsc_common import EAL, ROOT, exists, load, log, warn, to_ue, ensure_dir
import nsc_materials as M

HERE = os.path.dirname(os.path.abspath(__file__))
EAS = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
LES = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
CUBE = None
RNG = random.Random(1)

# browser material key -> Poly Haven surface id, tile size (cm), tint
SURFACES = {
    'floor': ('concrete_floor_worn_001', 400, (1, 1, 1)), 'wall': ('concrete_wall_006', 300, (1, 1, 1)),
    'outer': ('corrugated_iron_02', 300, (0.85, 0.88, 0.9)), 'hazard': ('painted_metal_shutter', 100, (1.0, 0.75, 0.1)),
    'shutter': ('rusted_shutter', 200, (1, 1, 1)), 'ceiling': ('box_profile_metal_sheet', 300, (0.8, 0.8, 0.8)),
    'beam': ('rusty_metal_02', 150, (1, 1, 1)), 'metal': ('metal_plate', 150, (1, 1, 1)),
    'container': ('rusty_corrugated_iron', 250, (1, 1, 1)), 'ground': ('worn_asphalt', 500, (1, 1, 1)),
    'quay': ('hangar_concrete_floor', 400, (1, 1, 1)),
}
CONTAINER_TINTS = [(0.75, 0.3, 0.12), (0.2, 0.38, 0.55), (0.42, 0.45, 0.48), (0.8, 0.6, 0.15), (0.3, 0.45, 0.25), (0.55, 0.14, 0.12)]
PROP = {  # browser model key -> baked prop name
    'barrel1': 'Barrel_01', 'barrel3': 'barrel_03', 'jerrycan': 'metal_jerrycan', 'propane': 'propane_tank',
    'extinguisher': 'korean_fire_extinguisher_01', 'toolbox': 'metal_toolbox', 'medbox': 'medical_box', 'ammobox': 'ammo_box',
    'generator': 'portable_generator', 'tyre': 'old_tyre', 'handtruck': 'hand_truck', 'cart': 'industrial_storage_cart',
    'palletTruck': 'PalletJack', 'lamp': 'hanging_industrial_lamp', 'shelves1': 'steel_frame_shelves_03',
    'milcrate': 'wooden_military_crate', 'crate2': 'wooden_crate_02', 'cardboard': 'cardboard_box_01',
    'plcrate': 'plastic_crate_01', 'pallet': 'Pallet', 'barrier': 'concrete_road_barrier',
}

MATS = {}
MESH_CACHE = {}
FOLDER = 'Warehouse'


def prop_mesh(name):
    if name not in MESH_CACHE:
        MESH_CACHE[name] = load('%s/Props/%s/SM_%s' % (ROOT, name, name))
        if MESH_CACHE[name] is None:
            warn('prop missing: ' + name)
    return MESH_CACHE[name]


def editor_world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


def spawn_mesh(mesh, loc, rot=None, scale=None, label=None, folder=None, collide=True, hidden=False, mat=None, shadow=True, nav=True):
    a = EAS.spawn_actor_from_class(unreal.StaticMeshActor, loc, rot or unreal.Rotator(0, 0, 0))
    c = a.static_mesh_component
    c.set_static_mesh(mesh)
    if scale is not None:
        a.set_actor_scale3d(scale)
    if mat is not None:
        for i in range(c.get_num_materials()):
            c.set_material(i, mat)
    if not collide:
        c.set_collision_profile_name('NoCollision')
        c.set_editor_property('can_ever_affect_navigation', False)
    if not nav:
        c.set_editor_property('can_ever_affect_navigation', False)
    if hidden:
        a.set_actor_hidden_in_game(True)
        c.set_editor_property('cast_shadow', False)
        c.set_editor_property('visible', False)
    if not shadow:
        c.set_editor_property('cast_shadow', False)
    if label:
        a.set_actor_label(label)
    a.set_folder_path(folder or FOLDER)
    return a


def box(cx, cz, sx, sz, h, y0=0.0, mat='wall', label='Box', collide=True, hidden=False, folder=None, nav=True):
    """Browser AABB (centre x/z, size x/z/h, bottom y0, metres) -> scaled engine cube (100 cm)."""
    loc = to_ue(cx, cz, y0 + h / 2.0)
    scale = unreal.Vector(sz, sx, h)       # UE X spans browser z, UE Y spans browser x
    m = MATS.get(mat) if isinstance(mat, str) else mat
    return spawn_mesh(CUBE, loc, None, scale, label, folder, collide, hidden, None if hidden else m, nav=nav)


def fit(name, x, y, z, sx, sy, sz, inner=0.0, outer=0.0, uniform=False, label=None, folder=None):
    """Port of the browser fitModel(): scale a bottom-centred prop to a world box (metres), rotate by inner+outer (radians)."""
    mesh = prop_mesh(name)
    if mesh is None:
        return None
    b = mesh.get_bounding_box()
    ux, uy, uz = max(b.max.x - b.min.x, 1e-3), max(b.max.y - b.min.y, 1e-3), max(b.max.z - b.min.z, 1e-3)
    mx_b, mz_b = uy / 100.0, ux / 100.0            # model extent along browser x / z
    rot90 = int(round(abs(inner) / (math.pi / 2))) % 2 == 1
    wx, wz = (mz_b, mx_b) if rot90 else (mx_b, mz_b)
    kx, ky, kz = sx / wx, sy / (uz / 100.0), sz / wz
    if uniform:
        kx = ky = kz = min(kx, ky, kz)
    s_ue_y = kz if rot90 else kx                  # local scale along model browser-x (UE Y)
    s_ue_x = kx if rot90 else kz                  # local scale along model browser-z (UE X)
    yaw = -math.degrees(inner + outer)
    # keep the bottom centre of the bounds on the target point
    cx, cy, bz = (b.min.x + b.max.x) / 2.0 * s_ue_x, (b.min.y + b.max.y) / 2.0 * s_ue_y, b.min.z * ky
    r = math.radians(yaw)
    off = unreal.Vector(cx * math.cos(r) - cy * math.sin(r), cx * math.sin(r) + cy * math.cos(r), bz)
    loc = to_ue(x, z, y) - off
    return spawn_mesh(mesh, loc, unreal.Rotator(0, 0, yaw), unreal.Vector(s_ue_x, s_ue_y, ky), label or name, folder, collide=False)


def jit():
    return (RNG.random() - 0.5) * 0.06


# ------------------------------------------------------------------------------------------------ layout pieces
def container(it):
    cx, cz, along, ci, y0 = it['cx'], it['cz'], it['alongZ'], it['colorIdx'], it.get('y0', 0)
    sx, sz = (2.44, 6.1) if along else (6.1, 2.44)
    box(cx, cz, sx, sz, 2.6, y0, MATS['container%d' % (ci % 6)], 'Container', folder=FOLDER + '/Containers')


def crate(it):
    cx, cz, s, stack, y0, kind = it['cx'], it['cz'], it['s'], it['stack'], it.get('y0', 0), it['kind']
    box(cx, cz, s, s, s * stack, y0, None, 'CrateCollider', hidden=True, folder=FOLDER + '/Collision')
    H = s * stack
    f = FOLDER + '/Cover'
    if kind == 0:
        n = max(1, round(H / 0.4)); lh = H / n
        for i in range(n):
            for k in (-1, 1):
                rot = math.pi / 2 if i % 2 else 0
                ox, oz = (k * s / 4, 0) if i % 2 else (0, k * s / 4)
                fit('wooden_military_crate', cx + ox, y0 + i * lh, cz + oz, s / 2 if i % 2 else s, lh, s if i % 2 else s / 2, rot, jit(), folder=f)
    elif kind == 1:
        n = max(1, round(H / 0.6)); lh = H / n
        for i in range(n):
            for k in (-1, 1):
                along_x = i % 2 == 1
                ox, oz = (0, k * s / 4) if along_x else (k * s / 4, 0)
                fit('wooden_crate_02', cx + ox, y0 + i * lh, cz + oz, s if along_x else s / 2, lh, s / 2 if along_x else s,
                    math.pi / 2 if along_x else 0, jit(), folder=f)
    else:
        for lv in range(int(stack)):
            by = y0 + lv * s
            fit('Pallet', cx, by, cz, s, 0.14, s, folder=f)
            n = 3; lh = (s - 0.14) / n
            for i in range(n):
                for a in (-1, 1):
                    for b in (-1, 1):
                        fit('cardboard_box_01', cx + a * s / 4, by + 0.14 + i * lh, cz + b * s / 4, s / 2 - 0.02, lh, s / 2 - 0.02,
                            math.pi / 2 if (i + a + b) % 2 else 0, jit(), folder=f)


def pallet(it):
    cx, cz, sx, sz, h = it['cx'], it['cz'], it['sx'], it['sz'], it['h']
    box(cx, cz, sx, sz, h, 0, None, 'PalletCollider', hidden=True, folder=FOLDER + '/Collision')
    f = FOLDER + '/Cover'
    fit('Pallet', cx, 0, cz, sx, 0.15, sz, folder=f)
    nx, nz, ny = max(1, round(sx / 0.55)), max(1, round(sz / 0.45)), max(1, round((h - 0.15) / 0.36))
    bx, bz, by = sx * 0.96 / nx, sz * 0.96 / nz, (h - 0.15) / ny
    for i in range(nx):
        for j in range(nz):
            for k in range(ny):
                fit('cardboard_box_01' if k % 2 else 'plastic_crate_02', cx - sx * 0.48 + bx * (i + 0.5), 0.15 + k * by,
                    cz - sz * 0.48 + bz * (j + 0.5), bx - 0.02, by, bz - 0.02, 0, (RNG.random() - 0.5) * 0.05, folder=f)


def rack(it):
    x, z0, z1 = it['x'], it['z0'], it['z1']
    L, w, H = z1 - z0, 1.3, 3.8
    box(x, (z0 + z1) / 2, w, L, H, 0, None, 'RackCollider', hidden=True, folder=FOLDER + '/Collision')
    n = max(1, round(L / 2.2)); ul = L / n; th = H / 2
    f = FOLDER + '/Shelving'
    for i in range(n):
        zc = z0 + ul * (i + 0.5)
        for side in (-1, 1):
            for t in range(2):
                fit('steel_frame_shelves_03', x + side * w / 4, t * th, zc, w / 2, th, ul, math.pi / 2, folder=f)
                for lv in (0.02, 0.5):
                    if RNG.random() < 0.3:
                        continue
                    key = RNG.choice(['cardboard_box_01', 'cardboard_box_01', 'plastic_crate_01', 'wooden_military_crate'])
                    bw = ul * (0.3 + RNG.random() * 0.25)
                    fit(key, x + side * w / 4, t * th + lv * th + 0.012, zc + (RNG.random() - 0.5) * (ul - bw) * 0.8,
                        w / 2 - 0.12, th * 0.17, bw, math.pi / 2, (RNG.random() - 0.5) * 0.1, folder=f)
    box(x, (z0 + z1) / 2, 0.04, L, H, 0, 'metal', 'RackBackPanel', collide=False, folder=f)


def lamp(it):
    x, z, with_light = it['x'], it['z'], it['withLight']
    fit('hanging_industrial_lamp', x, 9 - 1.25, z, 0.75, 1.25, 0.75, uniform=True, folder=FOLDER + '/Lights')
    if with_light:
        l = EAS.spawn_actor_from_class(unreal.PointLight, to_ue(x, z, 9 - 1.45))
        c = l.point_light_component
        c.set_editor_property('intensity_units', unreal.LightUnits.CANDELAS)
        c.set_editor_property('intensity', 900.0)
        c.set_editor_property('attenuation_radius', 3200.0)
        c.set_editor_property('light_color', unreal.Color(r=255, g=226, b=176, a=255))   # warm sodium-ish
        c.set_editor_property('source_radius', 20.0)
        l.set_actor_label('Lamp')
        l.set_folder_path(FOLDER + '/Lights')


def decor(it):
    name = PROP.get(it['key'])
    if not name:
        return
    if it.get('collide'):
        s = max(it['sx'], it['sz'])
        box(it['x'], it['z'], s, s, it['sy'], it.get('y0', 0), None, 'DecorCollider', hidden=True, folder=FOLDER + '/Collision')
    fit(name, it['x'], it.get('y0', 0), it['z'], it['sx'], it['sy'], it['sz'], 0, it.get('rotY', 0), uniform=True, folder=FOLDER + '/Decor')


def barrier(it):
    box(it['x'], it['z'], 0.4, 3, 1.0, 0, None, 'BarrierCollider', hidden=True, folder=FOLDER + '/Collision')
    for k in (-0.75, 0.75):
        fit('concrete_road_barrier', it['x'], 0, it['z'] + k, 0.5, 1.0, 1.5, math.pi / 2, folder=FOLDER + '/Cover')


def sign(it):
    a = EAS.spawn_actor_from_class(unreal.TextRenderActor, to_ue(it['x'], it['z'], it['y']))
    c = a.text_render
    c.set_editor_property('text', it['text'].replace('←', '<').replace('→', '>'))
    c.set_editor_property('world_size', 55.0)
    c.set_editor_property('horizontal_alignment', unreal.HorizTextAligment.EHTA_CENTER)
    c.set_editor_property('vertical_alignment', unreal.VerticalTextAligment.EVRTA_TEXT_CENTER)
    c.set_editor_property('text_render_color', unreal.Color(r=213, g=166, b=74, a=255))   # #D5A64A
    # browser signs face +z when rotY = 0 -> in UE that is -X; TextRender faces +X
    a.set_actor_rotation(unreal.Rotator(0, 0, 180 - math.degrees(it['rotY'])), False)
    a.set_actor_label('Sign_' + it['text'][:8])
    a.set_folder_path(FOLDER + '/Signs')


def model(it):
    if it['key'] == 'container':
        box(it['x'], it['z'], it['sx'], it['sz'], it['sy'], it['y'], MATS['container2'], 'Container', collide=False, folder=FOLDER + '/Containers')


# ------------------------------------------------------------------------------------------------ level
def build_shell(B):
    W, D = B['x1'] - B['x0'], B['z1'] - B['z0']
    box(0, 0, W, D, 0.2, -0.2, 'floor', 'Floor', folder=FOLDER + '/Shell')
    # roof with skylight openings (3.2 x 7 m at x in [-30..30 step 12], z in {-24, 0, 24})
    xs = [-30, -18, -6, 6, 18, 30]; zs = [-24, 0, 24]; hw, hd = 1.6, 3.5
    edges = [B['x0']]
    for x in xs:
        edges += [x - hw, x + hw]
    edges.append(B['x1'])
    for i in range(0, len(edges), 2):           # solid strips between skylight columns
        x0, x1 = edges[i], edges[i + 1]
        box((x0 + x1) / 2, 0, x1 - x0, D, 0.1, 9.0, 'ceiling', 'Roof', folder=FOLDER + '/Shell', nav=False)
    for x in xs:                                # inside a column: pieces between the openings
        zedges = [B['z0']]
        for z in zs:
            zedges += [z - hd, z + hd]
        zedges.append(B['z1'])
        for i in range(0, len(zedges), 2):
            z0, z1 = zedges[i], zedges[i + 1]
            box(x, (z0 + z1) / 2, 2 * hw, z1 - z0, 0.1, 9.0, 'ceiling', 'Roof', folder=FOLDER + '/Shell', nav=False)
    for x in range(-36, 37, 12):
        box(x, 0, 0.4, D, 0.6, 8.4, 'beam', 'Beam', collide=False, folder=FOLDER + '/Shell')
    for z in range(-32, 33, 16):
        box(0, z, W, 0.3, 0.35, 8.375, 'beam', 'Beam', collide=False, folder=FOLDER + '/Shell')
    T, H = 1.0, 9.0
    box(0, B['z0'] - T / 2, W + 2 * T, T, H, 0, 'outer', 'OuterWall', folder=FOLDER + '/Shell')
    box(0, B['z1'] + T / 2, W + 2 * T, T, H, 0, 'outer', 'OuterWall', folder=FOLDER + '/Shell')
    box(B['x0'] - T / 2, 0, T, D, H, 0, 'outer', 'OuterWall', folder=FOLDER + '/Shell')
    box(B['x1'] + T / 2, 0, T, D, H, 0, 'outer', 'OuterWall', folder=FOLDER + '/Shell')


def build_dockside(B):
    """Non-playable dockyard dressing outside the warehouse (seen through the skylights / from spawn doors)."""
    f = 'Dockside'
    box(0, 0, 220, 200, 0.2, -0.45, 'ground', 'Ground', folder=f, nav=False)
    box(0, B['z0'] - 22, 160, 30, 1.2, -1.2, 'quay', 'Quay', folder=f, nav=False)
    for i, x in enumerate(range(-60, 61, 8)):
        if abs(x) < 50:
            box(x, B['z1'] + 12 + (i % 3) * 3, 2.44, 6.1, 2.6, 0, MATS['container%d' % (i % 6)], 'YardContainer', folder=f)
            if i % 2 == 0:
                box(x, B['z1'] + 12 + (i % 3) * 3, 2.44, 6.1, 2.6, 2.6, MATS['container%d' % ((i + 3) % 6)], 'YardContainer', folder=f)
    fit('DockCrane', -20, 0, B['z0'] - 30, 27, 17, 7, 0, 0, uniform=True, folder=f)
    fit('DockCrane', 25, 0, B['z0'] - 30, 27, 17, 7, 0, 0, uniform=True, folder=f)
    for x in range(-70, 71, 14):
        fit('Bollard', x, 0, B['z0'] - 35.5, 0.6, 0.98, 0.6, uniform=True, folder=f)
    fit('Forklift', 50, 0, B['z1'] + 6, 1.4, 2.65, 3.1, 0.4, 0, uniform=True, folder=f)


def spawn_lighting(hdri):
    f = 'Lighting'
    sun = EAS.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 2000), unreal.Rotator(0, -50, 35))
    sc = sun.get_component_by_class(unreal.DirectionalLightComponent)
    sc.set_editor_property('intensity', 8.0)
    sc.set_editor_property('light_color', unreal.Color(r=255, g=240, b=220, a=255))
    sc.set_editor_property('atmosphere_sun_light', True)
    sun.set_folder_path(f)
    EAS.spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector(0, 0, 0)).set_folder_path(f)
    sky = EAS.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 1500))
    slc = sky.get_component_by_class(unreal.SkyLightComponent)
    if hdri is not None:
        slc.set_editor_property('source_type', unreal.SkyLightSourceType.SLS_SPECIFIED_CUBEMAP)
        slc.set_editor_property('cubemap', hdri)
    else:
        slc.set_editor_property('real_time_capture', True)
    slc.set_editor_property('intensity', 1.2)
    sky.set_folder_path(f)
    fog = EAS.spawn_actor_from_class(unreal.ExponentialHeightFog, unreal.Vector(0, 0, -100))
    fc = fog.get_component_by_class(unreal.ExponentialHeightFogComponent)
    fc.set_editor_property('fog_density', 0.012)
    fc.set_editor_property('volumetric_fog', True)
    fog.set_folder_path(f)
    ppv = EAS.spawn_actor_from_class(unreal.PostProcessVolume, unreal.Vector(0, 0, 400))
    ppv.set_editor_property('unbound', True)
    s = ppv.get_editor_property('settings')
    for k, v in (('override_auto_exposure_method', True), ('auto_exposure_method', unreal.AutoExposureMethod.AEM_HISTOGRAM),
                 ('override_auto_exposure_bias', True), ('auto_exposure_bias', 0.6),
                 ('override_auto_exposure_min_brightness', True), ('auto_exposure_min_brightness', -1.0),
                 ('override_auto_exposure_max_brightness', True), ('auto_exposure_max_brightness', 12.0),
                 ('override_bloom_intensity', True), ('bloom_intensity', 0.5),
                 ('override_vignette_intensity', True), ('vignette_intensity', 0.35),
                 ('override_motion_blur_amount', True), ('motion_blur_amount', 0.0)):
        try:
            s.set_editor_property(k, v)
        except Exception as e:
            warn('post process %s: %s' % (k, e))
    ppv.set_editor_property('settings', s)
    ppv.set_folder_path(f)
    # skylight panels (emissive) in the roof openings
    for x in [-30, -18, -6, 6, 18, 30]:
        for z in [-24, 0, 24]:
            box(x, z, 3.2, 7, 0.02, 9.12, MATS['skylight'], 'SkylightGlass', collide=False, folder=f)


def gameplay_actors(L):
    f = 'Gameplay'
    site_cls = unreal.load_class(None, '/Script/NoSecondChance.NSCBombSite')
    for name, s in L['SITES'].items():
        if site_cls is None:
            break
        a = EAS.spawn_actor_from_class(site_cls, to_ue(s['cx'], s['cz'], 1.5))
        a.set_editor_property('site_name', name)
        vol = a.get_editor_property('volume')
        vol.set_box_extent(unreal.Vector((s['z1'] - s['z0']) * 50.0, (s['x1'] - s['x0']) * 50.0, 300.0), True)
        a.set_actor_label('BombSite_' + name)
        a.set_folder_path(f)
        # floor marking
        t = EAS.spawn_actor_from_class(unreal.TextRenderActor, to_ue(s['cx'], s['cz'], 0.02), unreal.Rotator(0, 90, 0))   # roll, pitch, yaw: lie flat, text up
        tc = t.text_render
        tc.set_editor_property('text', name)
        tc.set_editor_property('world_size', 650.0)
        tc.set_editor_property('horizontal_alignment', unreal.HorizTextAligment.EHTA_CENTER)
        tc.set_editor_property('vertical_alignment', unreal.VerticalTextAligment.EVRTA_TEXT_CENTER)
        tc.set_editor_property('text_render_color', unreal.Color(r=213, g=166, b=74, a=140))
        t.set_actor_label('SiteMarking_' + name)
        t.set_folder_path(f)
    for tag, key, yaw in (('Attackers', 'atk', 0.0), ('Defenders', 'def', 180.0)):
        for i, (x, z) in enumerate(L['SPAWNS'][key]):
            ps = EAS.spawn_actor_from_class(unreal.PlayerStart, to_ue(x, z, 1.0), unreal.Rotator(0, 0, yaw))
            ps.set_editor_property('player_start_tag', tag)
            ps.set_actor_label('%s_%d' % (tag, i))
            ps.set_folder_path(f)
    B = L['BOUNDS']
    nav = EAS.spawn_actor_from_class(unreal.NavMeshBoundsVolume, to_ue((B['x0'] + B['x1']) / 2, (B['z0'] + B['z1']) / 2, 4.0))
    nav.set_actor_scale3d(unreal.Vector((B['z1'] - B['z0'] + 4) / 2.0, (B['x1'] - B['x0'] + 4) / 2.0, 6.0))   # brush is 200 cm
    nav.set_folder_path(f)
    # world settings: Operations game mode
    try:
        ws = editor_world().get_world_settings()
        gm = unreal.load_class(None, '/Script/NoSecondChance.NSCGameMode')
        if gm:
            ws.set_editor_property('default_game_mode', gm)
    except Exception as e:
        warn('world settings: %s' % e)


def make_materials(mats, surfaces):
    for key, (sid, tile, tint) in SURFACES.items():
        s = surfaces.get(sid, {})
        MATS[key] = M.make_mi('MI_Surf_' + key, mats['surface'], scalars={'TileSize': float(tile)},
                              vectors={'Tint': tint + (1,)}, textures={'T_BaseColor': s.get('bc'), 'T_Normal': s.get('n'), 'T_ORM': s.get('orm')})
    s = surfaces.get('rusty_corrugated_iron', {})
    for i, c in enumerate(CONTAINER_TINTS):
        MATS['container%d' % i] = M.make_mi('MI_Container_%d' % i, mats['surface'], scalars={'TileSize': 250.0, 'MetallicScale': 0.6},
                                            vectors={'Tint': c + (1,)}, textures={'T_BaseColor': s.get('bc'), 'T_Normal': s.get('n'), 'T_ORM': s.get('orm')})
    MATS['skylight'] = M.make_mi('MI_Skylight', mats['emissive'], scalars={'Intensity': 6.0}, vectors={'Color': (0.85, 0.9, 1.0, 1)})
    MATS[None] = None


def new_level(path):
    if exists(path):
        unreal.EditorLoadingAndSavingUtils.new_blank_map(False)   # unload it before deleting
        EAL.delete_asset(path)
    ensure_dir(path.rsplit('/', 1)[0])
    if not LES.new_level(path):
        raise RuntimeError('could not create level ' + path)


def build_warehouse(mats, surfaces, hdri):
    global CUBE
    CUBE = unreal.load_asset('/Engine/BasicShapes/Cube.Cube')
    L = json.load(open(os.path.join(HERE, 'warehouse_layout.json')))
    make_materials(mats, surfaces)
    new_level(ROOT + '/Maps/L_Warehouse')
    build_shell(L['BOUNDS'])
    handlers = {'box': lambda it: box(it['cx'], it['cz'], it['sx'], it['sz'], it['h'], it.get('y0', 0), it['mat'], 'Wall' if it['mat'] == 'wall' else it['mat'].title()),
                'container': container, 'crate': crate, 'pallet': pallet, 'rack': rack, 'lamp': lamp, 'decor': decor,
                'barrier': barrier, 'sign': sign, 'model': model,
                'solid': lambda it: box(it['cx'], it['cz'], it['sx'], it['sz'], it['h'], it.get('y0', 0), None, 'Collider', hidden=True, folder=FOLDER + '/Collision')}
    counts = {}
    for it in L['items']:
        h = handlers.get(it['t'])
        if h is None:
            continue
        try:
            h(it)
            counts[it['t']] = counts.get(it['t'], 0) + 1
        except Exception as e:
            warn('layout item %s failed: %s' % (it, e))
    log('layout: ' + json.dumps(counts))
    build_dockside(L['BOUNDS'])
    spawn_lighting(hdri)
    gameplay_actors(L)
    try:
        unreal.SystemLibrary.execute_console_command(editor_world(), 'RebuildNavigation')
    except Exception as e:
        warn('nav rebuild: %s' % e)
    LES.save_current_level()
    log('L_Warehouse saved')


def build_menu(hdri):
    """L_MainMenu: the menu game mode has no pawn, so the view sits at the origin looking down +X: stage a container yard."""
    global CUBE
    CUBE = unreal.load_asset('/Engine/BasicShapes/Cube.Cube')
    new_level(ROOT + '/Maps/L_MainMenu')
    global FOLDER
    FOLDER = 'Menu'
    box(10, -20, 60, 60, 0.2, -1.9, 'ground', 'Ground')
    for i, (x, z, along) in enumerate([(-6, -14, True), (-6, -14, True), (5, -18, False), (9, -12, True), (-12, -24, False)]):
        y0 = 2.6 if i == 1 else 0
        sx, sz = (2.44, 6.1) if along else (6.1, 2.44)
        box(x, z, sx, sz, 2.6, y0 - 1.7, MATS['container%d' % (i % 6)], 'Container')
    spawn_lighting(hdri)
    try:
        ws = editor_world().get_world_settings()
        gm = unreal.load_class(None, '/Script/NoSecondChance.NSCMenuGameMode')
        if gm:
            ws.set_editor_property('default_game_mode', gm)
    except Exception as e:
        warn('menu world settings: %s' % e)
    LES.save_current_level()
    FOLDER = 'Warehouse'
    log('L_MainMenu saved')
