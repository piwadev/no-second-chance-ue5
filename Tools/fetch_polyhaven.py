#!/usr/bin/env python3
"""Download the Poly Haven (CC0) models, PBR textures and HDRIs used by NO SECOND CHANCE into SourceAssets/.
Re-runnable: files already present with the right size are skipped. Appends entries to SourceAssets/manifest.json.
Usage: python Tools/fetch_polyhaven.py [--res 1k|2k] [--out SourceAssets]
"""
import json, os, sys, urllib.request, argparse, time

MODELS = """Barrel_01 Barrel_02 barrel_03 wooden_barrels_01 ammo_box cardboard_box_01 concrete_road_barrier concrete_road_barrier_02
hand_truck hanging_industrial_lamp industrial_storage_cart industrial_pastic_container industrial_pipe_lamp industrial_wall_lamp
industrial_caged_sconce industrial_wall_sconce caged_hanging_light korean_fire_extinguisher_01 medical_box medical_tape metal_jerrycan
metal_jerrycan_green plastic_jerrycan metal_toolbox metal_tool_chest tool_cart mounted_fluorescent_lights old_tyre plastic_crate_01
plastic_crate_02 plastic_crate_03 portable_generator propane_tank small_lpg_tank steel_frame_shelves_01 steel_frame_shelves_02
steel_frame_shelves_03 wooden_crate_01 wooden_crate_02 wooden_military_crate old_military_crate old_military_compressor worn_metal_rack
security_camera_01 security_camera_02 security_light flood_lights portable_searchlight power_box_01 utility_box_01 utility_box_02
fire_alarm fire_hydrant exterior_aircon_unit modular_airduct_circular_01 modular_airduct_rectangular_01 modular_chainlink_fence
modular_electric_cables modular_factory_facade modular_fire_escape modular_industrial_pipes_01 modular_pipes modular_metal_gutter
rollershutter_door rollershutter_window_01 overhead_crane cement_bag trashbag metal_trash_can wooden_ladder ladder_sectioned_01
metal_office_desk metal_stool_01 drawer_cabinet clipboard crowbar_01 sledgehammer_01 portable_welding_cart WetFloorSign_01
water_manhole_cover lifebuoy life_jacket ocean_buoy modular_wooden_pier covered_car bolt_action_rifle_7_62 service_pistol stick_grenade
old_gas_mask signal_flashlight pastic_torch_6v binoculars vintage_radio_transceiver circuit_board long_life_food""".split()
TEXTURES = """concrete_floor_worn_001 corrugated_iron_02 box_profile_metal_sheet concrete_wall_006 metal_plate painted_metal_shutter rusty_metal_02
denim_fabric hangar_concrete_floor garage_floor concrete_floor_painted anti_slip_concrete factory_wall precast_concrete_wall
rusty_corrugated_iron worn_corrugated_iron rusty_metal_sheet metal_grate_rusty green_metal_rust blue_metal_plate rusty_painted_metal
rusted_shutter red_brick_03 factory_brick asphalt_02 worn_asphalt plywood weathered_planks dirty_concrete painted_concrete
concrete_block_wall rough_concrete metal_plate_02 rusty_metal_grid wood_planks_dirt""".split()
HDRIS = "empty_warehouse_01 abandoned_garage burnt_warehouse docklands_01 industrial_sunset_puresky hangar_interior".split()
UA = {"User-Agent": "NSC-asset-fetch/1.0 (github.com/piwadev/no-second-chance-ue5)"}

def get(url):
    for i in range(4):
        try:
            with urllib.request.urlopen(urllib.request.Request(url, headers=UA), timeout=120) as r: return r.read()
        except Exception as e:
            if i == 3: raise
            time.sleep(2 * (i + 1))

def save(url, dest, size=None):
    if os.path.exists(dest) and (size is None or os.path.getsize(dest) == size): return False
    os.makedirs(os.path.dirname(dest), exist_ok=True)
    data = get(url); open(dest + '.part', 'wb').write(data); os.replace(dest + '.part', dest); return True

def main():
    ap = argparse.ArgumentParser(); ap.add_argument('--res', default='1k'); ap.add_argument('--tex-res', default='2k'); ap.add_argument('--out', default=os.path.join(os.path.dirname(__file__), '..', 'SourceAssets'))
    a = ap.parse_args(); out = os.path.abspath(a.out); entries = []
    info = json.loads(get('https://api.polyhaven.com/assets'))
    def meta(aid, kind, files):
        i = info.get(aid, {}); authors = ', '.join(i.get('authors', {}).keys())
        entries.append({"name": i.get('name', aid), "id": aid, "kind": kind, "source": f"https://polyhaven.com/a/{aid}", "author": authors, "license": "CC0-1.0", "files": files})
    for m in MODELS:
        try:
            f = json.loads(get(f'https://api.polyhaven.com/files/{m}'))
            res = a.res if a.res in f['gltf'] else sorted(f['gltf'].keys())[0]
            g = f['gltf'][res]['gltf']; base = os.path.join(out, 'Props', 'PolyHaven', m); files = []
            save(g['url'], os.path.join(base, f'{m}.gltf'), g.get('size')); files.append(f'Props/PolyHaven/{m}/{m}.gltf')
            for rel, inc in g.get('include', {}).items():
                save(inc['url'], os.path.join(base, rel), inc.get('size'))
            meta(m, 'model', files); print('model', m, flush=True)
        except Exception as e: print('FAILED model', m, e, flush=True)
    for t in TEXTURES:
        try:
            f = json.loads(get(f'https://api.polyhaven.com/files/{t}')); base = os.path.join(out, 'Textures', 'PolyHaven', t); files = []
            for key, suf in (('Diffuse', 'diff'), ('nor_gl', 'nor_gl'), ('arm', 'arm'), ('Displacement', 'disp')):
                if key not in f: continue
                r = f[key].get(a.tex_res) or f[key][sorted(f[key])[0]]; fmt = 'jpg' if 'jpg' in r else ('png' if 'png' in r else list(r)[0])
                fn = f'{t}_{suf}_{a.tex_res}.{fmt}'; save(r[fmt]['url'], os.path.join(base, fn), r[fmt].get('size')); files.append(f'Textures/PolyHaven/{t}/{fn}')
            meta(t, 'texture', files); print('texture', t, flush=True)
        except Exception as e: print('FAILED texture', t, e, flush=True)
    for h in HDRIS:
        try:
            f = json.loads(get(f'https://api.polyhaven.com/files/{h}')); r = f['hdri']['2k']['hdr']
            fn = f'{h}_2k.hdr'; save(r['url'], os.path.join(out, 'HDRI', fn), r.get('size'))
            meta(h, 'hdri', [f'HDRI/{fn}']); print('hdri', h, flush=True)
        except Exception as e: print('FAILED hdri', h, e, flush=True)
    os.makedirs(os.path.join(out, '_manifests'), exist_ok=True)
    json.dump(entries, open(os.path.join(out, '_manifests', 'polyhaven.json'), 'w'), indent=1)
    print('done', len(entries))

if __name__ == '__main__': main()
