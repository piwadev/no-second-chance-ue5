"""Master materials + instances built from code (no hand-made .uasset needed)."""
import unreal
from nsc_common import MEL, ROOT, create_asset, exists, load, log, warn

MAT = ROOT + '/Materials'
MI = ROOT + '/Materials/Instances'
MP = unreal.MaterialProperty

DEFAULT_TEX = '/Engine/EngineResources/DefaultTexture.DefaultTexture'
DEFAULT_NORMAL = '/Engine/EngineMaterials/DefaultNormal.DefaultNormal'
WHITE = '/Engine/EngineResources/WhiteSquareTexture.WhiteSquareTexture'


def new_material(name, folder=MAT):
    return create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())


def ex(mat, cls, x, y, **props):
    e = MEL.create_material_expression(mat, cls, x, y)
    for k, v in props.items():
        e.set_editor_property(k, v)
    return e


def link(a, a_out, b, b_in):
    if not MEL.connect_material_expressions(a, a_out, b, b_in):
        warn('material link failed %s.%s -> %s.%s' % (a.get_name(), a_out, b.get_name(), b_in))


def out(e, e_out, prop):
    if not MEL.connect_material_property(e, e_out, prop):
        warn('material output failed %s.%s -> %s' % (e.get_name(), e_out, prop))


def tex_param(mat, name, x, y, sampler, default):
    return ex(mat, unreal.MaterialExpressionTextureSampleParameter2D, x, y, parameter_name=name,
              sampler_type=sampler, texture=unreal.load_asset(default))


def scalar(mat, name, value, x, y):
    return ex(mat, unreal.MaterialExpressionScalarParameter, x, y, parameter_name=name, default_value=value)


def vector(mat, name, color, x, y):
    return ex(mat, unreal.MaterialExpressionVectorParameter, x, y, parameter_name=name, default_value=unreal.LinearColor(*color))


def mask(mat, x, y, r=False, g=False, b=False, a=False):
    return ex(mat, unreal.MaterialExpressionComponentMask, x, y, r=r, g=g, b=b, a=a)


def finish(mat):
    MEL.recompile_material(mat)
    unreal.EditorAssetLibrary.save_loaded_asset(mat)
    return mat


def build_surface():
    """M_NSC_Surface: world-aligned box mapping (tiles on any scaled cube or prop without UVs), PBR via BC/N/ORM."""
    m = new_material('M_NSC_Surface')
    m.set_editor_property('used_with_static_lighting', False)
    wp = ex(m, unreal.MaterialExpressionWorldPosition, -1500, 0)
    tile = scalar(m, 'TileSize', 300.0, -1500, 150)
    div = ex(m, unreal.MaterialExpressionDivide, -1300, 0); link(wp, '', div, 'A'); link(tile, '', div, 'B')
    xy = mask(m, -1100, -150, r=True, g=True); link(div, '', xy, '')
    yz = mask(m, -1100, 0, g=True, b=True); link(div, '', yz, '')
    xz = mask(m, -1100, 150, r=True, b=True); link(div, '', xz, '')
    n = ex(m, unreal.MaterialExpressionVertexNormalWS, -1500, 350)
    an = ex(m, unreal.MaterialExpressionAbs, -1300, 350); link(n, '', an, '')
    nx = mask(m, -1100, 300, r=True); link(an, '', nx, '')
    nz = mask(m, -1100, 450, b=True); link(an, '', nz, '')
    rx = ex(m, unreal.MaterialExpressionRound, -950, 300); link(nx, '', rx, '')
    rz = ex(m, unreal.MaterialExpressionRound, -950, 450); link(nz, '', rz, '')
    l1 = ex(m, unreal.MaterialExpressionLinearInterpolate, -800, 100); link(xz, '', l1, 'A'); link(yz, '', l1, 'B'); link(rx, '', l1, 'Alpha')
    uv = ex(m, unreal.MaterialExpressionLinearInterpolate, -650, 0); link(l1, '', uv, 'A'); link(xy, '', uv, 'B'); link(rz, '', uv, 'Alpha')
    bc = tex_param(m, 'T_BaseColor', -450, -250, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, DEFAULT_TEX); link(uv, '', bc, 'UVs')
    nm = tex_param(m, 'T_Normal', -450, 50, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL, DEFAULT_NORMAL); link(uv, '', nm, 'UVs')
    orm = tex_param(m, 'T_ORM', -450, 350, unreal.MaterialSamplerType.SAMPLERTYPE_MASKS, WHITE); link(uv, '', orm, 'UVs')
    tint = vector(m, 'Tint', (1, 1, 1, 1), -450, -450)
    tint3 = mask(m, -250, -450, r=True, g=True, b=True); link(tint, '', tint3, '')
    mul = ex(m, unreal.MaterialExpressionMultiply, -150, -250); link(bc, 'RGB', mul, 'A'); link(tint3, '', mul, 'B')
    out(mul, '', MP.MP_BASE_COLOR)
    out(nm, 'RGB', MP.MP_NORMAL)
    rs = scalar(m, 'RoughnessScale', 1.0, -250, 550)
    rmul = ex(m, unreal.MaterialExpressionMultiply, -100, 400); link(orm, 'G', rmul, 'A'); link(rs, '', rmul, 'B')
    out(rmul, '', MP.MP_ROUGHNESS)
    ms = scalar(m, 'MetallicScale', 1.0, -250, 650)
    mmul = ex(m, unreal.MaterialExpressionMultiply, -100, 550); link(orm, 'B', mmul, 'A'); link(ms, '', mmul, 'B')
    out(mmul, '', MP.MP_METALLIC)
    out(orm, 'R', MP.MP_AMBIENT_OCCLUSION)
    return finish(m)


