"""Blender 4.x: normalise one weapon/grenade model for NO SECOND CHANCE and export a self-contained GLB.

  blender -b -P Tools/blender/normalize_weapon.py -- <weapon dir> <Id> <length_m> [overrides json]

* imports <Id>.fbx (with T_<Id>_BC/N/ORM textures + material.json) or <Id>.glb
* joins all meshes, drops empties/armatures, bakes transforms
* orients it: barrel along +X, top along +Z (heuristic, overridable: {"flipX":true,"flipZ":true,"swapUp":true})
* scales so its longest side is <length_m>
* origin: 30% from the rear and 30% from the top, so the bounds stay asymmetric and the Unreal importer can
  re-check the orientation after glTF axis conversion
* exports SM_<Id>.glb (PBR material embedded) and a preview PNG (side view)"""
import bpy, bmesh, json, math, os, sys
from mathutils import Matrix, Vector

argv = sys.argv[sys.argv.index('--') + 1:]
wdir, wid, length = argv[0], argv[1], float(argv[2])
ov = json.loads(argv[3]) if len(argv) > 3 else {}

bpy.ops.wm.read_factory_settings(use_empty=True)
fbx = os.path.join(wdir, wid + '.fbx'); glb = os.path.join(wdir, wid + '.glb')
if os.path.exists(fbx):
    bpy.ops.import_scene.fbx(filepath=fbx, use_anim=False, ignore_leaf_bones=True)
else:
    bpy.ops.import_scene.gltf(filepath=glb)

meshes = [o for o in bpy.context.scene.objects if o.type == 'MESH']
for o in bpy.context.scene.objects:
    o.select_set(False)
# free meshes from armatures / parents and bake world transforms
for o in meshes:
    for m in [m for m in o.modifiers if m.type == 'ARMATURE']:
        o.modifiers.remove(m)
    mw = o.matrix_world.copy(); o.parent = None; o.matrix_world = mw
for o in [o for o in bpy.context.scene.objects if o.type != 'MESH']:
    bpy.data.objects.remove(o, do_unlink=True)
for o in meshes:
    o.select_set(True)
bpy.context.view_layer.objects.active = meshes[0]
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
if len(meshes) > 1:
    bpy.ops.object.join()
obj = bpy.context.view_layer.objects.active
obj.name = 'SM_' + wid
if ov.get('dropLoose'):   # remove loose parts that sit outside the main body (e.g. a spare magazine next to the gun)
    bpy.ops.object.mode_set(mode='EDIT'); bpy.ops.mesh.select_all(action='SELECT'); bpy.ops.mesh.separate(type='LOOSE'); bpy.ops.object.mode_set(mode='OBJECT')
    parts = list(bpy.context.selected_objects)
    def bb(o):
        cs = [o.matrix_world @ Vector(c) for c in o.bound_box]
        return Vector((min(c.x for c in cs), min(c.y for c in cs), min(c.z for c in cs))), Vector((max(c.x for c in cs), max(c.y for c in cs), max(c.z for c in cs)))
    main = max(parts, key=lambda o: len(o.data.vertices)); a0, a1 = bb(main)
    keep = []
    for o in parts:
        b0, b1 = bb(o)
        if o is main or all(b0[i] <= a1[i] and b1[i] >= a0[i] for i in range(3)): keep.append(o)
        else: bpy.data.objects.remove(o, do_unlink=True)
    for o in keep: o.select_set(True)
    bpy.context.view_layer.objects.active = main
    if len(keep) > 1: bpy.ops.object.join()
    obj = bpy.context.view_layer.objects.active; obj.name = 'SM_' + wid

# ---------------------------------------------------------------- material for FBX sources
if os.path.exists(fbx):
    meta = json.load(open(os.path.join(wdir, 'material.json')))
    mat = bpy.data.materials.new('MI_' + wid); mat.use_nodes = True
    nt = mat.node_tree; bsdf = nt.nodes['Principled BSDF']
    def tex(name, colorspace):
        p = os.path.join(wdir, name)
        if not os.path.exists(p): return None
        n = nt.nodes.new('ShaderNodeTexImage'); n.image = bpy.data.images.load(p); n.image.colorspace_settings.name = colorspace
        return n
    bc = tex(f'T_{wid}_BC.png', 'sRGB')
    if bc: nt.links.new(bc.outputs['Color'], bsdf.inputs['Base Color'])
    orm = tex(f'T_{wid}_ORM.png', 'Non-Color')
    if orm:
        sep = nt.nodes.new('ShaderNodeSeparateColor'); nt.links.new(orm.outputs['Color'], sep.inputs['Color'])
        nt.links.new(sep.outputs['Green'], bsdf.inputs['Roughness']); nt.links.new(sep.outputs['Blue'], bsdf.inputs['Metallic'])
    n = tex(f'T_{wid}_N.png', 'Non-Color')
    if n:
        src = n.outputs['Color']
        if meta.get('normal') == 'DirectX':   # glTF wants OpenGL (+Y) normals: invert green
            sep = nt.nodes.new('ShaderNodeSeparateColor'); nt.links.new(src, sep.inputs['Color'])
            inv = nt.nodes.new('ShaderNodeMath'); inv.operation = 'SUBTRACT'; inv.inputs[0].default_value = 1.0
            nt.links.new(sep.outputs['Green'], inv.inputs[1])
            comb = nt.nodes.new('ShaderNodeCombineColor'); nt.links.new(sep.outputs['Red'], comb.inputs['Red'])
            nt.links.new(inv.outputs[0], comb.inputs['Green']); nt.links.new(sep.outputs['Blue'], comb.inputs['Blue'])
            src = comb.outputs['Color']
        nm = nt.nodes.new('ShaderNodeNormalMap'); nt.links.new(src, nm.inputs['Color']); nt.links.new(nm.outputs['Normal'], bsdf.inputs['Normal'])
    e = tex(f'T_{wid}_E.png', 'sRGB')
    if e:
        nt.links.new(e.outputs['Color'], bsdf.inputs['Emission Color']); bsdf.inputs['Emission Strength'].default_value = 1.0
    obj.data.materials.clear(); obj.data.materials.append(mat)

