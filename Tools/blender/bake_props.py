"""Blender 4.x batch: every prop under SourceAssets/Props/{PolyHaven,3DAssets} -> one joined mesh per prop with the
origin at the bottom centre, exported to SourceAssets/Props/Baked/SM_<Name>.glb, plus Baked/sizes.json (metres, Z up).
  blender -b -P Tools/blender/bake_props.py -- <SourceAssets dir>"""
import bpy, glob, json, os, sys
from mathutils import Matrix, Vector
sa = sys.argv[sys.argv.index('--') + 1]
out_dir = os.path.join(sa, 'Props', 'Baked'); os.makedirs(out_dir, exist_ok=True)
files = sorted(glob.glob(os.path.join(sa, 'Props', 'PolyHaven', '*', '*.gltf')) + glob.glob(os.path.join(sa, 'Props', '3DAssets', '*.glb')))
sizes = {}
for f in files:
    name = os.path.splitext(os.path.basename(f))[0]
    bpy.ops.wm.read_factory_settings(use_empty=True)
    try:
        bpy.ops.import_scene.gltf(filepath=f)
    except Exception as e:
        print('NSC_FAIL', name, e); continue
    meshes = [o for o in bpy.context.scene.objects if o.type == 'MESH']
    # Poly Haven files sometimes carry LOD meshes: keep only the most detailed variant of each "_LODn" group
    lods = [o for o in meshes if '_LOD' in o.name and not o.name.split('_LOD')[-1].startswith('0')]
    for o in lods:
        bpy.data.objects.remove(o, do_unlink=True)
    meshes = [o for o in bpy.context.scene.objects if o.type == 'MESH']
    if not meshes:
        print('NSC_FAIL', name, 'no mesh'); continue
    for o in meshes:
        mw = o.matrix_world.copy(); o.parent = None; o.matrix_world = mw
    for o in [o for o in bpy.context.scene.objects if o.type != 'MESH']:
        bpy.data.objects.remove(o, do_unlink=True)
    for o in meshes: o.select_set(True)
    bpy.context.view_layer.objects.active = meshes[0]
    bpy.ops.object.make_single_user(object=True, obdata=True)
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    if len(meshes) > 1: bpy.ops.object.join()
    obj = bpy.context.view_layer.objects.active; obj.name = 'SM_' + name
    cs = [v.co for v in obj.data.vertices]
    mn = Vector((min(c.x for c in cs), min(c.y for c in cs), min(c.z for c in cs))); mx = Vector((max(c.x for c in cs), max(c.y for c in cs), max(c.z for c in cs)))
    obj.data.transform(Matrix.Translation((-(mn.x + mx.x) / 2, -(mn.y + mx.y) / 2, -mn.z)))
    d = mx - mn
    sizes[name] = [round(d.x, 4), round(d.y, 4), round(d.z, 4)]   # Blender X, Y(depth), Z(up) in metres
    bpy.ops.export_scene.gltf(filepath=os.path.join(out_dir, f'SM_{name}.glb'), export_format='GLB', export_yup=True,
                              export_animations=False, export_skins=False, export_image_format='AUTO')
    print('NSC_BAKED', name, sizes[name])
    json.dump(sizes, open(os.path.join(out_dir, 'sizes.json'), 'w'), indent=0)
json.dump(sizes, open(os.path.join(out_dir, 'sizes.json'), 'w'), indent=0)