def build_solid():
    """M_NSC_Solid: flat PBR colour (gear and AI-generated props that ship without textures)."""
    m = new_material('M_NSC_Solid')
    c = vector(m, 'BaseColor', (0.3, 0.3, 0.3, 1), -500, 0)
    c3 = mask(m, -300, 0, r=True, g=True, b=True); link(c, '', c3, '')
    out(c3, '', MP.MP_BASE_COLOR)
    out(scalar(m, 'Roughness', 0.7, -300, 150), '', MP.MP_ROUGHNESS)
    out(scalar(m, 'Metallic', 0.0, -300, 250), '', MP.MP_METALLIC)
    m.set_editor_property('used_with_skeletal_mesh', True)
    return finish(m)


def build_soldier():
    """M_NSC_Soldier: skin texture + vertex-colour masks (R clothing, G trousers, B gloves/boots, see
    Tools/blender/merge_character.py). TeamColor/TeamIndex are set at runtime by ANSCCharacter::ApplyTeamLook."""
    m = new_material('M_NSC_Soldier')
    m.set_editor_property('used_with_skeletal_mesh', True)
    m.set_editor_property('two_sided', True)
    bc = tex_param(m, 'T_BaseColor', -900, -300, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, DEFAULT_TEX)
    nm = tex_param(m, 'T_Normal', -900, 100, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL, DEFAULT_NORMAL)
    vc = ex(m, unreal.MaterialExpressionVertexColor, -900, 400)
    team = vector(m, 'TeamColor', (0.25, 0.45, 0.85, 1), -900, -550)
    team3 = mask(m, -700, -550, r=True, g=True, b=True); link(team, '', team3, '')
    bright = scalar(m, 'ShirtBrightness', 0.55, -700, -450)
    shirt = ex(m, unreal.MaterialExpressionMultiply, -500, -500); link(team3, '', shirt, 'A'); link(bright, '', shirt, 'B')
    # fabric detail: a little of the texture luminance on top of the flat uniform colour
    luma = ex(m, unreal.MaterialExpressionDesaturation, -700, -300); link(bc, 'RGB', luma, '')
    detail = ex(m, unreal.MaterialExpressionLinearInterpolate, -500, -300, const_a=0.75, const_b=1.15); link(luma, '', detail, 'Alpha')
    shirt_d = ex(m, unreal.MaterialExpressionMultiply, -350, -450); link(shirt, '', shirt_d, 'A'); link(detail, '', shirt_d, 'B')
    pants = vector(m, 'TrouserColor', (0.055, 0.06, 0.065, 1), -700, -150)
    pants3 = mask(m, -500, -150, r=True, g=True, b=True); link(pants, '', pants3, '')
    pants_d = ex(m, unreal.MaterialExpressionMultiply, -350, -150); link(pants3, '', pants_d, 'A'); link(detail, '', pants_d, 'B')
    gear = vector(m, 'GearColor', (0.02, 0.02, 0.02, 1), -700, 0)
    gear3 = mask(m, -500, 0, r=True, g=True, b=True); link(gear, '', gear3, '')
    l1 = ex(m, unreal.MaterialExpressionLinearInterpolate, -200, -350); link(bc, 'RGB', l1, 'A'); link(shirt_d, '', l1, 'B'); link(vc, 'R', l1, 'Alpha')
    l2 = ex(m, unreal.MaterialExpressionLinearInterpolate, -50, -300); link(l1, '', l2, 'A'); link(pants_d, '', l2, 'B'); link(vc, 'G', l2, 'Alpha')
    l3 = ex(m, unreal.MaterialExpressionLinearInterpolate, 100, -250); link(l2, '', l3, 'A'); link(gear3, '', l3, 'B'); link(vc, 'B', l3, 'Alpha')
    out(l3, '', MP.MP_BASE_COLOR)
    out(nm, 'RGB', MP.MP_NORMAL)
    rough = ex(m, unreal.MaterialExpressionLinearInterpolate, 100, 200, const_a=0.55, const_b=0.85); link(vc, 'R', rough, 'Alpha')
    out(rough, '', MP.MP_ROUGHNESS)
    scalar(m, 'TeamIndex', 0.0, -900, 650)   # read by gameplay code; kept so the parameter exists
    return finish(m)


