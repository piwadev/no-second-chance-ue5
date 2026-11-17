// End-to-end test of the launcher against a local mock GitHub release server (nothing is published).
// Run from launcher/:  node test/e2e.mjs   (needs playwright-core: PLAYWRIGHT_CORE=/path/to/playwright-core; Linux electron)
// The "game" is a fake Unreal build (NoSecondChance.exe + version.json + pak); NSC_GAME_CMD runs a node stand-in that
// writes a GVAS save slot, so the update / play / backup / rollback flow is exercised without Windows.
import { createRequire } from 'module'; import http from 'http'; import fs from 'fs'; import path from 'path'; import os from 'os'; import crypto from 'crypto';
import { execFileSync } from 'child_process';
const require = createRequire(import.meta.url);
const { _electron } = require(process.env.PLAYWRIGHT_CORE || '/workspace/kotools/node_modules/playwright-core');
const ROOT = path.resolve(path.dirname(new URL(import.meta.url).pathname), '..');
const TMP = fs.mkdtempSync(path.join(os.tmpdir(), 'nsctest-')); const results = []; let failed = 0;
const ok = (name, cond, extra = '') => { results.push([cond, name, extra]); if (!cond) failed++; console.log((cond ? 'PASS ' : 'FAIL ') + name + (extra ? '  — ' + extra : '')); };
const sleep = ms => new Promise(r => setTimeout(r, ms));
const sha = f => crypto.createHash('sha256').update(fs.readFileSync(f)).digest('hex');

function buildVersion(ver, parts = 0) {
  const d = path.join(TMP, 'src-' + ver), o = path.join(TMP, 'out-' + ver); fs.mkdirSync(path.join(d, 'NoSecondChance/Content/Paks'), { recursive: true }); fs.mkdirSync(o, { recursive: true });
  fs.writeFileSync(path.join(d, 'NoSecondChance.exe'), 'MZ fake unreal exe ' + ver);
  fs.writeFileSync(path.join(d, 'version.json'), '\uFEFF' + JSON.stringify({ version: ver }));   // PowerShell 5 writes a BOM
  fs.writeFileSync(path.join(d, 'NoSecondChance/Content/Paks/NoSecondChance-Windows.pak'), crypto.randomBytes(3 * 1024 * 1024));
  const zip = path.join(o, `NSC-Game-${ver}.zip`);
  execFileSync('python3', ['-c', 'import shutil,sys; shutil.make_archive(sys.argv[1][:-4], "zip", sys.argv[2])', zip, d]);
  const game = { file: path.basename(zip), sha256: sha(zip), size: fs.statSync(zip).size };
  if (parts) {
    const buf = fs.readFileSync(zip), n = Math.ceil(buf.length / parts); game.parts = [];
    for (let i = 0; i < parts; i++) { const f = `${game.file}.${String(i + 1).padStart(3, '0')}`; fs.writeFileSync(path.join(o, f), buf.subarray(i * n, (i + 1) * n)); game.parts.push({ file: f, sha256: sha(path.join(o, f)), size: fs.statSync(path.join(o, f)).size }); }
    fs.unlinkSync(zip);
  }
  const manifest = { name: 'NO SECOND CHANCE', version: ver, exe: 'NoSecondChance.exe', minLauncher: '2.0.0', notes: '', game,
    launcher: { version: ver === '0.2.0' ? '2.0.1' : '2.0.0', file: 'NSC-Launcher-Setup.exe', sha256: '0'.repeat(64), size: 10 } };
  fs.writeFileSync(path.join(o, 'manifest.json'), JSON.stringify(manifest));
  return { dir: o, manifest };
}
const R = { '0.1.0': buildVersion('0.1.0'), '0.2.0': buildVersion('0.2.0', 3) };
let serve = '0.1.0', tamper = false;
const srv = http.createServer((req, res) => {
  const base = `http://127.0.0.1:${srv.address().port}`;
  if (req.url === '/api/latest') {
    const v = serve, m = R[v].manifest, files = ['manifest.json', 'NSC-Launcher-Setup.exe', ...(m.game.parts ? m.game.parts.map(p => p.file) : [m.game.file])];
    res.setHeader('content-type', 'application/json');
    return res.end(JSON.stringify({ tag_name: 'v' + v, name: 'NO SECOND CHANCE ' + v, published_at: new Date().toISOString(), html_url: 'https://github.com/piwadev/no-second-chance-ue5/releases/tag/v' + v,
      body: `## What's new in ${v}\n- Unreal build <b>escaped</b>`, assets: files.map(f => ({ name: f, browser_download_url: `${base}/dl/${v}/${f}`, size: 1 })) }));
  }
  const m = /^\/dl\/([\d.]+)\/(.+)$/.exec(req.url);
  if (m) {
    const f = path.join(R[m[1]].dir, m[2]); if (!fs.existsSync(f)) { res.statusCode = 404; return res.end(); }
    let buf = fs.readFileSync(f);
    if (m[2] === 'manifest.json' && tamper) { const j = JSON.parse(buf); (j.game.parts ? j.game.parts[1] : j.game).sha256 = 'f'.repeat(64); buf = Buffer.from(JSON.stringify(j)); }
    res.setHeader('content-length', buf.length); return res.end(buf);
  }
  res.statusCode = 404; res.end();
});
await new Promise(r => srv.listen(0, '127.0.0.1', r)); const API = `http://127.0.0.1:${srv.address().port}/api/latest`;