# ---------------------------------------------------------------- orientation heuristics
V = [v.co.copy() for v in obj.data.vertices]
mn = Vector((min(v[i] for v in V) for i in range(3))); mx = Vector((max(v[i] for v in V) for i in range(3)))
ext = mx - mn
axes = sorted(range(3), key=lambda i: -ext[i])
L, U = axes[0], axes[1]
if ov.get('swapUp'): U = axes[2]
grenade = wid.startswith('Grenade')
if grenade:   # keep the tall axis up, nothing else to decide
    L, U = (axes[1] if axes[0] == 2 else axes[0]), 2
def slices(axis_l, axis_u, n=12):
    lo, hi = mn[axis_l], mx[axis_l]; top = [], 
    tops, bots = [], []
    for k in range(n):
        a = lo + (hi - lo) * k / n; b = lo + (hi - lo) * (k + 1) / n
        us = [v[axis_u] for v in V if a <= v[axis_l] <= b]
        if us: tops.append(max(us)); bots.append(min(us))
    return tops, bots
def var(x):
    m = sum(x) / len(x); return sum((t - m) ** 2 for t in x) / len(x)
tops, bots = slices(L, U)
up_sign = 1.0 if var(tops) <= var(bots) else -1.0      # the top line of a gun is flatter than its underside
# barrel end = the end whose vertical extent is smaller
q = ext[L] * 0.2
def vext(lo, hi):
    us = [v[U] for v in V if lo <= v[L] <= hi]; return (max(us) - min(us)) if us else 0
fwd_sign = 1.0 if vext(mx[L] - q, mx[L]) <= vext(mn[L], mn[L] + q) else -1.0
if grenade: up_sign, fwd_sign = 1.0, 1.0
if ov.get('flipX'): fwd_sign = -fwd_sign
if ov.get('flipZ'): up_sign = -up_sign
fx = Vector((0, 0, 0)); fx[L] = fwd_sign
fz = Vector((0, 0, 0)); fz[U] = up_sign
fy = fz.cross(fx)
R = Matrix((fx, fy, fz))          # rows: new axes expressed in old coords -> maps old -> new
obj.data.transform(R.to_4x4())
# scale + origin
V = [v.co.copy() for v in obj.data.vertices]
mn = Vector((min(v[i] for v in V) for i in range(3))); mx = Vector((max(v[i] for v in V) for i in range(3)))
s = length / max(mx - mn)
obj.data.transform(Matrix.Scale(s, 4))
mn *= s; mx *= s
ox = mn.x + 0.3 * (mx.x - mn.x); oy = (mn.y + mx.y) / 2; oz = mx.z - 0.3 * (mx.z - mn.z)
if grenade: ox = (mn.x + mx.x) / 2; oz = mn.z + 0.3 * (mx.z - mn.z)
obj.data.transform(Matrix.Translation((-ox, -oy, -oz)))
obj.location = (0, 0, 0); obj.rotation_euler = (0, 0, 0); obj.scale = (1, 1, 1)
bpy.ops.object.shade_smooth() if not obj.data.polygons or True else None

out = os.path.join(wdir, f'SM_{wid}.glb')
bpy.ops.export_scene.gltf(filepath=out, export_format='GLB', use_selection=False, export_apply=True, export_yup=True,
                          export_image_format='AUTO', export_animations=False, export_skins=False)
# ---------------------------------------------------------------- preview (side view, barrel to the right)
scn = bpy.context.scene
cam = bpy.data.objects.new('cam', bpy.data.cameras.new('cam')); scn.collection.objects.link(cam)
cam.data.type = 'ORTHO'; cam.data.ortho_scale = length * 1.3
cam.location = (length * 0.2, -3, 0); cam.rotation_euler = (math.pi / 2, 0, 0)
scn.camera = cam
sun = bpy.data.objects.new('sun', bpy.data.lights.new('sun', 'SUN')); sun.rotation_euler = (0.6, 0.2, 0.3); scn.collection.objects.link(sun)
sun.data.energy = 4
scn.render.engine = 'BLENDER_WORKBENCH'; scn.render.resolution_x = 480; scn.render.resolution_y = 240
scn.display.shading.color_type = 'TEXTURE'
scn.render.filepath = os.path.join(wdir, f'preview_{wid}.png')
bpy.ops.render.render(write_still=True)
print('NSC_NORMALIZED', wid, 'L', L, 'U', U, 'fwd', fwd_sign, 'up', up_sign, 'scale', round(s, 4))