def build_smoke():
    m = new_material('M_Smoke')
    m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property('two_sided', True)
    col = vector(m, 'SmokeColor', (0.62, 0.63, 0.64, 1), -600, -100)
    col3 = mask(m, -400, -100, r=True, g=True, b=True); link(col, '', col3, '')
    out(col3, '', MP.MP_BASE_COLOR)
    fres = ex(m, unreal.MaterialExpressionFresnel, -700, 150, exponent=1.6)
    inv = ex(m, unreal.MaterialExpressionOneMinus, -500, 150); link(fres, '', inv, '')
    op = scalar(m, 'Opacity', 0.92, -500, 300)
    mul = ex(m, unreal.MaterialExpressionMultiply, -300, 200); link(inv, '', mul, 'A'); link(op, '', mul, 'B')
    out(mul, '', MP.MP_OPACITY)
    out(scalar(m, 'Roughness', 1.0, -300, 400), '', MP.MP_ROUGHNESS)
    return finish(m)


def build_decal(name, texture):
    m = new_material(name)
    m.set_editor_property('material_domain', unreal.MaterialDomain.MD_DEFERRED_DECAL)
    m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
    t = ex(m, unreal.MaterialExpressionTextureSampleParameter2D, -500, 0, parameter_name='T_Decal',
           sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, texture=texture or unreal.load_asset(DEFAULT_TEX))
    out(t, 'RGB', MP.MP_BASE_COLOR)
    out(t, 'A', MP.MP_OPACITY)
    out(scalar(m, 'Roughness', 0.55, -300, 250), '', MP.MP_ROUGHNESS)
    return finish(m)


def build_emissive():
    m = new_material('M_NSC_Emissive')
    m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    c = vector(m, 'Color', (1.0, 0.85, 0.6, 1), -500, 0)
    c3 = mask(m, -300, 0, r=True, g=True, b=True); link(c, '', c3, '')
    s = scalar(m, 'Intensity', 20.0, -300, 150)
    mul = ex(m, unreal.MaterialExpressionMultiply, -150, 50); link(c3, '', mul, 'A'); link(s, '', mul, 'B')
    out(mul, '', MP.MP_EMISSIVE_COLOR)
    return finish(m)


def make_mi(name, parent, scalars=None, vectors=None, textures=None, folder=MI):
    mi = create_asset(name, folder, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    MEL.set_material_instance_parent(mi, parent)
    for k, v in (scalars or {}).items():
        MEL.set_material_instance_scalar_parameter_value(mi, k, float(v))
    for k, v in (vectors or {}).items():
        MEL.set_material_instance_vector_parameter_value(mi, k, unreal.LinearColor(*v))
    for k, v in (textures or {}).items():
        if v is not None:
            MEL.set_material_instance_texture_parameter_value(mi, k, v)
    MEL.update_material_instance(mi)
    unreal.EditorAssetLibrary.save_loaded_asset(mi)
    return mi


def build_all(decal_textures):
    mats = {
        'surface': build_surface(),
        'solid': build_solid(),
        'soldier': build_soldier(),
        'smoke': build_smoke(),
        'emissive': build_emissive(),
    }
    mats['bullethole'] = build_decal('MD_BulletHole', decal_textures.get('bullethole'))
    mats['blood'] = build_decal('MD_Blood', decal_textures.get('blood'))
    log('materials built: ' + ', '.join(mats))
    return mats