const ROAM = path.join(TMP, 'roaming'), LOCAL = path.join(TMP, 'local'), SAVES = path.join(TMP, 'ue-saves'), CLOUD = path.join(TMP, 'cloud'), EXPORT = path.join(TMP, 'export.sav');
const MARK = path.join(TMP, 'game-ran.txt'), STANDIN = path.join(TMP, 'standin.cjs');
fs.writeFileSync(STANDIN, `const fs=require('fs'),path=require('path');fs.writeFileSync(${JSON.stringify(MARK)},process.cwd());fs.mkdirSync(${JSON.stringify(SAVES)},{recursive:true});
const b=Buffer.alloc(64);b.write('GVAS');b.write('NSC '+Date.now(),8);fs.writeFileSync(path.join(${JSON.stringify(SAVES)},'NSCSettings.sav'),b);setTimeout(()=>process.exit(0),1200);`);
async function start(env = {}) {
  const app = await _electron.launch({ executablePath: path.join(ROOT, 'node_modules/electron/dist/electron'), cwd: ROOT, timeout: 30000,
    args: ['.', '--no-sandbox', '--use-gl=angle', '--use-angle=swiftshader', '--enable-unsafe-swiftshader'],
    env: { ...process.env, NSC_TEST: '1', NSC_NO_BUNDLE: '1', NSC_ROAMING: ROAM, NSC_LOCAL: LOCAL, NSC_UE_SAVES: SAVES, NSC_API: API, NSC_GAME_CMD: `${process.execPath}|${STANDIN}`, ...env } });
  const launcher = await app.firstWindow(); const errors = []; launcher.on('pageerror', e => errors.push('launcher: ' + e)); launcher.on('console', m => { if (m.type() === 'error') errors.push('console: ' + m.text()); });
  await launcher.waitForLoadState('domcontentloaded'); return { app, launcher, errors };
}
const txt = (p, id) => p.locator('#' + id).innerText({ timeout: 5000 });
async function waitStatus(p, re, ms = 30000) { const t0 = Date.now(); while (Date.now() - t0 < ms) { const s = await txt(p, 'statusText'); if (re.test(s)) return s; await sleep(150); } throw new Error('status timeout; last=' + await txt(p, 'statusText')); }
const state = () => JSON.parse(fs.readFileSync(path.join(LOCAL, 'state.json')));
const click = (p, sel) => p.click(sel, { timeout: 5000 });
let app;
try {
  let launcher, errors;
  ({ app, launcher, errors } = await start());
  await waitStatus(launcher, /0\.1\.0 installed|up to date/i, 40000);
  ok('1. first run installs v0.1.0 from the mock release', (await txt(launcher, 'vInstalled')) === 'v0.1.0', await txt(launcher, 'statusText'));
  ok('1b. NoSecondChance.exe + pak streamed into local/game/0.1.0', fs.existsSync(path.join(LOCAL, 'game/0.1.0/NoSecondChance.exe')) && fs.statSync(path.join(LOCAL, 'game/0.1.0/NoSecondChance/Content/Paks/NoSecondChance-Windows.pak')).size === 3 * 1024 * 1024);
  ok('1c. release notes rendered with HTML escaped', (await launcher.locator('#notes').innerHTML()).includes('&lt;b&gt;escaped&lt;/b&gt;'));
  await launcher.screenshot({ path: path.join(TMP, 'launcher-1.png') });

  await click(launcher, '#btnPlay'); const t0 = Date.now(); while (!fs.existsSync(MARK) && Date.now() - t0 < 10000) await sleep(100);
  ok('2. Play starts the game process in the installed version folder', fs.existsSync(MARK) && fs.readFileSync(MARK, 'utf8') === path.join(LOCAL, 'game', '0.1.0'), fs.existsSync(MARK) ? fs.readFileSync(MARK, 'utf8') : 'not started');
  await sleep(2500);
  const bdir = path.join(ROAM, 'saves/backups'), backups = fs.existsSync(bdir) ? fs.readdirSync(bdir) : [];
  ok('2b. save slots backed up when the game exits', backups.some(b => fs.existsSync(path.join(bdir, b, 'NSCSettings.sav'))), backups.join(','));
  const saveHash = sha(path.join(SAVES, 'NSCSettings.sav'));

  await app.close(); ({ app, launcher, errors } = await start({ NSC_TEST_PATH: EXPORT, NSC_TEST_DIR: CLOUD }));
  await waitStatus(launcher, /up to date/i);
  await click(launcher, '#btnSettings'); await click(launcher, '#btnBackup'); await sleep(600);
  ok('3. "Back up save" exports the .sav byte-identical', fs.existsSync(EXPORT) && sha(EXPORT) === saveHash);
  await click(launcher, '#btnChooseCloud'); await sleep(400);
  await click(launcher, '#btnCloud'); await sleep(600);
  ok('3b. "Cloud backup" writes NSC-save-latest.sav', fs.existsSync(path.join(CLOUD, 'NSC-save-latest.sav')));
  fs.writeFileSync(path.join(SAVES, 'NSCSettings.sav'), 'garbage');
  await click(launcher, '#btnRestore'); await sleep(600);
  ok('3c. "Restore save" brings the exported slot back', sha(path.join(SAVES, 'NSCSettings.sav')) === saveHash);
  await click(launcher, '#btnCloseSettings');

  serve = '0.2.0'; await click(launcher, '#btnCheck'); await waitStatus(launcher, /0\.2\.0 is available/i, 15000);
  ok('4. update check sees v0.2.0', (await txt(launcher, 'vLatest')) === 'v0.2.0');
  ok('4b. newer launcher (2.0.1) advertised', await launcher.locator('#launcherBanner').isVisible());
  await click(launcher, '#btnUpdate'); await waitStatus(launcher, /0\.2\.0 installed/i, 40000);
  const st = state();
  ok('4c. 3-part download joined, verified, installed; v0.1.0 kept', st.current === '0.2.0' && st.previous === '0.1.0' && fs.existsSync(path.join(LOCAL, 'game/0.2.0/NoSecondChance.exe')), JSON.stringify(st));
  ok('4d. no leftover parts / zips in downloads', fs.readdirSync(path.join(LOCAL, 'downloads')).length === 0, fs.readdirSync(path.join(LOCAL, 'downloads')).join(','));
  ok('4e. save untouched by the update', sha(path.join(SAVES, 'NSCSettings.sav')) === saveHash);

  await click(launcher, '#btnSettings'); await click(launcher, '#btnRollback'); await sleep(800);
  ok('5. rollback switches to 0.1.0', state().current === '0.1.0' && (await txt(launcher, 'vInstalled')) === 'v0.1.0');
  await click(launcher, '#btnCloseSettings');

  fs.rmSync(path.join(LOCAL, 'game', '0.2.0'), { recursive: true }); fs.writeFileSync(path.join(LOCAL, 'state.json'), JSON.stringify({ current: '0.1.0', previous: null }));
  tamper = true; await click(launcher, '#btnCheck'); await waitStatus(launcher, /available/i, 15000); await click(launcher, '#btnUpdate');
  const es = await waitStatus(launcher, /checksum mismatch/i, 30000);
  ok('6. checksum mismatch on a part rejected, nothing installed', !fs.existsSync(path.join(LOCAL, 'game/0.2.0')) && state().current === '0.1.0', es.slice(0, 80));
  tamper = false;
  ok('6b. launcher UI has no JS errors', errors.length === 0, errors.join(' | '));
  await app.close();

  await new Promise(r => srv.close(r));
  ({ app, launcher, errors } = await start()); await waitStatus(launcher, /could not reach/i, 20000);
  ok('7. offline: friendly message and Play enabled', await launcher.locator('#btnPlay').isEnabled());
  await launcher.screenshot({ path: path.join(TMP, 'launcher-offline.png') });
  await app.close(); app = null;
} catch (e) { failed++; console.log('TEST ERROR', e.message); try { if (app) await app.close(); } catch { /* gone */ } }
finally { try { srv.close(); } catch { /* closed */ } }
fs.writeFileSync(path.join(TMP, 'results.json'), JSON.stringify(results, null, 1));
console.log(`\n${results.length - failed}/${results.length} passed  (artifacts: ${TMP})`);
process.exit(failed ? 1 : 0);
