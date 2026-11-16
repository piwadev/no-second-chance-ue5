"""NO SECOND CHANCE editor bootstrap. Run by Tools/Setup.bat:
    UnrealEditor.exe NoSecondChance.uproject -ExecutePythonScript="<project>/Content/Python/nsc_setup.py"
Environment:
    NSC_SETUP_ONLY = all (default) | import | map     NSC_SETUP_QUIT = 1 -> close the editor when done
Imports SourceAssets/ into /Game/NSC, builds the materials, the Warehouse and main-menu maps, input assets, and saves.
The C++ game also runs without any of these assets (placeholder shapes), so a failed step never blocks a build."""
import glob
import os
import sys
import time
import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import nsc_common as C                      # noqa: E402
import nsc_import as I                      # noqa: E402
import nsc_materials as M                   # noqa: E402
import nsc_build_map as BM                  # noqa: E402
from nsc_common import ROOT, log, warn, step, load, src   # noqa: E402

for mod in (C, I, M, BM):                   # re-running inside one editor session picks up edits
    try:
        import importlib
        importlib.reload(mod)
    except Exception:
        pass


@step
def create_input_assets():
    """IA_* + IMC_Default mirroring UNSCInputSet (the C++ builds the same set at runtime; these are for rebinding/reference)."""
    folder = ROOT + '/Input'
    VT = unreal.InputActionValueType
    actions = {
        'Move': (VT.AXIS2D, []), 'Look': (VT.AXIS2D, ['Mouse2D']), 'Jump': (VT.BOOLEAN, ['SpaceBar']),
        'Sprint': (VT.BOOLEAN, ['LeftShift']), 'Walk': (VT.BOOLEAN, ['LeftAlt']), 'Crouch': (VT.BOOLEAN, ['LeftControl', 'C']),
        'Fire': (VT.BOOLEAN, ['LeftMouseButton']), 'Aim': (VT.BOOLEAN, ['RightMouseButton']), 'Reload': (VT.BOOLEAN, ['R']),
        'SlotPrimary': (VT.BOOLEAN, ['One']), 'SlotSecondary': (VT.BOOLEAN, ['Two']), 'SlotSmoke': (VT.BOOLEAN, ['Three']),
        'SlotFlash': (VT.BOOLEAN, ['Four']), 'SlotFrag': (VT.BOOLEAN, ['Five']), 'CycleWeapon': (VT.AXIS1D, ['MouseWheelAxis']),
        'LeanLeft': (VT.BOOLEAN, ['Q']), 'LeanRight': (VT.BOOLEAN, ['E']), 'Interact': (VT.BOOLEAN, ['F']), 'Drag': (VT.BOOLEAN, ['G']),
        'Bandage': (VT.BOOLEAN, ['X']), 'Tourniquet': (VT.BOOLEAN, ['V']), 'BuyMenu': (VT.BOOLEAN, ['B']),
        'Scoreboard': (VT.BOOLEAN, ['Tab']), 'Pause': (VT.BOOLEAN, ['Escape', 'P']),
    }
    ia = {}
    for name, (vt, _) in actions.items():
        a = C.create_asset('IA_' + name, folder, unreal.InputAction, unreal.InputAction_Factory())
        a.set_editor_property('value_type', vt)
        ia[name] = a
    imc = C.create_asset('IMC_Default', folder, unreal.InputMappingContext, unreal.InputMappingContext_Factory())

    def key(n):
        k = unreal.Key()
        try:
            k.set_editor_property('key_name', n)
        except Exception:
            k.import_text(n)
        return k
    for name, (_, keys) in actions.items():
        for k in keys:
            imc.map_key(ia[name], key(k))
    # WASD -> 2D move with swizzle/negate modifiers
    for k, neg, swz in (('W', False, True), ('S', True, True), ('D', False, False), ('A', True, False)):
        imc.map_key(ia['Move'], key(k))
    try:
        mappings = imc.get_editor_property('mappings')
        for m in mappings:
            if m.get_editor_property('action') != ia['Move']:
                continue
            kn = str(m.get_editor_property('key').get_editor_property('key_name'))
            mods = []
            if kn in ('W', 'S'):
                mods.append(unreal.new_object(unreal.InputModifierSwizzleAxis, imc))
            if kn in ('S', 'A'):
                mods.append(unreal.new_object(unreal.InputModifierNegate, imc))
            m.set_editor_property('modifiers', mods)
        imc.set_editor_property('mappings', mappings)
    except Exception as e:
        warn('IMC modifiers not set (%s); WASD axes need Swizzle/Negate added by hand if you use IMC_Default' % e)
    for a in list(ia.values()) + [imc]:
        unreal.EditorAssetLibrary.save_loaded_asset(a)


