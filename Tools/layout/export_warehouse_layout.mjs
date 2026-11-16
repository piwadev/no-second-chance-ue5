// Exports the browser game's Warehouse 01 layout (js/map.js of piwadev/no-second-chance) to JSON so the
// Unreal bootstrap (Content/Python/nsc_build_map.py) rebuilds the same map. Usage:
//   node Tools/layout/export_warehouse_layout.mjs <path/to/no-second-chance/js/map.js> > Content/Python/warehouse_layout.json
import fs from 'fs';
const src = fs.readFileSync(process.argv[2], 'utf8');
const grab = (start, end) => { const a = src.indexOf(start); const b = src.indexOf(end, a); return src.slice(a + start.length, b); };
const body = (name) => { const a = src.indexOf(`  ${name}() {`); let i = src.indexOf('{', a), d = 0, j = i; for (; j < src.length; j++) { if (src[j] === '{') d++; else if (src[j] === '}' && --d === 0) break; } return src.slice(i + 1, j); };
const consts = {};
for (const n of ['BOUNDS', 'SITES', 'SPAWNS', 'HOLDS', 'ROUTES', 'AREAS']) {
  const m = src.match(new RegExp(`export const ${n} = ([\\s\\S]*?);\\n(?:export|function|const)`));
  consts[n] = Function(`return (${m[1].replace(/\/\/[^\n]*/g, '')})`)();
}
const out = { units: 'browser metres; UE: X = -z*100, Y = x*100, Z = y*100', ...consts, items: [] };
const rec = (o) => out.items.push(o);
let seed = 1; const rnd = () => { seed = (seed * 16807) % 2147483647; return (seed - 1) / 2147483646; };
const absorb = new Proxy(function () {}, { get: (t, k) => (k === Symbol.toPrimitive ? () => 1 : absorb), apply: () => absorb, construct: () => absorb, set: () => true });
const THREE = absorb;
const fitModel = (key, sx, sy, sz, o = {}) => ({ __model: key, sx, sy, sz, rotY: o.rotY || 0, tint: o.tint });
const self = {
  mats: new Proxy({}, { get: (t, k) => String(k) }),
  colliders: { push: (c) => { if (c.kind === 'barrier') rec({ t: 'prepBarrier', min: c.min, max: c.max }); else if (c.kind === 'solid' && c.max) rec({ t: 'rackCollider', min: c.min, max: c.max }); } },
  scene: { add: () => {} }, lamps: [], meshes: [], shelfLevels: [0.02, 0.5],
  box(cx, cz, sx, sz, h, mat, y0 = 0) { rec({ t: 'box', cx, cz, sx, sz, h, y0, mat }); },
  wallX(x, z0, z1, gaps = [], h = 5.5, t = 0.6) { this.wallRun('x', x, z0, z1, gaps, h, t); },
  wallZ(z, x0, x1, gaps = [], h = 5.5, t = 0.6) { this.wallRun('z', z, x0, x1, gaps, h, t); },
  wallRun(axis, c, a0, a1, gaps, h, t) {
    const segs = []; let s = a0; for (const [g0, g1] of gaps.sort((p, q) => p[0] - q[0])) { if (g0 > s) segs.push([s, g0]); s = g1; } if (s < a1) segs.push([s, a1]);
    const put = (p0, p1, y0, hh, mat) => { const L = p1 - p0, m = (p0 + p1) / 2; if (axis === 'x') this.box(c, m, t, L, hh, mat, y0); else this.box(m, c, L, t, hh, mat, y0); };
    for (const [p0, p1] of segs) put(p0, p1, 0, h, 'wall');
    for (const [g0, g1] of gaps) { put(g0, g1, 3.2, h - 3.2, 'wall'); put(g0, g1, 3.0, 0.2, 'hazard'); rec({ t: 'door', axis, c, a0: g0, a1: g1 }); }
  },
  solid(cx, cz, sx, sz, h, y0 = 0) { rec({ t: 'solid', cx, cz, sx, sz, h, y0 }); },
  put(obj, x, y, z, rotY = 0) { if (obj && obj.__model) rec({ t: 'model', key: obj.__model, x, y, z, sx: obj.sx, sy: obj.sy, sz: obj.sz, rotY: obj.rotY + rotY, tint: obj.tint }); return absorb; },
  container(cx, cz, alongZ, colorIdx, y0 = 0) { rec({ t: 'container', cx, cz, alongZ, colorIdx, y0 }); },
  crate(cx, cz, s = 1.2, stack = 1, y0 = 0) { rec({ t: 'crate', cx, cz, s, stack, y0, kind: Math.floor(Math.abs(Math.sin(cx * 12.9898 + cz * 78.233) * 43758.5453)) % 3 }); },
  pallet(cx, cz, sx, sz, h) { rec({ t: 'pallet', cx, cz, sx, sz, h }); },
  rack(x, z0, z1) { rec({ t: 'rack', x, z0, z1 }); },
  lamp(x, z, intensity = 22, withLight = true) { rec({ t: 'lamp', x, z, intensity, withLight }); },
  decor(key, x, z, sx, sy, sz, rotY = 0, collide = false, y0 = 0) { rec({ t: 'decor', key, x, z, sx, sy, sz, rotY, collide, y0 }); },
  sign(text, x, y, z, rotY) { rec({ t: 'sign', text, x, y, z, rotY }); },
  barrier(x, z) { rec({ t: 'barrier', x, z }); },
  decorate() { Function('THREE', 'fitModel', 'rnd', 'BOUNDS', 'SITES', body('decorate')).call(this, THREE, fitModel, rnd, consts.BOUNDS, consts.SITES); },
};
Function('THREE', 'fitModel', 'rnd', 'BOUNDS', 'SITES', 'siteTexture', 'signTexture', 'boxGeo', 'SHAFT_MAT', body('build'))
  .call(self, THREE, fitModel, rnd, consts.BOUNDS, consts.SITES, absorb, absorb, absorb, absorb);
process.stdout.write(JSON.stringify(out, null, 0).replace(/\},\{/g, '},\n{'));