def existing_surfaces():
    res = {}
    for d in glob.glob(src('Textures', 'PolyHaven', '*')):
        sid = os.path.basename(d)
        base = '%s/Textures/%s/T_%s' % (ROOT, sid, sid)
        res[sid] = {'bc': load(base + '_BC'), 'n': load(base + '_N'), 'orm': load(base + '_ORM')}
    return res


def existing_materials():
    names = {'surface': 'M_NSC_Surface', 'solid': 'M_NSC_Solid', 'soldier': 'M_NSC_Soldier', 'smoke': 'M_Smoke',
             'emissive': 'M_NSC_Emissive', 'bullethole': 'MD_BulletHole', 'blood': 'MD_Blood'}
    return {k: load(ROOT + '/Materials/' + v) for k, v in names.items()}


def main():
    t0 = time.time()
    only = os.environ.get('NSC_SETUP_ONLY', 'all').lower()
    if not os.path.isdir(C.SOURCE):
        warn('SourceAssets/ not found at %s - run Tools\\Setup.bat (it downloads the asset pack) first' % C.SOURCE)
    log('SourceAssets: ' + C.SOURCE)
    surfaces, mats, hdri = {}, {}, None
    if only in ('all', 'import'):
        surfaces = step(I.import_surfaces)() or {}
        decals = step(I.import_decals)() or {}
        mats = step(M.build_all)(decals) or {}
        step(I.import_weapons)()
        step(I.import_props)(mats, surfaces)
        step(I.import_characters)(mats)
        step(I.import_audio)()
        step(I.import_fonts)()
        hdris = step(I.import_hdri)() or {}
        hdri = hdris.get('HDR_docklands_01_2k')
        create_input_assets()
        C.save_all()
    if only in ('all', 'map'):
        if not surfaces:
            surfaces = existing_surfaces()
        if not mats:
            mats = existing_materials()
        if hdri is None:
            hdri = load(ROOT + '/HDRI/HDR_docklands_01_2k')
        if hdri is not None and not isinstance(hdri, unreal.TextureCube):
            warn('HDRI imported as %s, not a cube map; sky light uses real-time capture' % type(hdri).__name__)
            hdri = None
        step(BM.build_warehouse)(mats, surfaces, hdri)
        step(BM.build_menu)(hdri)
        C.save_all()
    report = os.path.join(C.PROJECT_DIR, 'Saved', 'NSC_Setup_Report.txt')
    os.makedirs(os.path.dirname(report), exist_ok=True)
    with open(report, 'w', encoding='utf-8') as f:
        f.write('NO SECOND CHANCE setup (%.0f s)\nOK: %s\n\nWarnings (%d):\n%s\n' % (
            time.time() - t0, ', '.join(C.REPORT['ok']), len(C.REPORT['warn']), '\n'.join(C.REPORT['warn'])))
    log('done in %.0f s, %d warnings -> %s' % (time.time() - t0, len(C.REPORT['warn']), report))
    if os.environ.get('NSC_SETUP_QUIT') == '1':
        unreal.SystemLibrary.quit_editor()


main()
