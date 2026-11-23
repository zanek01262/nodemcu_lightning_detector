/* =====================================================================
   webpages.h  —  PROGMEM HTML for the setup page and the dashboard
   ---------------------------------------------------------------------
   Theme "Night Storm": deep navy sky, lightning-yellow accent, violet
   storm glow. Strike colours run hot to cold with age (white -> yellow
   -> orange -> red -> magenta -> violet), the way lightning maps do.

   SETUP_PAGE — AP mode. Fully self-contained (no fonts, no CDNs): phones
                on the setup network have no internet, and a blocking
                fetch hangs the page.
   DASH_PAGE  — STA mode. Loads Leaflet + map tiles from the internet
                after the page has rendered, so the log, chart and
                settings still work on a network with no internet.
   ===================================================================== */
#ifndef WEBPAGES_H
#define WEBPAGES_H

#include <Arduino.h>

// ----------------------------------------------------------------------
//  SETUP PAGE  (AP mode)
// ----------------------------------------------------------------------
static const char SETUP_PAGE[] PROGMEM = R"HTML(<!DOCTYPE html><html lang="en"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Lightning Detector &mdash; Setup</title>
<style>
  :root{
    --bg:#0a0d16; --panel:#121828; --panel2:#182036; --line:#243052;
    --ink:#e9edf7; --dim:#a3aecb; --faint:#6c7899;
    --bolt:#ffd84a; --bolt-soft:rgba(255,216,74,.12); --volt:#8f7dff;
    --good:#4fd18b; --bad:#ff7b7b;
    --body:system-ui,-apple-system,'Segoe UI',Roboto,sans-serif;
  }
  *{box-sizing:border-box}
  html,body{margin:0;min-height:100%}
  body{background:var(--bg);color:var(--ink);font-family:var(--body);min-height:100vh;
    background-image:radial-gradient(900px 500px at 50% -15%,rgba(143,125,255,.22),transparent 65%);
    display:flex;align-items:center;justify-content:center;padding:20px 16px;
    -webkit-font-smoothing:antialiased}
  .panel{width:100%;max-width:420px;background:var(--panel);border:1px solid var(--line);
    border-radius:16px;padding:28px 24px 24px;box-shadow:0 24px 70px rgba(0,0,0,.5)}
  .brand{display:flex;align-items:center;gap:13px}
  .logo{width:44px;height:44px;flex:none;border-radius:12px;background:var(--bolt-soft);
    display:grid;place-items:center;box-shadow:inset 0 0 0 1px rgba(255,216,74,.25)}
  h1{font-size:21px;margin:0;font-weight:700;letter-spacing:-.01em}
  .sub{color:var(--dim);font-size:13.5px;margin:3px 0 0}
  .lbl{display:flex;align-items:center;justify-content:space-between;margin:22px 0 8px;
    font-size:11.5px;font-weight:700;letter-spacing:.09em;text-transform:uppercase;color:var(--bolt)}
  .ghost{color:var(--dim);font:500 13px var(--body);background:none;border:0;cursor:pointer;
    padding:4px 0;text-transform:none;letter-spacing:0}
  .ghost:hover{color:var(--bolt)}
  .nets{border:1px solid var(--line);border-radius:11px;max-height:232px;overflow:auto;background:var(--panel2)}
  .nets button{display:flex;width:100%;align-items:center;gap:10px;background:none;border:0;
    border-bottom:1px solid var(--line);color:var(--ink);font:500 15px var(--body);
    padding:13px 14px;cursor:pointer;text-align:left}
  .nets button:last-child{border-bottom:0}
  .nets button:hover{background:rgba(255,255,255,.03)}
  .nets button.sel{background:var(--bolt-soft);color:var(--bolt)}
  .nets .ss{flex:1;min-width:0;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
  .nets .empty{padding:16px 14px;color:var(--faint);font-size:14px}
  .lock{display:inline-flex;color:var(--faint)}
  .sig{display:inline-flex;align-items:flex-end;gap:2px;height:14px}
  .sig i{display:block;width:3px;border-radius:1px;background:var(--line)}
  .sig i.on{background:var(--dim)}
  .sel .sig i.on{background:var(--bolt)}
  input[type=password],input[type=text]{width:100%;background:var(--panel2);border:1px solid var(--line);
    border-radius:11px;color:var(--ink);font:15px var(--body);padding:13px 14px;outline:none}
  input:focus{border-color:var(--bolt)}
  #manualWrap{display:none;margin-top:10px}
  .row{display:flex;justify-content:flex-end;margin-top:6px}
  .go{margin-top:22px;width:100%;padding:14px;border-radius:11px;border:0;cursor:pointer;
    background:var(--bolt);color:#1d1600;font:700 15.5px var(--body);letter-spacing:.02em}
  .go:hover{background:#ffe27a}
  .go:disabled{opacity:.35;cursor:not-allowed}
  .status{margin-top:14px;font-size:13.5px;min-height:20px;line-height:1.5;color:var(--dim)}
  .status.ok{color:var(--good)} .status.err{color:var(--bad)}
  .note{margin-top:18px;padding-top:14px;border-top:1px solid var(--line);font-size:12.5px;
    color:var(--faint);line-height:1.5}
  .hint{margin-top:7px;font-size:12.5px;color:var(--faint);line-height:1.45}
  .banner{margin-top:18px;padding:11px 13px;border-radius:11px;font-size:13.5px;line-height:1.45;display:none}
  .banner.err{display:block;background:rgba(255,123,123,.1);border:1px solid rgba(255,123,123,.35);color:#ffc2c2}
  .banner.info{display:block;background:var(--bolt-soft);border:1px solid rgba(255,216,74,.3);color:var(--ink)}
</style></head>
<body>
  <main class="panel">
    <div class="brand">
      <div class="logo"><svg width="22" height="22" viewBox="0 0 24 24" aria-hidden="true"><path d="M13.5 2 4 13.5h6.5L9 22l10-12.5h-6.6L13.5 2z" fill="#ffd84a"/></svg></div>
      <div><h1>Lightning Detector</h1><p class="sub">Connect your detector to Wi-Fi</p></div>
    </div>
    <div class="banner" id="banner" role="status"></div>

    <div class="lbl"><span>Network</span><button class="ghost" id="rescan" type="button">Rescan</button></div>
    <div class="nets" id="nets"><div class="empty">Scanning&hellip;</div></div>
    <div class="row"><button class="ghost" id="hidden" type="button">Hidden network?</button></div>
    <div id="manualWrap"><input type="text" id="manual" placeholder="Network name (SSID)" autocomplete="off" autocapitalize="off" spellcheck="false"></div>

    <div class="lbl"><span>Password</span><button class="ghost" id="showpw" type="button">Show</button></div>
    <input type="password" id="pass" placeholder="Wi-Fi password" autocomplete="off">
    <div class="hint" id="passHint" style="display:none"></div>

    <div class="lbl"><span>Device password</span></div>
    <input type="text" id="devUser" autocomplete="username" value="lightning" hidden>
    <input type="password" id="dev" placeholder="8 or more characters" autocomplete="new-password">
    <div class="hint" id="devHint">Protects the detector: you'll need it to change settings or
      update the firmware. Anyone on your network can still view the dashboard.</div>

    <button class="go" id="save" type="button" disabled>Connect</button>
    <div class="status" id="st" role="status"></div>

    <div class="note">After it connects, rejoin your home Wi-Fi and open
      <b>http://lightning.local</b> (or the device's IP from your router).
      On iPhone, turn off Cellular Data if this page won't load.</div>
  </main>
<script>
  let chosen = null;
  const $ = id => document.getElementById(id);
  const nets = $('nets'), save = $('save'), st = $('st'), pass = $('pass'), manual = $('manual'), dev = $('dev');
  // From /info: is a device password set already, which network is saved.
  let INFO = { hasPass: 0, ssid: '', failed: 0, forced: 0 };
  function validate(){
    const n = dev.value.length;
    const devOk = INFO.hasPass ? (n === 0 || n >= 8) : n >= 8;
    save.disabled = !chosen || !devOk || n > 64;
    // Same network as saved: its password can stay as it is.
    const keep = !!(chosen && INFO.ssid && chosen === INFO.ssid);
    pass.placeholder = keep ? 'Leave blank to keep the saved password' : 'Wi-Fi password';
    $('passHint').style.display = keep ? 'block' : 'none';
    $('passHint').textContent = keep ? 'This network is already saved. Type a password only if it changed.' : '';
  }
  const LOCK = '<svg width="12" height="12" viewBox="0 0 24 24" aria-label="secured"><path fill="currentColor" d="M6 10V8a6 6 0 1 1 12 0v2h1a1 1 0 0 1 1 1v10a1 1 0 0 1-1 1H5a1 1 0 0 1-1-1V11a1 1 0 0 1 1-1h1zm2 0h8V8a4 4 0 1 0-8 0v2z"/></svg>';

  function level(r){ return r >= -55 ? 4 : r >= -65 ? 3 : r >= -75 ? 2 : 1; }
  function bars(n){
    const s = document.createElement('span'); s.className = 'sig';
    for (let i = 1; i <= 4; i++){ const b = document.createElement('i'); b.style.height = (i * 3 + 2) + 'px'; if (i <= n) b.className = 'on'; s.appendChild(b); }
    return s;
  }
  function message(t){ nets.innerHTML = ''; const d = document.createElement('div'); d.className = 'empty'; d.textContent = t; nets.appendChild(d); }
  function render(list){
    // Mesh systems broadcast one name from several access points: keep the strongest.
    const best = {};
    list.forEach(n => { if (n.ssid && (!best[n.ssid] || best[n.ssid].rssi < n.rssi)) best[n.ssid] = n; });
    const arr = Object.values(best).sort((a, b) => b.rssi - a.rssi);
    if (!arr.length){ message('No networks found. Try Rescan.'); return; }
    nets.innerHTML = '';
    arr.forEach(n => {
      const b = document.createElement('button'); b.type = 'button';
      const ss = document.createElement('span'); ss.className = 'ss'; ss.textContent = n.ssid; b.appendChild(ss);
      if (n.lock){ const l = document.createElement('span'); l.className = 'lock'; l.innerHTML = LOCK; b.appendChild(l); }
      b.appendChild(bars(level(n.rssi)));
      b.onclick = () => {
        chosen = n.ssid; manual.value = '';
        [...nets.children].forEach(c => c.classList.remove('sel'));
        b.classList.add('sel'); validate();
        if (n.lock) pass.focus();
      };
      nets.appendChild(b);
    });
  }
  function scan(){
    fetch('/scan').then(r => r.json()).then(render).catch(() => message('Scan failed. Try Rescan.'));
  }
  $('rescan').onclick = () => {
    message('Scanning…');
    fetch('/rescan').catch(() => {});
    setTimeout(scan, 3500);          // the async scan finishes well inside this
  };
  $('hidden').onclick = () => { $('manualWrap').style.display = 'block'; manual.focus(); };
  manual.oninput = () => {
    chosen = manual.value.trim() || null;
    [...nets.children].forEach(c => c.classList && c.classList.remove('sel'));
    validate();
  };
  dev.oninput = validate;
  $('showpw').onclick = function(){
    const show = pass.type === 'password';
    pass.type = show ? 'text' : 'password';
    this.textContent = show ? 'Hide' : 'Show';
  };
  save.onclick = () => {
    if (!chosen) return;
    save.disabled = true; st.className = 'status'; st.textContent = 'Saving…';
    const body = 'ssid=' + encodeURIComponent(chosen) + '&pass=' + encodeURIComponent(pass.value) +
                 '&devpass=' + encodeURIComponent(dev.value);
    fetch('/save', { method: 'POST', headers: { 'Content-Type': 'application/x-www-form-urlencoded', 'X-LD': '1' }, body })
      .then(r => r.json()).then(j => {
        if (j.ok){
          st.className = 'status ok';
          st.textContent = 'Saved. The detector is restarting and joining “' + chosen + '”. ' +
            'If it can’t join, this setup network comes back within a minute so you can try again.';
        } else {
          st.className = 'status err'; st.textContent = 'Could not save: ' + (j.err || 'unknown error');
          save.disabled = false;
        }
      }).catch(() => {
        st.className = 'status ok';
        st.textContent = 'The detector is restarting (the connection dropped, which is expected).';
      });
  };
  pass.addEventListener('keydown', e => { if (e.key === 'Enter' && !save.disabled) save.click(); });
  dev.addEventListener('keydown', e => { if (e.key === 'Enter' && !save.disabled) save.click(); });
  function showInfo(){
    const bn = $('banner');
    if (INFO.hasPass){
      dev.placeholder = 'Leave blank to keep the current one';
      $('devHint').textContent = 'A device password is already set. Type a new one only to replace it (for example, if you forgot it).';
    }
    if (INFO.failed && INFO.ssid){
      bn.className = 'banner err';
      bn.textContent = 'The detector could not join “' + INFO.ssid + '”. Check the network and its password, then connect again.';
    } else if (INFO.forced){
      bn.className = 'banner info';
      bn.textContent = 'Setup mode (reset pressed twice). Change the network or the device password below. ' +
        (INFO.ssid ? 'Left alone, the detector goes back to “' + INFO.ssid + '” in about 5 minutes.' : '');
    }
    validate();
  }
  fetch('/info').then(r => r.json()).then(j => { INFO = j; showInfo(); }).catch(() => {});
  scan();
</script>
</body></html>
)HTML";

// ----------------------------------------------------------------------
//  DASHBOARD  (STA mode)
// ----------------------------------------------------------------------
static const char DASH_PAGE[] PROGMEM = R"HTML(<!DOCTYPE html><html lang="en"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Lightning Detector</title>
<link rel="preconnect" href="https://fonts.googleapis.com">
<link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
<link href="https://fonts.googleapis.com/css2?family=Inter:wght@400;500;600;700&family=JetBrains+Mono:wght@400;500&family=Space+Grotesk:wght@500;600;700&display=swap" rel="stylesheet" media="print" onload="this.media='all'">
<style>
  :root{
    --bg:#0a0d16; --panel:#111726; --panel2:#172034; --line:#222d4a; --line2:#2d3a60;
    --ink:#e9edf7; --dim:#a3aecb; --faint:#6c7899;
    --bolt:#ffd84a; --bolt-dim:#8f7a25; --bolt-soft:rgba(255,216,74,.11);
    --volt:#8f7dff; --volt-soft:rgba(143,125,255,.14);
    --good:#4fd18b; --warn:#ffa13d; --bad:#ff5d62;
    --a0:#ffffff; --a1:#ffe14d; --a2:#ffa63d; --a3:#ff5a4a; --a4:#d4479c; --a5:#7d6cf2;
    --radius:14px;
    --display:'Space Grotesk',system-ui,sans-serif;
    --body:'Inter',system-ui,-apple-system,'Segoe UI',Roboto,sans-serif;
    --mono:'JetBrains Mono',ui-monospace,Consolas,monospace;
  }
  *{box-sizing:border-box}
  html,body{margin:0}
  body{background:var(--bg);color:var(--ink);font-family:var(--body);font-size:14.5px;line-height:1.5;
    -webkit-font-smoothing:antialiased;
    background-image:radial-gradient(1100px 520px at 70% -12%,rgba(143,125,255,.16),transparent 65%),
                     radial-gradient(800px 400px at 0% 0%,rgba(255,216,74,.05),transparent 60%);
    background-attachment:fixed}
  .wrap{max-width:1320px;margin:0 auto;padding:18px 16px 60px}
  button,select,input{font-family:inherit}

  /* ---- header ---- */
  header{display:flex;align-items:center;justify-content:space-between;gap:14px;flex-wrap:wrap;padding:6px 0 18px}
  .brand{display:flex;align-items:center;gap:12px;min-width:0}
  .logo{width:40px;height:40px;flex:none;border-radius:11px;background:var(--bolt-soft);display:grid;place-items:center;
    box-shadow:inset 0 0 0 1px rgba(255,216,74,.22);transition:background .15s,box-shadow .15s}
  .logo.zap{background:var(--bolt);box-shadow:0 0 0 6px rgba(255,216,74,.25),0 0 40px rgba(255,216,74,.6)}
  .logo.zap path{fill:#1d1600}
  h1{font-family:var(--display);font-weight:700;font-size:21px;letter-spacing:-.01em;margin:0;white-space:nowrap}
  .hdr-r{display:flex;align-items:center;gap:10px;flex-wrap:wrap}
  .pill{display:inline-flex;align-items:center;gap:7px;padding:5px 11px;border-radius:999px;font-size:12.5px;
    font-weight:600;background:var(--panel2);border:1px solid var(--line);color:var(--dim);white-space:nowrap}
  .pill::before{content:"";width:7px;height:7px;border-radius:50%;background:var(--faint)}
  .pill.ok::before{background:var(--good);box-shadow:0 0 0 3px rgba(79,209,139,.18)}
  .pill.warn::before{background:var(--warn)} .pill.bad::before{background:var(--bad)}
  .clock{font-family:var(--mono);font-size:13px;color:var(--dim);font-variant-numeric:tabular-nums}

  /* ---- alert banner ---- */
  .alert{display:flex;align-items:center;gap:16px;padding:16px 18px;border-radius:var(--radius);
    border:1px solid var(--line);background:var(--panel);margin-bottom:14px;flex-wrap:wrap}
  .alert .ai{width:42px;height:42px;flex:none;border-radius:50%;display:grid;place-items:center;background:var(--panel2)}
  .alert .ai svg{width:20px;height:20px}
  .alert .at{font-family:var(--display);font-weight:700;font-size:19px;letter-spacing:-.01em}
  .alert .as{color:var(--dim);font-size:13.5px}
  .alert .ab{flex:1;min-width:220px}
  .alert .trend{font-size:13px;font-weight:600;padding:6px 12px;border-radius:999px;background:var(--panel2);
    color:var(--dim);white-space:nowrap}
  .alert.clear .ai{background:rgba(79,209,139,.12);color:var(--good)}
  .alert.far{border-color:rgba(143,125,255,.35)} .alert.far .ai{background:var(--volt-soft);color:var(--volt)}
  .alert.near{border-color:rgba(255,161,61,.45);background:linear-gradient(90deg,rgba(255,161,61,.10),var(--panel) 60%)}
  .alert.near .ai{background:rgba(255,161,61,.16);color:var(--warn)}
  .alert.close{border-color:rgba(255,93,98,.55);background:linear-gradient(90deg,rgba(255,93,98,.14),var(--panel) 65%)}
  .alert.close .ai{background:rgba(255,93,98,.2);color:var(--bad)}
  .alert.fault{border-color:rgba(255,93,98,.55)} .alert.fault .ai{background:rgba(255,93,98,.18);color:var(--bad)}
  .alert.busy .ai{background:var(--bolt-soft);color:var(--bolt)}
  .alert .fix{flex-basis:100%;margin:4px 0 0;padding:12px 14px 12px 32px;background:var(--panel2);border-radius:10px;
    color:var(--dim);font-size:13px;display:none}
  .alert.fault .fix{display:block}
  .alert .fix li{margin:2px 0} .alert .fix b{color:var(--ink)}

  /* ---- toolbar ---- */
  .toolbar{display:flex;align-items:center;gap:12px;flex-wrap:wrap;margin:0 0 14px}
  .seg{display:inline-flex;background:var(--panel);border:1px solid var(--line);border-radius:11px;padding:3px;flex-wrap:wrap}
  .seg button{border:0;background:none;color:var(--dim);font-weight:600;font-size:13px;padding:6px 11px;border-radius:8px;cursor:pointer}
  .seg button:hover{color:var(--ink)}
  .seg button.on{background:var(--bolt);color:#1d1600}
  .tlabel{font-size:11.5px;font-weight:700;letter-spacing:.08em;text-transform:uppercase;color:var(--faint)}
  .chk{display:inline-flex;align-items:center;gap:8px;color:var(--dim);font-size:13px;cursor:pointer;user-select:none}
  .chk input{accent-color:var(--bolt);width:15px;height:15px}

  /* ---- tiles ---- */
  .tiles{display:grid;grid-template-columns:repeat(auto-fit,minmax(150px,1fr));gap:12px;margin-bottom:16px}
  .tile{background:var(--panel);border:1px solid var(--line);border-radius:12px;padding:13px 15px;min-width:0}
  .tile .k{font-size:11px;letter-spacing:.07em;text-transform:uppercase;color:var(--faint);font-weight:700}
  .tile .v{font-family:var(--display);font-weight:700;font-size:25px;letter-spacing:-.02em;margin-top:3px;
    font-variant-numeric:tabular-nums;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
  .tile .s{font-size:12.5px;color:var(--dim);white-space:nowrap;overflow:hidden;text-overflow:ellipsis}

  /* ---- panels / layout ---- */
  .main{display:grid;grid-template-columns:minmax(0,1fr);gap:16px}
  @media(min-width:1040px){.main{grid-template-columns:minmax(0,1.65fr) minmax(0,1fr)}}
  .side{display:flex;flex-direction:column;gap:16px;min-width:0}
  .lower{display:grid;grid-template-columns:minmax(0,1fr);gap:16px;margin-top:16px}
  @media(min-width:760px){.lower{grid-template-columns:repeat(auto-fit,minmax(340px,1fr))}}
  .panel{background:var(--panel);border:1px solid var(--line);border-radius:var(--radius);padding:16px;min-width:0}
  .ph{display:flex;align-items:center;justify-content:space-between;gap:10px;flex-wrap:wrap;margin-bottom:12px}
  h2{font-family:var(--display);font-weight:600;font-size:13px;letter-spacing:.1em;text-transform:uppercase;color:var(--bolt);margin:0}
  .hint{font-size:12.5px;color:var(--faint)}
  .muted{color:var(--dim)}

  /* ---- buttons / inputs ---- */
  .btn{font-weight:600;font-size:13px;color:var(--ink);border:1px solid var(--line2);background:var(--panel2);
    padding:8px 13px;border-radius:9px;cursor:pointer;white-space:nowrap;text-decoration:none;display:inline-flex;align-items:center;gap:6px}
  .btn:hover{border-color:var(--bolt-dim);color:var(--bolt);background:var(--bolt-soft)}
  .btn:disabled{opacity:.45;cursor:not-allowed}
  .btn.primary{background:var(--bolt);border-color:var(--bolt);color:#1d1600}
  .btn.primary:hover{background:#ffe27a;color:#1d1600}
  .btn.danger{color:#ff9a9d;border-color:rgba(255,93,98,.35)}
  .btn.danger:hover{background:rgba(255,93,98,.12);border-color:var(--bad);color:#ffb9bb}
  .actions{display:flex;gap:8px;flex-wrap:wrap;align-items:center}
  input[type=text],input[type=password],select{background:var(--panel2);border:1px solid var(--line2);border-radius:9px;color:var(--ink);
    font-size:13.5px;padding:8px 11px;outline:none;min-width:0}
  input[type=text]:focus,input[type=password]:focus,select:focus{border-color:var(--bolt)}

  /* ---- lock + password dialog ---- */
  button.pill{cursor:pointer;font-family:inherit}
  button.pill:hover{border-color:var(--bolt-dim);color:var(--ink)}
  .pill.lock::before{display:none}
  .pill.lock svg{width:12px;height:12px}
  .pill.lock.open{color:var(--good);border-color:rgba(79,209,139,.35)}
  .pill.lock.none{color:var(--warn);border-color:rgba(255,161,61,.4)}
  dialog.dlg{background:var(--panel);color:var(--ink);border:1px solid var(--line2);border-radius:14px;padding:20px;
    width:min(390px,calc(100vw - 32px));box-shadow:0 24px 70px rgba(0,0,0,.6);overflow:visible}
  dialog.dlg::backdrop{background:rgba(5,8,15,.72)}
  .dlg h3{font-family:var(--display);font-weight:700;font-size:18px;margin:0 0 6px}
  .dlg p{margin:0 0 14px;font-size:13.5px;color:var(--dim)}
  .dlg input[type=password]{display:block;width:100%;margin:0 0 10px;font-size:15px;padding:10px 12px}
  .dlg .err{color:#ff9a9d;font-size:13px;min-height:19px;margin:-2px 0 8px}
  .dlg .actions{justify-content:flex-end}
  .saved{font-size:12.5px;color:var(--good);font-weight:600;opacity:0;transition:opacity .3s}
  .saved.show{opacity:1}

  /* ---- map ---- */
  .mapbar{display:flex;gap:8px;flex-wrap:wrap;align-items:center}
  .mapbar input{flex:1;min-width:150px}
  .mapwrap{position:relative;border-radius:11px;overflow:hidden;border:1px solid var(--line);background:#0d1220}
  #map{height:560px;width:100%;background:#0d1220}
  @media(max-width:700px){#map{height:420px}}
  .mapmsg{position:absolute;inset:0;display:none;align-items:center;justify-content:center;padding:24px;
    text-align:center;color:var(--dim);font-size:14px;z-index:500;pointer-events:none}
  .mapmsg.show{display:flex}
  .mapmsg .card{max-width:340px;background:rgba(17,23,38,.92);border:1px solid var(--line2);border-radius:12px;padding:16px 18px;
    pointer-events:auto;box-shadow:0 12px 40px rgba(0,0,0,.45)}
  .mapmsg .card b{color:var(--ink);display:block;font-family:var(--display);font-size:16px;margin-bottom:4px}
  .placebar{position:absolute;left:50%;top:12px;transform:translateX(-50%);z-index:600;display:none;align-items:center;gap:8px;
    background:rgba(17,23,38,.95);border:1px solid var(--bolt-dim);border-radius:11px;padding:8px 10px 8px 14px;
    box-shadow:0 10px 30px rgba(0,0,0,.45);font-size:13px;max-width:calc(100% - 24px);flex-wrap:wrap;justify-content:center}
  .placebar.show{display:flex}
  .placebar .coords{font-family:var(--mono);color:var(--bolt);font-size:12.5px}
  .placing .leaflet-container{cursor:crosshair}
  .legend{display:flex;align-items:center;gap:12px;flex-wrap:wrap;margin-top:10px;font-size:12px;color:var(--dim)}
  .legend .sw{display:inline-flex;align-items:center;gap:5px}
  .legend .sw i{width:16px;height:4px;border-radius:2px;display:inline-block}
  .legend .note{margin-left:auto;color:var(--faint)}

  /* leaflet, re-themed */
  .leaflet-container{font-family:var(--body);background:#0d1220}
  .leaflet-bar a,.leaflet-control-layers{background:var(--panel2)!important;color:var(--ink)!important;border-color:var(--line2)!important}
  .leaflet-bar{border:1px solid var(--line2)!important;box-shadow:none!important}
  .leaflet-control-layers{border:1px solid var(--line2)!important;border-radius:9px!important;box-shadow:none!important}
  .leaflet-control-layers-expanded{padding:8px 12px;color:var(--ink)}
  .leaflet-control-layers-toggle{filter:invert(1) hue-rotate(180deg) brightness(1.4)}
  .leaflet-control-attribution{background:rgba(10,13,22,.75)!important;color:var(--faint)!important}
  .leaflet-control-attribution a{color:var(--dim)!important}
  .leaflet-tooltip.ldtip{background:rgba(17,23,38,.96);border:1px solid var(--line2);color:var(--ink);border-radius:9px;
    box-shadow:0 8px 24px rgba(0,0,0,.45);font-size:12.5px;padding:7px 10px}
  .leaflet-tooltip.ldtip::before{display:none}
  .leaflet-tooltip.reflabel{background:none;border:0;box-shadow:none;color:var(--faint);font:600 11px var(--mono);padding:0}
  .leaflet-tooltip.reflabel::before{display:none}
  .sensor-pin{background:none;border:0}
  .sensor-pin .dot{width:16px;height:16px;border-radius:50%;background:var(--bolt);border:3px solid #1d1600;
    box-shadow:0 0 0 2px var(--bolt);position:relative}
  .sensor-pin .dot::after{content:"";position:absolute;inset:-9px;border-radius:50%;border:2px solid rgba(255,216,74,.55);
    animation:halo 2.4s ease-out infinite}
  .sensor-pin.temp .dot{background:#fff;box-shadow:0 0 0 2px #fff}
  @keyframes halo{0%{transform:scale(.5);opacity:1}100%{transform:scale(1.6);opacity:0}}
  .ring-pulse{animation:ringPulse 1.8s ease-out infinite}
  @keyframes ringPulse{0%{stroke-opacity:.95;stroke-width:2}100%{stroke-opacity:0;stroke-width:16}}
  @media(prefers-reduced-motion:reduce){.ring-pulse,.sensor-pin .dot::after{animation:none}}

  /* ---- chart ---- */
  .chartwrap{position:relative}
  #chart{display:block;width:100%;height:250px}
  .ctip{position:absolute;pointer-events:none;background:rgba(17,23,38,.96);border:1px solid var(--line2);border-radius:9px;
    padding:7px 10px;font-size:12.5px;white-space:nowrap;display:none;z-index:5;box-shadow:0 8px 24px rgba(0,0,0,.45)}
  .ctip b{font-family:var(--display)}

  /* ---- log ---- */
  .logwrap{max-height:340px;overflow:auto;border:1px solid var(--line);border-radius:10px}
  table{width:100%;border-collapse:collapse}
  th{position:sticky;top:0;background:var(--panel2);color:var(--faint);font-weight:700;font-size:11px;letter-spacing:.07em;
    text-transform:uppercase;text-align:left;padding:8px 10px;border-bottom:1px solid var(--line);z-index:1}
  td{padding:7px 10px;border-bottom:1px solid rgba(34,45,74,.7);font-size:13px;white-space:nowrap}
  tr:last-child td{border-bottom:0}
  td.num{font-family:var(--mono);font-size:12.5px;text-align:right;color:var(--dim)}
  th.num{text-align:right}
  .adot{display:inline-block;width:8px;height:8px;border-radius:50%;margin-right:8px;vertical-align:1px}
  .tag{font-size:10.5px;font-weight:700;letter-spacing:.06em;padding:1px 6px;border-radius:5px;background:var(--volt-soft);color:var(--volt);margin-left:6px}
  .empty{padding:22px 12px;text-align:center;color:var(--faint);font-size:13.5px}

  /* ---- settings ---- */
  .ctl{display:grid;grid-template-columns:112px minmax(0,1fr) 40px;align-items:center;gap:10px;margin:11px 0}
  .ctl select{width:100%}
  .ctl > label{font-size:13px;color:var(--dim);font-weight:500}
  .ctl output{font-family:var(--mono);color:var(--bolt);font-size:13px;text-align:right}
  input[type=range]{width:100%;accent-color:var(--bolt)}
  .sub{font-size:12px;color:var(--faint);margin:-6px 0 8px 122px}
  .divider{border-top:1px solid var(--line);margin:14px 0}
  .kv{display:grid;grid-template-columns:auto minmax(0,1fr);gap:6px 14px;font-size:13px;margin:0}
  .kv dt{color:var(--faint)} .kv dd{margin:0;color:var(--ink);font-family:var(--mono);font-size:12.5px;overflow-wrap:anywhere}
  .prog{height:6px;border-radius:3px;background:var(--panel2);overflow:hidden;margin:10px 0 6px;display:none}
  .prog i{display:block;height:100%;width:0;background:var(--bolt);transition:width .4s}
  #tuneChart{display:block;width:100%;height:130px;margin-top:8px}
  .tres{font-size:13px;color:var(--dim);margin-top:6px;min-height:20px}
  .tres b{color:var(--ink)} .tres .okc{color:var(--good)} .tres .badc{color:var(--warn)}

  /* ---- help mode ---- */
  #helpBtn.on{background:var(--bolt);border-color:var(--bolt);color:#1d1600}
  body.help [data-help]{cursor:help;outline:1px dashed var(--bolt-dim);outline-offset:3px;border-radius:4px}
  .tip{position:fixed;z-index:2000;max-width:300px;background:#0d1220;border:1px solid var(--bolt-dim);border-radius:10px;
    padding:10px 12px;font-size:13px;line-height:1.45;pointer-events:none;box-shadow:0 10px 30px rgba(0,0,0,.55);
    opacity:0;transition:opacity .12s}
  .tip.show{opacity:1}
  .tip b{display:block;color:var(--bolt);font-family:var(--display);margin-bottom:2px}
  .foot{margin-top:22px;text-align:center;font-size:12px;color:var(--faint)}
</style></head>
<body>
<div class="wrap">
  <header>
    <div class="brand">
      <div class="logo" id="logo"><svg width="21" height="21" viewBox="0 0 24 24" aria-hidden="true"><path d="M13.5 2 4 13.5h6.5L9 22l10-12.5h-6.6L13.5 2z" fill="#ffd84a"/></svg></div>
      <h1>Lightning Detector</h1>
    </div>
    <div class="hdr-r">
      <span class="pill" id="sensorPill" data-help="Sensor|The AS3935 chip and how it is connected. Green means it answered and its oscillators calibrated.">Sensor &hellip;</span>
      <span class="pill" id="linkPill">Connecting&hellip;</span>
      <button class="pill lock" id="lockPill" type="button" data-help="Device password|Viewing is open to anyone on your network; changing settings, tuning, clearing the log and updating the firmware need the device password. Click to unlock this browser, or to lock it again.">Locked</button>
      <span class="clock" id="clock" data-help="Device clock|The detector's clock, shown in your time zone. Strikes are only timestamped once this is set (from the internet, or from this browser).">--:--:--</span>
      <button class="btn" id="helpBtn" type="button" title="Explain the controls">? Help</button>
    </div>
  </header>

  <section class="alert" id="alert">
    <div class="ai" id="alertIcon"></div>
    <div class="ab">
      <div class="at" id="alertTitle">Loading&hellip;</div>
      <div class="as" id="alertSub"></div>
    </div>
    <div class="trend" id="alertTrend" style="display:none" data-help="Storm trend|Fitted from the strike distances over the last 30 minutes. Needs at least 4 strikes spread over 6+ minutes."></div>
    <ul class="fix" id="alertFix"></ul>
  </section>

  <div class="toolbar">
    <span class="tlabel">Show</span>
    <div class="seg" id="winSeg" data-help="Time window|How far back the map rings, chart, tiles and log reach."></div>
    <label class="chk" data-help="Test strikes|Strikes made with the Simulate button. They are labelled TEST in the log; untick to hide them everywhere."><input type="checkbox" id="showTests" checked> Test strikes</label>
  </div>

  <div class="tiles">
    <div class="tile"><div class="k">Strikes</div><div class="v" id="tStrikes">0</div><div class="s" id="tStrikesS">&nbsp;</div></div>
    <div class="tile"><div class="k">Last strike</div><div class="v" id="tLast">&mdash;</div><div class="s" id="tLastS">&nbsp;</div></div>
    <div class="tile" data-help="Closest|The nearest distance reported in the window. 'Overhead' means the storm is right above the sensor."><div class="k">Closest</div><div class="v" id="tClose">&mdash;</div><div class="s" id="tCloseS">&nbsp;</div></div>
    <div class="tile" data-help="Storm front|The chip's own running estimate of the distance to the leading edge of the storm. It can change without a new strike as old strikes age out."><div class="k">Storm front</div><div class="v" id="tStorm">&mdash;</div><div class="s" id="tStormS">sensor estimate</div></div>
    <div class="tile" data-help="Disturbers|Man-made pulses (motors, switches, chargers) the chip rejected as not-lightning. Each one deafens the chip for 1.5 s. 'Near Wi-Fi' counts those within half a second of the detector answering a web request: by chance about 1 in 10 land there while this page is open, so if most do, the detector's own Wi-Fi is the noise source."><div class="k">Disturbers</div><div class="v" id="tDist">0</div><div class="s" id="tDistS">last hour</div></div>
    <div class="tile" data-help="Noise floor|Background radio noise level the chip tolerates (0-7). With auto noise floor on, it rises while noise is too high and falls back after 30 quiet minutes."><div class="k">Noise floor</div><div class="v" id="tNoise">&mdash;</div><div class="s" id="tNoiseS">&nbsp;</div></div>
  </div>

  <div class="main">
    <section class="panel">
      <div class="ph">
        <h2 data-help="Map|Rings are drawn around your sensor at each reported distance. The AS3935 measures distance only, not direction, so a strike lies somewhere on its ring. Thicker rings had more strikes; colour shows how recent the latest one was.">Strike map</h2>
        <div class="mapbar">
          <input type="text" id="search" placeholder="Search an address, or lat, lon" aria-label="Search the map">
          <button class="btn" id="searchBtn" type="button">Go</button>
          <button class="btn" id="placeBtn" type="button" data-help="Place sensor|Click the map (or search an address) to drop the sensor pin, drag it to the exact spot, then Save.">Place sensor</button>
          <button class="btn" id="centerBtn" type="button" title="Zoom to the 40 km detection range">Recenter</button>
        </div>
      </div>
      <div class="mapwrap" id="mapwrap">
        <div id="map"></div>
        <div class="placebar" id="placebar">
          <span>Click the map, then drag the pin</span>
          <span class="coords" id="placeCoords"></span>
          <button class="btn primary" id="placeSave" type="button" disabled>Save location</button>
          <button class="btn" id="placeCancel" type="button">Cancel</button>
        </div>
        <div class="mapmsg" id="mapmsg"><div class="card" id="mapmsgCard"></div></div>
      </div>
      <div class="legend" id="legend"></div>
    </section>

    <div class="side">
      <section class="panel">
        <div class="ph">
          <h2 data-help="Distance over time|Each dot is a strike: height is distance, size is the chip's energy figure, colour is age. A falling trend means the storm is coming closer.">Distance over time</h2>
          <span class="hint" id="chartHint"></span>
        </div>
        <div class="chartwrap" id="chartwrap"><canvas id="chart"></canvas><div class="ctip" id="ctip"></div></div>
      </section>

      <section class="panel">
        <div class="ph">
          <h2>Strike log</h2>
          <div class="actions">
            <button class="btn" id="csvBtn" type="button" data-help="Download CSV|The whole log (not just the window) with your local time, UTC time, distance and energy.">Download CSV</button>
            <button class="btn danger" id="clearBtn" type="button">Clear log</button>
          </div>
        </div>
        <div class="logwrap" id="logwrap">
          <table><thead><tr><th>Time</th><th>Distance</th><th class="num" data-help="Energy|A dimensionless number from the chip. Higher usually means a stronger or closer strike, but it has no physical unit.">Energy</th></tr></thead>
          <tbody id="logBody"></tbody></table>
        </div>
        <div class="actions" style="margin-top:10px"><span class="hint" id="logInfo"></span><button class="btn" id="moreBtn" type="button" style="display:none;margin-left:auto">Show more</button></div>
      </section>
    </div>
  </div>

  <div class="lower">
    <section class="panel">
      <div class="ph"><h2>Sensor settings</h2><span class="saved" id="sSaved">Saved &#10003;</span></div>
      <div class="ctl" data-help="Placement|Sets the receiver gain. Indoor is far more sensitive; choose Outdoor only if the sensor really is outside, or it will be deaf.">
        <label for="sOut">Placement</label>
        <select id="sOut"><option value="0">Indoor</option><option value="1">Outdoor</option></select><span></span>
      </div>
      <div class="ctl" data-help="Noise floor|How much continuous background noise is tolerated before the chip reports 'noise too high' (and stops detecting). Lower = more sensitive.">
        <label for="sNf">Noise floor</label><input type="range" id="sNf" min="0" max="7" step="1"><output id="sNfV">2</output>
      </div>
      <div class="sub" id="sNfSub"></div>
      <div class="ctl" data-help="Auto noise floor|Raises the noise floor one step each time noise is too high, then lowers it again after 30 quiet minutes, never below the value above.">
        <label for="sAuto">Auto noise</label>
        <label class="chk"><input type="checkbox" id="sAuto"> Raise while noisy</label><span></span>
      </div>
      <div class="ctl" data-help="Watchdog threshold|How strong a signal must be to be analysed at all. Higher rejects more disturbers but misses weak, distant strikes.">
        <label for="sWd">Watchdog</label><input type="range" id="sWd" min="0" max="10" step="1"><output id="sWdV">2</output>
      </div>
      <div class="ctl" data-help="Spike rejection|How strictly the signal shape must look like lightning. Higher rejects more disturbers but lowers detection efficiency.">
        <label for="sSr">Spike rejection</label><input type="range" id="sSr" min="0" max="11" step="1"><output id="sSrV">2</output>
      </div>
      <div class="ctl" data-help="Minimum strikes|The chip stays quiet until this many strikes have happened within 15 minutes, then reports normally. Cuts false alarms; 1 reports everything.">
        <label for="sMin">Min. strikes</label>
        <select id="sMin"><option value="1">1</option><option value="5">5</option><option value="9">9</option><option value="16">16</option></select><span></span>
      </div>
      <div class="ctl" data-help="Mask disturbers|Stops the chip reporting disturbers at all. The Disturbers tile then stays at 0.">
        <label for="sMask">Disturbers</label>
        <label class="chk"><input type="checkbox" id="sMask"> Mask (don't report)</label><span></span>
      </div>
      <div class="actions" style="margin-top:14px">
        <button class="btn primary" id="sApply" type="button">Apply &amp; save</button>
        <button class="btn" id="sDefaults" type="button">Defaults</button>
      </div>
    </section>

    <section class="panel">
      <div class="ph"><h2 data-help="Antenna tuning|The antenna must resonate at 500 kHz (within 3.5%) for good detection and distance estimates. Auto-tune measures all 16 capacitor values and keeps the best.">Antenna</h2><span class="hint" id="calibNote"></span></div>
      <dl class="kv">
        <dt>Tuning capacitor</dt><dd id="capNow">&mdash;</dd>
        <dt>Measured</dt><dd id="capHz">not measured since boot</dd>
      </dl>
      <div class="prog" id="tuneProg"><i></i></div>
      <canvas id="tuneChart"></canvas>
      <div class="tres" id="tuneRes"></div>
      <div class="actions" style="margin-top:10px">
        <button class="btn primary" id="tuneBtn" type="button">Auto-tune (9 s)</button>
        <select id="capSel" aria-label="Tuning capacitor"></select>
        <button class="btn" id="capSet" type="button">Set</button>
      </div>
      <div class="divider"></div>
      <div class="actions">
        <button class="btn" id="simBtn" type="button" data-help="Simulate strike|Adds a test strike at a random distance so you can check the map and log without a storm. Test strikes are labelled and can be hidden or cleared.">Simulate strike</button>
        <a class="btn" href="/api/regs" target="_blank" data-help="Registers|Raw AS3935 register values, for troubleshooting.">Registers</a>
        <span class="hint" id="simNote"></span>
      </div>
    </section>

    <section class="panel">
      <div class="ph"><h2>Device</h2></div>
      <dl class="kv">
        <dt>Firmware</dt><dd id="dFw">&mdash;</dd>
        <dt>Access</dt><dd id="dAuth">&mdash;</dd>
        <dt>Address</dt><dd id="dIp">&mdash;</dd>
        <dt>Wi-Fi signal</dt><dd id="dRssi">&mdash;</dd>
        <dt>Transmit</dt><dd id="dTx">&mdash;</dd>
        <dt>Clock</dt><dd id="dTime">&mdash;</dd>
        <dt>Uptime</dt><dd id="dUp">&mdash;</dd>
        <dt>Log</dt><dd id="dLog">&mdash;</dd>
        <dt>Free memory</dt><dd id="dHeap">&mdash;</dd>
      </dl>
      <div class="divider"></div>
      <div class="ctl" data-help="Wi-Fi power|Every transmission is a burst of supply current right next to the sensor. Auto trims power by however much your signal is stronger than needed and goes back to full power if the link ever drops.">
        <label for="wPow">Wi-Fi power</label>
        <select id="wPow">
          <option value="-1">Auto (recommended)</option>
          <option value="20">20.5 dBm (full)</option>
          <option value="17">17 dBm</option>
          <option value="14">14 dBm</option>
          <option value="11">11 dBm</option>
          <option value="8">8 dBm</option>
        </select><span></span>
      </div>
      <div class="ctl" data-help="Radio sleep|Power saving switches the radio off and on around ten times a second, and each switch is a current step the sensor can hear. Always awake draws a steady ~50 mA more, which a USB supply won't notice.">
        <label for="wSleep">Radio</label>
        <select id="wSleep"><option value="0">Always awake (quieter)</option><option value="1">Power saving</option></select><span></span>
      </div>
      <div class="actions">
        <button class="btn primary" id="wApply" type="button">Apply</button>
        <span class="saved" id="wSaved">Saved &#10003;</span>
      </div>
      <div class="divider"></div>
      <div class="actions">
        <button class="btn" id="fwBtn" type="button" data-help="Firmware update|Install a compiled .bin (Arduino IDE: Sketch > Export Compiled Binary). Needs the device password. The detector restarts and this page reloads by itself.">Update firmware</button>
        <input type="file" id="fwFile" accept=".bin,application/octet-stream" hidden>
        <button class="btn" id="pwBtn" type="button" data-help="Device password|Change it here (you need the current one). Every other browser is signed out. Forgot it? Press the detector's reset button twice within 5 seconds and set a new one on the setup page.">Change password</button>
        <button class="btn danger" id="wifiReset" type="button" data-help="Reset Wi-Fi|Forgets the Wi-Fi network and restarts into setup mode. The device password is kept.">Reset Wi-Fi</button>
      </div>
      <div class="prog" id="fwProg"><i></i></div>
      <div class="tres" id="fwNote"></div>
    </section>
  </div>

  <dialog class="dlg" id="pwDlg">
    <form id="pwForm" method="dialog" autocomplete="on">
      <h3 id="pwTitle">Unlock</h3>
      <p id="pwText"></p>
      <input type="text" id="pwUser" autocomplete="username" value="lightning" hidden>
      <input type="password" id="pwCur" placeholder="Device password" autocomplete="current-password">
      <input type="password" id="pwNew" placeholder="New password (8+ characters)" autocomplete="new-password">
      <input type="password" id="pwNew2" placeholder="Repeat the new password" autocomplete="new-password">
      <div class="err" id="pwErr" role="alert"></div>
      <div class="actions">
        <button class="btn" id="pwCancel" type="button">Cancel</button>
        <button class="btn primary" id="pwOk" type="submit">Unlock</button>
      </div>
    </form>
  </dialog>

  <div class="foot">A hobby sensor, not a safety device. If you hear thunder, you are close enough to be struck.</div>
</div>
<div class="tip" id="tip"></div>

<script>
'use strict';
// ===================================================================
//  Helpers
// ===================================================================
const $ = id => document.getElementById(id);
const sleep = ms => new Promise(r => setTimeout(r, ms));
const nowS = () => Date.now() / 1000;
const lsGet = k => { try { return localStorage.getItem(k); } catch (e) { return null; } };
const lsSet = (k, v) => { try { localStorage.setItem(k, v); } catch (e) {} };

// Sign-in token from /api/login, kept in this browser. X-LD marks requests
// as coming from this page: other sites cannot add it without the browser
// asking the detector first, which it never allows.
let TOKEN = lsGet('ld.token') || '';
function setToken(t) {
  TOKEN = t || '';
  try { if (TOKEN) localStorage.setItem('ld.token', TOKEN); else localStorage.removeItem('ld.token'); } catch (e) {}
  if (S) S.auth = TOKEN ? 1 : 0;
  renderLock();
}
function hdrs(extra) {
  const h = Object.assign({ 'X-LD': '1' }, extra || {});
  if (TOKEN) h['X-Auth'] = TOKEN;
  return h;
}
async function api(p, o) {
  o = o || {};
  const r = await fetch(p, Object.assign({ cache: 'no-store' }, o, { headers: hdrs(o.headers) }));
  if (!r.ok) {
    let j = null;
    try { j = await r.json(); } catch (e) {}
    const err = new Error((j && j.err) || 'HTTP ' + r.status);
    err.status = r.status;
    throw err;
  }
  return r.json();
}
const form = data => ({ method: 'POST', headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
  body: new URLSearchParams(data || {}).toString() });
// A change: if the detector says it is locked (or has no password yet),
// ask for the password once and try again.
async function post(p, data) {
  try { return await api(p, form(data)); }
  catch (e) {
    if (e.status !== 401 && e.status !== 428) throw e;
    if (e.status === 401) setToken('');
    if (!await pwDialog(e.status === 428 ? 'create' : 'unlock')) { const c = new Error('cancelled'); c.cancelled = true; throw c; }
    return api(p, form(data));
  }
}
function oops(what, e) { if (!e || !e.cancelled) alert(what + ': ' + (e ? e.message : 'failed')); }
const css = n => getComputedStyle(document.documentElement).getPropertyValue(n).trim();

const OVERHEAD = 1, OUT = 63;
const fmtDist = d => d === OVERHEAD ? 'Overhead' : d === OUT ? '> 40 km' : d + ' km';
const ringKm = d => d === OVERHEAD ? 2.5 : d;       // overhead: a small filled disc
const isTest = s => (s.f & 1) === 1;

function ago(sec) {
  sec = Math.max(0, Math.round(sec));
  if (sec < 60) return sec + ' s ago';
  if (sec < 3600) return Math.round(sec / 60) + ' min ago';
  if (sec < 86400) { const h = Math.floor(sec / 3600), m = Math.round((sec % 3600) / 60);
    return h + ' h' + (m ? ' ' + m + ' min' : '') + ' ago'; }
  const d = Math.floor(sec / 86400); return d + ' day' + (d > 1 ? 's' : '') + ' ago';
}
function dur(sec) {
  sec = Math.round(sec);
  const d = Math.floor(sec / 86400), h = Math.floor(sec % 86400 / 3600), m = Math.floor(sec % 3600 / 60);
  return d ? d + 'd ' + h + 'h' : h ? h + 'h ' + m + 'm' : m + 'm ' + (sec % 60) + 's';
}
const fmtTime = t => new Date(t * 1000).toLocaleString([], { month: 'short', day: 'numeric',
  hour: 'numeric', minute: '2-digit', second: '2-digit' });
const fmtClock = t => new Date(t * 1000).toLocaleTimeString([], { hour: 'numeric', minute: '2-digit', second: '2-digit' });
const fmtNum = n => n.toLocaleString();

// Age buckets (minutes) -> colour, shared by map, chart, log and legend.
const AGES = [[5, '--a0', '< 5 min'], [15, '--a1', '5-15 min'], [30, '--a2', '15-30 min'],
              [60, '--a3', '30-60 min'], [180, '--a4', '1-3 h'], [Infinity, '--a5', 'older']];
const ageColor = sec => { const m = sec / 60; for (const a of AGES) if (m < a[0]) return css(a[1]); return css('--a5'); };

const WINDOWS = [[3600, '1 h'], [10800, '3 h'], [43200, '12 h'], [86400, '24 h'],
                 [604800, '7 d'], [2592000, '30 d'], [0, 'All']];
const winName = w => (WINDOWS.find(x => x[0] === w) || [0, 'All'])[1];

// ===================================================================
//  State
// ===================================================================
let S = null;                 // last /api/state
let CFG = null;               // /api/config
let strikes = [];             // {t, d, e, f}, oldest first
let seen = new Set();
let seq = null, gen = null;
let winSec = +(lsGet('ld.win') || 86400);
if (!WINDOWS.some(w => w[0] === winSec)) winSec = 86400;
let showTests = lsGet('ld.tests') !== '0';
let dirty = true, lastRender = 0, logLimit = 200;
let stateAt = 0;              // browser time of the last /api/state

function addRows(rows) {
  let n = 0;
  for (const r of rows || []) {
    const k = r.join(',');
    if (seen.has(k)) continue;
    seen.add(k);
    strikes.push({ t: r[0], d: r[1], e: r[2], f: r[3] });
    n++;
  }
  if (n) { strikes.sort((a, b) => a.t - b.t); dirty = true; }
  return n;
}
const visibleAll = () => showTests ? strikes : strikes.filter(s => !isTest(s));
function inWindow() {
  const all = visibleAll();
  if (!winSec) return all;
  const t0 = nowS() - winSec;
  return all.filter(s => s.t >= t0);
}

async function loadLog() {
  // State first: anything committed after this seq is fetched by the next
  // poll, anything before it is in the file. Overlaps are de-duplicated.
  const st = await api('/api/state');
  const rows = await api('/api/strikes');
  strikes = []; seen = new Set();
  addRows(rows);
  seq = st.seq; gen = st.gen; S = st; stateAt = nowS();
  dirty = true;
}

// ===================================================================
//  Polling
// ===================================================================
// Every poll makes the detector transmit a reply right next to the sensor,
// so poll gently: 5 s is plenty for lightning, and a hidden tab backs off.
const POLL_MS = 5000, POLL_HIDDEN_MS = 30000;
let lastTimePush = 0, tunePolling = false, pollTimer = null, polling = false;
function schedulePoll(ms) { clearTimeout(pollTimer); pollTimer = setTimeout(poll, ms); }
document.addEventListener('visibilitychange', () => { if (!document.hidden) schedulePoll(300); });

async function poll() {
  if (polling) return;
  polling = true;
  try {
    const st = await api('/api/state' + (seq !== null ? '?since=' + seq : ''));
    if (gen !== null && (st.gen !== gen || st.reload)) {
      await loadLog();
    } else {
      const fresh = (st.new || []).filter(r => !seen.has(r.join(',')));
      addRows(st.new);
      seq = st.seq; S = st; stateAt = nowS();
      if (fresh.length) zap();
    }
    setLink(true);
    if (!S.timeValid && Date.now() - lastTimePush > 60000) {
      lastTimePush = Date.now();
      post('/api/time', { t: Math.floor(nowS()) }).catch(() => {});
    }
    if (S.tuning && !tunePolling) pollTune();
  } catch (e) {
    setLink(false);
  }
  polling = false;
  renderStatus();
  if (dirty || Date.now() - lastRender > 30000) renderData();
  schedulePoll(document.hidden ? POLL_HIDDEN_MS : POLL_MS);
}

function setLink(ok) {
  const p = $('linkPill');
  p.className = 'pill ' + (ok ? 'ok' : 'bad');
  p.textContent = ok ? 'Live' : 'Offline';
}

function zap() {
  const l = $('logo');
  l.classList.add('zap');
  setTimeout(() => l.classList.remove('zap'), 700);
}

// ===================================================================
//  Status (every poll)
// ===================================================================
const ICONS = {
  bolt: '<svg viewBox="0 0 24 24"><path d="M13.5 2 4 13.5h6.5L9 22l10-12.5h-6.6L13.5 2z" fill="currentColor"/></svg>',
  ok: '<svg viewBox="0 0 24 24"><path d="M9.5 16.2 5.3 12l-1.4 1.4 5.6 5.6L20.1 8.4 18.7 7z" fill="currentColor"/></svg>',
  warn: '<svg viewBox="0 0 24 24"><path d="M12 2 1 21h22L12 2zm1 15h-2v2h2v-2zm0-7h-2v5h2v-5z" fill="currentColor"/></svg>',
  tune: '<svg viewBox="0 0 24 24"><path d="M3 12h3l3-8 4 16 3-8h5" stroke="currentColor" stroke-width="2.2" fill="none" stroke-linecap="round" stroke-linejoin="round"/></svg>'
};

function renderStatus() {
  if (!S) return;
  const sp = $('sensorPill');
  if (!S.sensor) { sp.className = 'pill bad'; sp.textContent = 'Sensor not found'; }
  else if (S.tuning) { sp.className = 'pill warn'; sp.textContent = 'Tuning antenna'; }
  else if (!S.calib) { sp.className = 'pill warn'; sp.textContent = 'AS3935 · ' + S.bus + ' · not calibrated'; }
  else { sp.className = 'pill ok'; sp.textContent = 'AS3935 · ' + S.bus; }

  const devNow = S.time + (nowS() - stateAt);
  $('clock').textContent = S.timeValid ? fmtClock(devNow) : 'clock not set';

  // tiles fed by the device
  $('tDist').textContent = S.sensor ? fmtNum(S.disturbersHr) : '—';
  $('tDistS').textContent = 'last hour · ' + fmtNum(S.distTxHr || 0) + ' near Wi-Fi';
  $('tNoise').textContent = S.sensor ? S.nf + ' / 7' : '—';
  const noisyNow = S.noiseAge >= 0 && S.noiseAge < 10;
  $('tNoiseS').textContent = noisyNow ? 'noise too high now'
    : (S.autoNoise ? 'auto, min ' + S.nfMin : 'manual') + (S.noiseHr ? ' · noisy ' + S.noiseHr + ' s/h' : '');
  $('tNoiseS').style.color = noisyNow ? css('--warn') : '';
  if (S.stormAge >= 0 && S.stormAge < 1800 && S.storm) {
    $('tStorm').textContent = fmtDist(S.storm);
    $('tStormS').textContent = 'sensor estimate · ' + ago(S.stormAge);
  } else { $('tStorm').textContent = '—'; $('tStormS').textContent = 'sensor estimate'; }

  // device panel
  $('dFw').textContent = 'v' + S.fw;
  $('dRssi').textContent = S.rssi + ' dBm (' + (S.rssi >= -60 ? 'strong' : S.rssi >= -72 ? 'fair' : 'weak') + ')';
  $('dTx').textContent = S.txDbm.toFixed(1) + ' dBm · ' +
    (S.txLocked ? 'full power: the link dropped at a lower setting' : S.wifiPower < 0 ? 'auto' : 'fixed') +
    ' · ' + (S.wifiSleep ? 'power saving' : 'always awake');
  $('dTime').textContent = S.timeValid ? new Date(devNow * 1000).toLocaleString() : 'not set yet';
  $('dUp').textContent = dur(S.up);
  $('dHeap').textContent = fmtNum(S.heap) + ' bytes';
  $('dLog').textContent = fmtNum(strikes.length) + ' strikes' + (S.fs ? '' : ' · FILESYSTEM NOT MOUNTED') +
    (S.pending ? ' · ' + S.pending + ' waiting for the clock' : '');

  $('capNow').textContent = S.cap >= 0 ? S.cap + ' (' + S.cap * 8 + ' pF)' : 'not tuned yet';
  $('calibNote').textContent = !S.sensor ? '' : S.calib ? 'oscillators calibrated ✓' : 'oscillator calibration failed';
  $('calibNote').style.color = S.sensor && !S.calib ? css('--warn') : '';
  $('tuneBtn').disabled = !S.sensor || S.tuning;
  renderLock();
}

// ===================================================================
//  Data views (on change, and every 30 s so ages and colours move)
// ===================================================================
function renderData() {
  lastRender = Date.now();
  dirty = false;
  const list = inWindow();
  renderTiles(list);
  renderAlert();
  drawChart(list);
  renderRings(list);
  renderLegend(list);
  renderLog(list);
}

function renderTiles(list) {
  const now = nowS();
  $('tStrikes').textContent = fmtNum(list.length);
  $('tStrikesS').textContent = winSec ? 'in the last ' + winName(winSec) : 'all time';
  const last = list.length ? list[list.length - 1] : null;
  $('tLast').textContent = last ? ago(now - last.t).replace(' ago', '') : '—';
  $('tLastS').textContent = last ? fmtDist(last.d) + (isTest(last) ? ' · test' : '') + ' · ' + fmtClock(last.t) : 'none in window';
  const inRange = list.filter(s => s.d !== OUT);
  if (inRange.length) {
    let c = inRange[0];
    for (const s of inRange) if (s.d < c.d || (s.d === c.d && s.t > c.t)) c = s;
    $('tClose').textContent = fmtDist(c.d);
    $('tCloseS').textContent = ago(now - c.t);
  } else {
    $('tClose').textContent = list.length ? '> 40 km' : '—';
    $('tCloseS').textContent = list.length ? 'all out of range' : ' ';
  }
}

// Linear fit of distance against time over the last 30 minutes.
function trend() {
  const t0 = nowS() - 1800;
  const pts = visibleAll().filter(s => s.t >= t0 && s.d !== OUT);
  if (pts.length < 4 || pts[pts.length - 1].t - pts[0].t < 360) return null;
  const n = pts.length, mx = pts.reduce((a, s) => a + s.t, 0) / n, my = pts.reduce((a, s) => a + s.d, 0) / n;
  let sxy = 0, sxx = 0;
  for (const s of pts) { sxy += (s.t - mx) * (s.d - my); sxx += (s.t - mx) * (s.t - mx); }
  if (!sxx) return null;
  const slope = sxy / sxx;             // km per second
  return { kmh: slope * 3600, mx, my, slope, from: pts[0].t };
}

function renderAlert() {
  const a = $('alert'), fix = $('alertFix'), tr = $('alertTrend');
  const set = (cls, icon, title, sub) => {
    a.className = 'alert ' + cls; $('alertIcon').innerHTML = ICONS[icon];
    $('alertTitle').textContent = title; $('alertSub').textContent = sub;
  };
  tr.style.display = 'none';
  if (S && !S.sensor) {
    set('fault', 'warn', 'Sensor not found', 'The AS3935 is not answering. The detector retries every 10 seconds, so you can fix the wiring with it running (power it off first if you can).');
    fix.innerHTML = '<li><b>VCC</b> to 3V3 and <b>GND</b> to G.</li>' +
      '<li>I2C: <b>SCL</b> to D1, <b>MOSI</b> (the I2C data pad, SDA on some boards) to D2, <b>IRQ</b> to D5.</li>' +
      '<li>Tie <b>SI</b>, <b>A0</b> and <b>A1</b> to 3V3; tie <b>CS</b> to GND.</li>' +
      '<li>Some boards swap the MOSI/MISO labels: if it still fails, try the MISO pad on D2.</li>' +
      '<li>SPI boards (SparkFun v2): enable <b>AS3935_USE_SPI</b> in config.h and use the SPI pins listed there.</li>';
    return;
  }
  fix.innerHTML = '';
  if (S && S.tuning) {
    set('busy', 'tune', 'Tuning the antenna…', 'Measuring capacitor ' + (S.tuneStep + 1) + ' of 16. Detection pauses for about 9 seconds.');
    return;
  }
  const now = nowS();
  const all = visibleAll();
  const recent = all.filter(s => s.t >= now - 1800);
  const last = all.length ? all[all.length - 1] : null;
  if (!recent.length) {
    set('clear', 'ok', 'All clear', last
      ? 'No lightning in the last 30 minutes. Last strike: ' + fmtTime(last.t) + ', ' + fmtDist(last.d) + '.'
      : (S && !S.timeValid ? 'Waiting for the clock to be set before logging strikes.' : 'No lightning detected yet.'));
    return;
  }
  const minD = Math.min(...recent.map(s => s.d));
  const tests = recent.every(isTest) ? ' (test strikes)' : '';
  const sub = 'Last strike ' + ago(now - last.t) + ' at ' + fmtDist(last.d) + ' · ' +
    recent.length + ' in the last 30 min' + tests;
  if (minD <= 10) set('close', 'bolt', minD === OVERHEAD ? 'Storm overhead' : 'Lightning within 10 km',
    sub + '. Close enough to strike here: stay indoors until 30 minutes after the last strike.');
  else if (minD <= 20) set('near', 'bolt', 'Lightning ' + minD + ' km away', sub + '.');
  else set('far', 'bolt', minD === OUT ? 'Distant lightning (beyond 40 km)' : 'Distant lightning, ' + minD + ' km', sub + '.');
  const t = trend();
  if (t) {
    tr.style.display = '';
    tr.textContent = t.kmh < -3 ? 'Approaching ~' + Math.round(-t.kmh) + ' km/h'
      : t.kmh > 3 ? 'Moving away ~' + Math.round(t.kmh) + ' km/h' : 'Holding distance';
    tr.style.color = t.kmh < -3 ? css('--warn') : t.kmh > 3 ? css('--good') : '';
  }
}

// ===================================================================
//  Log table + CSV
// ===================================================================
function renderLog(list) {
  const body = $('logBody'), now = nowS();
  const rows = list.slice().reverse();
  if (!rows.length) {
    body.innerHTML = '<tr><td colspan="3" class="empty">' +
      (strikes.length ? 'No strikes in this window.' : 'No strikes logged yet.') + '</td></tr>';
  } else {
    const frag = document.createDocumentFragment();
    rows.slice(0, logLimit).forEach(s => {
      const tr = document.createElement('tr');
      const td1 = document.createElement('td');
      const dot = document.createElement('span'); dot.className = 'adot'; dot.style.background = ageColor(now - s.t);
      td1.appendChild(dot); td1.appendChild(document.createTextNode(fmtTime(s.t)));
      const td2 = document.createElement('td'); td2.textContent = fmtDist(s.d);
      if (isTest(s)) { const g = document.createElement('span'); g.className = 'tag'; g.textContent = 'TEST'; td2.appendChild(g); }
      const td3 = document.createElement('td'); td3.className = 'num'; td3.textContent = fmtNum(s.e);
      tr.appendChild(td1); tr.appendChild(td2); tr.appendChild(td3);
      frag.appendChild(tr);
    });
    body.innerHTML = ''; body.appendChild(frag);
  }
  $('logInfo').textContent = rows.length > logLimit ? 'Showing ' + logLimit + ' of ' + fmtNum(rows.length)
    : fmtNum(rows.length) + ' strike' + (rows.length === 1 ? '' : 's') + (winSec ? ' in the last ' + winName(winSec) : '');
  $('moreBtn').style.display = rows.length > logLimit ? '' : 'none';
}

function downloadCsv() {
  const p = n => String(n).padStart(2, '0');
  const local = t => { const d = new Date(t * 1000);
    return d.getFullYear() + '-' + p(d.getMonth() + 1) + '-' + p(d.getDate()) + ' ' + p(d.getHours()) + ':' + p(d.getMinutes()) + ':' + p(d.getSeconds()); };
  const lines = ['local_time,utc_time,epoch,distance_km,energy,note'];
  for (const s of strikes) {
    const note = [s.d === OVERHEAD ? 'storm overhead' : s.d === OUT ? 'out of range (>40 km)' : '', isTest(s) ? 'test' : '']
      .filter(Boolean).join('; ');
    lines.push([local(s.t), new Date(s.t * 1000).toISOString().replace('.000', ''), s.t,
      s.d === OUT ? '' : s.d, s.e, note].join(','));
  }
  const blob = new Blob([lines.join('\r\n') + '\r\n'], { type: 'text/csv' });
  const a = document.createElement('a');
  const d = new Date();
  a.href = URL.createObjectURL(blob);
  a.download = 'lightning-log-' + d.getFullYear() + p(d.getMonth() + 1) + p(d.getDate()) + '.csv';
  document.body.appendChild(a); a.click(); a.remove();
  setTimeout(() => URL.revokeObjectURL(a.href), 5000);
}

// ===================================================================
//  Distance-over-time chart (canvas)
// ===================================================================
let chartPts = [];
function drawChart(list) {
  const cv = $('chart'), dpr = window.devicePixelRatio || 1;
  const W = cv.clientWidth, H = cv.clientHeight;
  if (!W) return;
  if (cv.width !== Math.round(W * dpr) || cv.height !== Math.round(H * dpr)) {
    cv.width = Math.round(W * dpr); cv.height = Math.round(H * dpr);
  }
  const cx = cv.getContext('2d');
  cx.setTransform(dpr, 0, 0, dpr, 0, 0);
  cx.clearRect(0, 0, W, H);
  const L = 40, R = 10, T = 10, B = 24, pw = W - L - R, ph = H - T - B;
  const now = nowS();
  let t0 = winSec ? now - winSec : (list.length ? list[0].t : now - 3600);
  if (now - t0 < 600) t0 = now - 600;
  const X = t => L + (t - t0) / (now - t0) * pw;
  const Y = d => T + ph - d / 45 * ph;
  const yOf = d => d === OUT ? Y(43.5) : d === OVERHEAD ? Y(1) : Y(d);

  cx.font = '11px ' + css('--mono');
  cx.textBaseline = 'middle';
  // distance grid
  [0, 10, 20, 30, 40].forEach(d => {
    cx.strokeStyle = d === 40 ? 'rgba(143,125,255,.35)' : 'rgba(162,174,203,.10)';
    cx.setLineDash(d === 40 ? [4, 4] : []);
    cx.beginPath(); cx.moveTo(L, Y(d) + .5); cx.lineTo(L + pw, Y(d) + .5); cx.stroke();
    cx.fillStyle = css('--faint'); cx.textAlign = 'right';
    cx.fillText(d + (d ? '' : ' km'), L - 7, Y(d));
  });
  cx.setLineDash([]);
  cx.fillStyle = css('--faint'); cx.textAlign = 'right'; cx.fillText('>40', L - 7, Y(43.5));
  // close-range band
  cx.fillStyle = 'rgba(255,93,98,.06)';
  cx.fillRect(L, Y(10), pw, Y(0) - Y(10));

  // time axis
  const span = now - t0;
  const steps = [300, 600, 900, 1800, 3600, 7200, 10800, 21600, 43200, 86400, 172800, 604800, 1209600];
  const step = steps.find(s => span / s <= 6) || 2592000;
  const tz = new Date().getTimezoneOffset() * 60;   // align ticks to local clock
  cx.textAlign = 'center'; cx.textBaseline = 'top';
  for (let t = Math.ceil((t0 - tz) / step) * step + tz; t <= now; t += step) {
    const x = X(t);
    cx.strokeStyle = 'rgba(162,174,203,.07)';
    cx.beginPath(); cx.moveTo(x + .5, T); cx.lineTo(x + .5, T + ph); cx.stroke();
    const d = new Date(t * 1000);
    const lab = step >= 86400 ? d.toLocaleDateString([], { month: 'short', day: 'numeric' })
      : d.toLocaleTimeString([], { hour: 'numeric', minute: '2-digit' });
    cx.fillStyle = css('--faint');
    cx.fillText(lab, Math.min(Math.max(x, L + 20), L + pw - 20), T + ph + 7);
  }

  // trend line
  const tf = trend();
  if (tf && (!winSec || tf.from >= t0)) {
    const ya = tf.my + tf.slope * (tf.from - tf.mx), yb = tf.my + tf.slope * (now - tf.mx);
    cx.strokeStyle = css('--volt'); cx.lineWidth = 2; cx.setLineDash([6, 5]);
    cx.beginPath(); cx.moveTo(X(tf.from), Y(Math.max(0, Math.min(45, ya)))); cx.lineTo(X(now), Y(Math.max(0, Math.min(45, yb)))); cx.stroke();
    cx.setLineDash([]); cx.lineWidth = 1;
  }

  // strikes (oldest first so the newest sit on top)
  chartPts = [];
  for (const s of list) {
    const x = X(s.t), y = yOf(s.d);
    const r = 2.5 + Math.max(0, Math.min(3.5, (Math.log10(Math.max(1, s.e)) - 3) * 1.1));
    const col = ageColor(now - s.t);
    cx.beginPath(); cx.arc(x, y, r, 0, Math.PI * 2);
    if (isTest(s)) { cx.strokeStyle = col; cx.lineWidth = 1.5; cx.stroke(); cx.lineWidth = 1; }
    else { cx.fillStyle = col; cx.globalAlpha = .9; cx.fill(); cx.globalAlpha = 1; }
    chartPts.push({ x, y, s });
  }
  if (!list.length) {
    cx.fillStyle = css('--faint'); cx.textAlign = 'center'; cx.textBaseline = 'middle';
    cx.font = '13px ' + css('--body');
    cx.fillText('No strikes in this window', L + pw / 2, T + ph / 2);
  }
  $('chartHint').textContent = winSec ? 'last ' + winName(winSec) : 'all time';
}

(function chartHover() {
  const cv = $('chart'), tip = $('ctip');
  cv.addEventListener('mousemove', e => {
    const r = cv.getBoundingClientRect(), mx = e.clientX - r.left, my = e.clientY - r.top;
    let best = null, bd = 144;
    for (const p of chartPts) { const d = (p.x - mx) ** 2 + (p.y - my) ** 2; if (d < bd) { bd = d; best = p; } }
    if (!best) { tip.style.display = 'none'; return; }
    tip.innerHTML = '';
    const b = document.createElement('b'); b.textContent = fmtDist(best.s.d) + (isTest(best.s) ? ' (test)' : '');
    tip.appendChild(b);
    tip.appendChild(document.createElement('br'));
    tip.appendChild(document.createTextNode(fmtTime(best.s.t) + ' · energy ' + fmtNum(best.s.e)));
    tip.style.display = 'block';
    const tx = Math.min(best.x + 12, cv.clientWidth - tip.offsetWidth - 4);
    tip.style.left = Math.max(4, tx) + 'px';
    tip.style.top = Math.max(4, best.y - tip.offsetHeight - 10) + 'px';
  });
  cv.addEventListener('mouseleave', () => { tip.style.display = 'none'; });
})();

// ===================================================================
//  Map (Leaflet, loaded after the page so no-internet still works)
// ===================================================================
let map = null, ringLayer = null, refLayer = null, sensorMarker = null, tempMarker = null;
let placing = false, fittedOnce = false;

function mapMessage(html) {
  const m = $('mapmsg');
  if (!html) { m.classList.remove('show'); return; }
  $('mapmsgCard').innerHTML = html;
  m.classList.add('show');
}

function loadLeaflet() {
  const link = document.createElement('link');
  link.rel = 'stylesheet';
  link.href = 'https://unpkg.com/leaflet@1.9.4/dist/leaflet.css';
  link.integrity = 'sha256-p4NxAoJBhIIN+hmNHrzRCf9tD/miZyoHS5obTRR9BMY=';
  link.crossOrigin = '';
  document.head.appendChild(link);
  const s = document.createElement('script');
  s.src = 'https://unpkg.com/leaflet@1.9.4/dist/leaflet.js';
  s.integrity = 'sha256-20nQCchB9co0qIjJZRGuk2/Z9VM+kNiyxNV1lvTlZBo=';
  s.crossOrigin = '';
  s.onload = initMap;
  s.onerror = () => mapMessage('<b>Map unavailable</b>The map needs internet access to load (Leaflet and map tiles). The log, chart and settings still work.');
  document.head.appendChild(s);
}

const sensorIcon = cls => L.divIcon({ className: 'sensor-pin' + (cls ? ' ' + cls : ''), html: '<div class="dot"></div>', iconSize: [22, 22], iconAnchor: [11, 11] });

function initMap() {
  map = L.map('map', { worldCopyJump: true, zoomSnap: 0.5, maxZoom: 19 }).setView([25, -40], 2);
  // All three need no API key. Esri's dark canvas stops at zoom 16; past
  // that its tiles are upscaled.
  const esri = name => 'https://server.arcgisonline.com/ArcGIS/rest/services/' + name + '/MapServer/tile/{z}/{y}/{x}';
  const base = {
    'Dark': L.layerGroup([
      L.tileLayer(esri('Canvas/World_Dark_Gray_Base'), { maxNativeZoom: 16, maxZoom: 19,
        attribution: 'Tiles &copy; Esri &mdash; Esri, HERE, Garmin, &copy; OpenStreetMap contributors' }),
      L.tileLayer(esri('Canvas/World_Dark_Gray_Reference'), { maxNativeZoom: 16, maxZoom: 19 })
    ]),
    'Streets': L.tileLayer('https://tile.openstreetmap.org/{z}/{x}/{y}.png', { maxZoom: 19,
      attribution: '&copy; <a href="https://www.openstreetmap.org/copyright">OpenStreetMap</a> contributors' }),
    'Satellite': L.tileLayer(esri('World_Imagery'), { maxZoom: 19,
      attribution: 'Imagery &copy; Esri, Maxar, Earthstar Geographics' })
  };
  (base[lsGet('ld.layer')] || base.Dark).addTo(map);
  map.on('baselayerchange', e => lsSet('ld.layer', e.name));
  refLayer = L.layerGroup().addTo(map);
  ringLayer = L.layerGroup().addTo(map);
  L.control.layers(base, { 'Range rings': refLayer }, { position: 'topright' }).addTo(map);
  L.control.scale({ imperial: true, metric: true, position: 'bottomleft' }).addTo(map);
  map.on('click', e => { if (placing) setTemp(e.latlng); });
  new ResizeObserver(() => {
    map.invalidateSize();
    if (!fittedOnce && CFG && CFG.hasLoc) recenter();
  }).observe($('map'));
  showSensor();
  dirty = true; renderData();
}

function showSensor() {
  if (!map || !CFG) return;
  refLayer.clearLayers();
  if (sensorMarker) { sensorMarker.remove(); sensorMarker = null; }
  if (!CFG.hasLoc) {
    if (!placing) mapMessage('<b>Where is your sensor?</b>Click <i>Place sensor</i> and then the map, or search your address above. Distance rings are drawn around that point.');
    return;
  }
  mapMessage(null);
  const c = [CFG.lat, CFG.lon];
  sensorMarker = L.marker(c, { icon: sensorIcon(), keyboard: false, title: 'Lightning sensor', zIndexOffset: 1000 }).addTo(map);
  // Reference rings every 10 km; 40 km is the chip's detection limit.
  [10, 20, 30, 40].forEach(km => {
    const ring = L.circle(c, { radius: km * 1000, color: km === 40 ? '#8f7dff' : '#a3aecb', weight: km === 40 ? 1.5 : 1,
      opacity: km === 40 ? .55 : .22, dashArray: km === 40 ? '6 6' : '2 6', fill: false, interactive: false });
    refLayer.addLayer(ring);
    const lab = L.tooltip({ permanent: true, direction: 'center', className: 'reflabel', interactive: false })
      .setLatLng([CFG.lat + km / 111.32, CFG.lon]).setContent(km === 40 ? '40 km limit' : km + ' km');
    refLayer.addLayer(lab);
  });
  if (!fittedOnce) recenter();
}

function recenter() {
  if (!map) return;
  // A map laid out at zero size (hidden tab, collapsed panel) fits to max
  // zoom; wait for the ResizeObserver to report a real size instead.
  const sz = map.getSize();
  if (!sz.x || !sz.y) return;
  if (CFG && CFG.hasLoc) { map.fitBounds(L.latLng(CFG.lat, CFG.lon).toBounds(84000), { padding: [10, 10] }); fittedOnce = true; }
  else map.setView([25, -40], 2);
}

function renderRings(list) {
  if (!map || !ringLayer) return;
  ringLayer.clearLayers();
  if (!CFG || !CFG.hasLoc) return;
  const c = [CFG.lat, CFG.lon], now = nowS();
  const groups = new Map();
  for (const s of list) {
    if (s.d === OUT) continue;
    const g = groups.get(s.d) || { d: s.d, n: 0, last: 0, tests: 0 };
    g.n++; if (s.t > g.last) g.last = s.t; if (isTest(s)) g.tests++;
    groups.set(s.d, g);
  }
  // far rings first so near ones sit on top and stay hoverable
  [...groups.values()].sort((a, b) => b.d - a.d).forEach(g => {
    const age = now - g.last, col = ageColor(age);
    const ring = L.circle(c, {
      radius: ringKm(g.d) * 1000, color: col, weight: Math.min(2.5 + Math.log2(g.n) * 1.3, 9),
      opacity: age < 3600 ? .95 : .6, fill: g.d === OVERHEAD, fillColor: col, fillOpacity: .22,
      interactive: !placing, bubblingMouseEvents: false
    });
    ring.bindTooltip('<b>' + fmtDist(g.d) + '</b><br>' + g.n + ' strike' + (g.n > 1 ? 's' : '') +
      (g.tests ? ' (' + g.tests + ' test)' : '') + '<br>latest ' + ago(age), { sticky: true, className: 'ldtip' });
    ringLayer.addLayer(ring);
  });
  const newest = list.length ? list[list.length - 1] : null;
  if (newest && newest.d !== OUT && now - newest.t < 900) {
    ringLayer.addLayer(L.circle(c, { radius: ringKm(newest.d) * 1000, color: '#ffffff', weight: 2, fill: false,
      interactive: false, className: 'ring-pulse' }));
  }
}

function renderLegend(list) {
  const lg = $('legend');
  lg.innerHTML = '';
  AGES.forEach(a => {
    const s = document.createElement('span'); s.className = 'sw';
    const i = document.createElement('i'); i.style.background = css(a[1]);
    s.appendChild(i); s.appendChild(document.createTextNode(a[2])); lg.appendChild(s);
  });
  const out = list.filter(s => s.d === OUT).length;
  const n = document.createElement('span'); n.className = 'note';
  n.textContent = (out ? out + ' beyond 40 km (not drawn) · ' : '') + 'distance only, no direction';
  lg.appendChild(n);
}

// ---- placing the sensor ----
function setTemp(ll) {
  if (!tempMarker) {
    tempMarker = L.marker(ll, { icon: sensorIcon('temp'), draggable: true, zIndexOffset: 2000 }).addTo(map);
    tempMarker.on('drag', () => showCoords(tempMarker.getLatLng()));
  } else tempMarker.setLatLng(ll);
  showCoords(ll);
  $('placeSave').disabled = false;
}
function showCoords(ll) { $('placeCoords').textContent = ll.lat.toFixed(5) + ', ' + ll.lng.toFixed(5); }
function startPlacing() {
  if (!map) return;
  placing = true;
  $('mapwrap').classList.add('placing');
  $('placebar').classList.add('show');
  $('placeSave').disabled = true;
  $('placeCoords').textContent = '';
  mapMessage(null);
  if (CFG && CFG.hasLoc) setTemp(L.latLng(CFG.lat, CFG.lon));
  dirty = true; renderData();
}
function stopPlacing() {
  placing = false;
  $('mapwrap').classList.remove('placing');
  $('placebar').classList.remove('show');
  if (tempMarker) { tempMarker.remove(); tempMarker = null; }
  showSensor();
  dirty = true; renderData();
}
$('placeBtn').onclick = () => placing ? stopPlacing() : startPlacing();
$('placeCancel').onclick = stopPlacing;
$('placeSave').onclick = async () => {
  if (!tempMarker) return;
  const ll = tempMarker.getLatLng().wrap();
  $('placeSave').disabled = true;
  try {
    const r = await post('/api/config', { lat: ll.lat.toFixed(6), lon: ll.lng.toFixed(6) });
    if (!r.ok) throw new Error(r.err);
    CFG.hasLoc = 1; CFG.lat = +ll.lat.toFixed(6); CFG.lon = +ll.lng.toFixed(6);
    fittedOnce = false;
    stopPlacing();
  } catch (e) {
    oops('Could not save the location', e);
    $('placeSave').disabled = false;
  }
};
$('centerBtn').onclick = recenter;

async function doSearch() {
  const q = $('search').value.trim();
  if (!q || !map) return;
  let ll = null;
  const m = q.match(/^\s*(-?\d+(?:\.\d+)?)\s*[, ]\s*(-?\d+(?:\.\d+)?)\s*$/);
  if (m) ll = L.latLng(+m[1], +m[2]);
  else {
    $('searchBtn').disabled = true;
    try {
      const r = await fetch('https://nominatim.openstreetmap.org/search?format=jsonv2&limit=1&q=' + encodeURIComponent(q)).then(r => r.json());
      if (r && r.length) ll = L.latLng(+r[0].lat, +r[0].lon);
    } catch (e) {}
    $('searchBtn').disabled = false;
  }
  if (!ll) { alert('Nothing found for “' + q + '”.'); return; }
  map.setView(ll, 15);
  if (!placing) startPlacing();
  setTemp(ll);
}
$('searchBtn').onclick = doSearch;
$('search').addEventListener('keydown', e => { if (e.key === 'Enter') doSearch(); });

// ===================================================================
//  Sensor settings
// ===================================================================
const NF_UV = { 0: [28, 45, 62, 78, 95, 112, 130, 146], 1: [390, 630, 860, 1100, 1330, 1570, 1800, 2000] };
function nfSub() {
  const v = +$('sNf').value, out = +$('sOut').value;
  $('sNfV').textContent = v;
  $('sNfSub').textContent = 'tolerates ~' + NF_UV[out][v] + ' µVrms of noise (' + (out ? 'outdoor' : 'indoor') + ' gain)';
}
['sNf', 'sWd', 'sSr'].forEach(id => $(id).addEventListener('input', () => { $(id + 'V').textContent = $(id).value; nfSub(); }));
$('sOut').addEventListener('change', nfSub);

function fillSettings() {
  if (!CFG) return;
  $('sOut').value = CFG.outdoor; $('sNf').value = CFG.noiseFloor; $('sWd').value = CFG.watchdog;
  $('sSr').value = CFG.spikeRej; $('sMin').value = CFG.minStrikes;
  $('sAuto').checked = !!CFG.autoNoise; $('sMask').checked = !!CFG.maskDist;
  ['sWd', 'sSr'].forEach(id => $(id + 'V').textContent = $(id).value);
  nfSub();
  $('dIp').textContent = CFG.ip + ' · ' + CFG.host + '.local';
  $('capSel').value = CFG.tuneCap >= 0 ? CFG.tuneCap : 0;
  $('wPow').value = CFG.wifiPower; $('wSleep').value = CFG.wifiSleep;
}
$('sDefaults').onclick = () => {
  $('sOut').value = 0; $('sNf').value = 2; $('sWd').value = 2; $('sSr').value = 2; $('sMin').value = 1;
  $('sAuto').checked = true; $('sMask').checked = false;
  ['sWd', 'sSr'].forEach(id => $(id + 'V').textContent = $(id).value);
  nfSub();
};
$('sApply').onclick = async () => {
  const body = { outdoor: $('sOut').value, noiseFloor: $('sNf').value, watchdog: $('sWd').value,
    spikeRej: $('sSr').value, minStrikes: $('sMin').value,
    autoNoise: $('sAuto').checked ? 1 : 0, maskDist: $('sMask').checked ? 1 : 0 };
  try {
    const r = await post('/api/config', body);
    if (!r.ok) throw new Error(r.err || 'failed');
    Object.keys(body).forEach(k => CFG[k] = +body[k]);
    const s = $('sSaved'); s.classList.add('show'); setTimeout(() => s.classList.remove('show'), 1800);
  } catch (e) { oops('Could not save', e); }
};

// ===================================================================
//  Antenna tuning
// ===================================================================
for (let c = 0; c < 16; c++) {
  const o = document.createElement('option'); o.value = c; o.textContent = c + ' · ' + c * 8 + ' pF';
  $('capSel').appendChild(o);
}
$('tuneBtn').onclick = async () => {
  try { await post('/api/tune'); pollTune(); }
  catch (e) { oops('Could not start tuning', e); }
};
$('capSet').onclick = async () => {
  const v = $('capSel').value;
  try {
    const r = await post('/api/config', { tuneCap: v });
    if (!r.ok) throw new Error(r.err);
    CFG.tuneCap = +v;
  } catch (e) { oops('Could not set the capacitor', e); }
};

async function pollTune() {
  if (tunePolling) return;
  tunePolling = true;
  try {
    for (;;) {
      const t = await api('/api/tune');
      renderTune(t);
      if (!t.busy) break;
      await sleep(600);
    }
    CFG = await api('/api/config');
    fillSettings();
  } catch (e) {}
  tunePolling = false;
}

function renderTune(t) {
  const prog = $('tuneProg');
  prog.style.display = t.busy ? 'block' : 'none';
  prog.firstElementChild.style.width = (t.busy ? (t.step / 16) * 100 : 100) + '%';
  const res = $('tuneRes');
  if (t.busy) { res.textContent = 'Measuring capacitor ' + (t.step + 1) + ' of 16…'; }
  else if (t.valid) {
    const hz = t.hz[t.best], dev = (hz - t.target) / t.target * 100, ok = Math.abs(dev) <= t.tol;
    res.innerHTML = '';
    const b = document.createElement('b'); b.textContent = 'Best: ' + t.best + ' (' + t.best * 8 + ' pF) → ' + (hz / 1000).toFixed(1) + ' kHz ';
    const v = document.createElement('span'); v.className = ok ? 'okc' : 'badc';
    v.textContent = (dev >= 0 ? '+' : '') + dev.toFixed(2) + '% ' + (ok ? '✓ within ±' + t.tol + '%' :
      '— outside ±' + t.tol + '%. Some CJMCU boards ship with the wrong antenna capacitors (should be 680 pF + 270 pF).');
    res.appendChild(b); res.appendChild(v);
    $('capHz').textContent = (hz / 1000).toFixed(1) + ' kHz (' + (dev >= 0 ? '+' : '') + dev.toFixed(2) + '%)';
  } else if (t.err) { res.textContent = 'Tuning failed: ' + t.err; }
  else res.textContent = '';
  drawTune(t);
}

function drawTune(t) {
  const cv = $('tuneChart'), dpr = window.devicePixelRatio || 1, W = cv.clientWidth, H = cv.clientHeight;
  if (!W) return;
  cv.width = Math.round(W * dpr); cv.height = Math.round(H * dpr);
  const cx = cv.getContext('2d'); cx.setTransform(dpr, 0, 0, dpr, 0, 0); cx.clearRect(0, 0, W, H);
  const pts = t.hz.map((hz, c) => ({ c, hz })).filter(p => p.hz > 0);
  const L = 44, R = 8, T = 8, B = 18, pw = W - L - R, ph = H - T - B;
  const lo = Math.min(t.target * 0.94, ...pts.map(p => p.hz)), hi = Math.max(t.target * 1.06, ...pts.map(p => p.hz));
  const X = c => L + c / 15 * pw, Y = hz => T + ph - (hz - lo) / (hi - lo) * ph;
  cx.fillStyle = 'rgba(79,209,139,.10)';
  const b1 = Y(t.target * (1 + t.tol / 100)), b2 = Y(t.target * (1 - t.tol / 100));
  cx.fillRect(L, b1, pw, b2 - b1);
  cx.strokeStyle = 'rgba(79,209,139,.6)'; cx.setLineDash([4, 4]);
  cx.beginPath(); cx.moveTo(L, Y(t.target)); cx.lineTo(L + pw, Y(t.target)); cx.stroke(); cx.setLineDash([]);
  cx.font = '10.5px ' + css('--mono'); cx.fillStyle = css('--faint'); cx.textAlign = 'right'; cx.textBaseline = 'middle';
  cx.fillText('500k', L - 6, Y(t.target));
  cx.textAlign = 'center'; cx.textBaseline = 'top';
  [0, 5, 10, 15].forEach(c => cx.fillText(c, X(c), T + ph + 4));
  if (!pts.length) {
    cx.textBaseline = 'middle'; cx.fillText(t.busy ? 'measuring…' : 'run Auto-tune to measure', L + pw / 2, T + ph / 2);
    return;
  }
  cx.strokeStyle = css('--volt'); cx.lineWidth = 1.5;
  cx.beginPath(); pts.forEach((p, i) => i ? cx.lineTo(X(p.c), Y(p.hz)) : cx.moveTo(X(p.c), Y(p.hz))); cx.stroke();
  pts.forEach(p => {
    cx.beginPath(); cx.arc(X(p.c), Y(p.hz), p.c === t.best && !t.busy ? 5 : 3, 0, Math.PI * 2);
    cx.fillStyle = p.c === t.best && !t.busy ? css('--bolt') : css('--volt'); cx.fill();
  });
}

// ===================================================================
//  Device actions
// ===================================================================
$('simBtn').onclick = async () => {
  try {
    const r = await post('/api/simulate');
    $('simNote').textContent = 'Test strike at ' + fmtDist(r.d) + (S && !S.timeValid ? ' (waiting for the clock)' : '');
    schedulePoll(300);
    setTimeout(() => $('simNote').textContent = '', 4000);
  } catch (e) { oops('Simulate failed', e); }
};
$('clearBtn').onclick = async () => {
  if (!confirm('Delete every logged strike from the detector? This cannot be undone.')) return;
  try { await post('/api/clearlog'); await loadLog(); dirty = true; renderData(); }
  catch (e) { oops('Could not clear the log', e); }
};
$('wifiReset').onclick = async () => {
  if (!confirm('Forget the Wi-Fi network? The detector restarts into setup mode and raises the "LightningDetector-Setup" network. The device password is kept.')) return;
  try { await post('/api/reset'); }
  catch (e) { if (e.cancelled || e.status) { oops('Could not reset Wi-Fi', e); return; } }   // no status: it already restarted
  alert('Wi-Fi cleared. Join the LightningDetector-Setup network to set it up again.');
};
$('csvBtn').onclick = downloadCsv;
$('wApply').onclick = async () => {
  const body = { wifiPower: $('wPow').value, wifiSleep: $('wSleep').value };
  try {
    const r = await post('/api/config', body);
    if (!r.ok) throw new Error(r.err || 'failed');
    CFG.wifiPower = +body.wifiPower; CFG.wifiSleep = +body.wifiSleep;
    const s = $('wSaved'); s.classList.add('show'); setTimeout(() => s.classList.remove('show'), 1800);
    schedulePoll(1500);
  } catch (e) { oops('Could not save', e); }
};

// ---- device password ----
const LOCK_SVG = {
  shut: '<svg viewBox="0 0 24 24" aria-hidden="true"><path fill="currentColor" d="M6 10V8a6 6 0 1 1 12 0v2h1a1 1 0 0 1 1 1v10a1 1 0 0 1-1 1H5a1 1 0 0 1-1-1V11a1 1 0 0 1 1-1h1zm2 0h8V8a4 4 0 1 0-8 0v2z"/></svg>',
  open: '<svg viewBox="0 0 24 24" aria-hidden="true"><path fill="currentColor" d="M8 10V8a4 4 0 0 1 7.9-.9l1.9-.6A6 6 0 0 0 6 8v2H5a1 1 0 0 0-1 1v10a1 1 0 0 0 1 1h14a1 1 0 0 0 1-1V11a1 1 0 0 0-1-1H8z"/></svg>'
};
function renderLock() {
  const p = $('lockPill');
  if (!S) return;
  const none = !S.hasPass, open = !none && !!S.auth;
  p.className = 'pill lock' + (none ? ' none' : open ? ' open' : '');
  p.innerHTML = (open ? LOCK_SVG.open : LOCK_SVG.shut);
  p.appendChild(document.createTextNode(none ? 'No password' : open ? 'Unlocked' : 'Locked'));
  $('dAuth').textContent = none ? 'no device password set: anyone on your network can change settings'
    : open ? 'unlocked in this browser' : 'locked (viewing only)';
  $('pwBtn').textContent = none ? 'Set password' : 'Change password';
}
async function ensureUnlocked() {
  if (S && S.hasPass && S.auth) return true;
  return pwDialog(S && !S.hasPass ? 'create' : 'unlock');
}
$('lockPill').onclick = async () => {
  if (!S) return;
  if (!S.hasPass) { await pwDialog('create'); return; }
  if (S.auth) { setToken(''); return; }               // lock this browser again
  await pwDialog('unlock');
};
$('pwBtn').onclick = () => pwDialog(S && S.hasPass ? 'change' : 'create');

// One dialog for all three: unlock (current), create (new x2), change (all).
let pwBusy = false;
function pwDialog(mode) {
  if (pwBusy) return Promise.resolve(false);
  pwBusy = true;
  const d = $('pwDlg'), cur = $('pwCur'), n1 = $('pwNew'), n2 = $('pwNew2'), err = $('pwErr'), ok = $('pwOk');
  const M = {
    unlock: ['Unlock this detector', 'Enter the device password to change settings. This browser stays unlocked until you lock it or the password changes.', 'Unlock'],
    create: ['Set a device password', 'Right now anyone on your network can change settings or replace the firmware. Choose a password (8 or more characters) to protect them. Viewing stays open.', 'Set password'],
    change: ['Change the device password', 'Every other browser will need the new password.', 'Change password']
  }[mode];
  $('pwTitle').textContent = M[0]; $('pwText').textContent = M[1]; ok.textContent = M[2];
  cur.style.display = mode === 'create' ? 'none' : 'block';
  cur.placeholder = mode === 'change' ? 'Current password' : 'Device password';
  n1.style.display = n2.style.display = mode === 'unlock' ? 'none' : 'block';
  [cur, n1, n2].forEach(i => i.value = '');
  err.textContent = '';
  return new Promise(resolve => {
    const done = v => { d.close(); pwBusy = false; $('pwForm').onsubmit = null; $('pwCancel').onclick = null; d.oncancel = null; resolve(v); };
    $('pwCancel').onclick = () => done(false);
    d.oncancel = e => { e.preventDefault(); done(false); };
    $('pwForm').onsubmit = async e => {
      e.preventDefault();
      err.textContent = '';
      if (mode !== 'unlock') {
        if (n1.value.length < 8) { err.textContent = 'Use 8 or more characters.'; n1.focus(); return; }
        if (n1.value.length > 64) { err.textContent = 'Use 64 characters or fewer.'; n1.focus(); return; }
        if (n1.value !== n2.value) { err.textContent = 'The two new passwords differ.'; n2.focus(); return; }
      }
      ok.disabled = true;
      try {
        const j = mode === 'unlock' ? await api('/api/login', form({ pass: cur.value }))
                                    : await api('/api/password', form({ old: cur.value, new: n1.value }));
        setToken(j.token);
        if (S) { S.hasPass = 1; S.auth = 1; }
        renderLock();
        ok.disabled = false;
        done(true);
      } catch (x) {
        ok.disabled = false;
        err.textContent = x.message.charAt(0).toUpperCase() + x.message.slice(1) + '.';
        (mode === 'create' ? n1 : cur).focus();
      }
    };
    d.showModal();
    (mode === 'create' ? n1 : cur).focus();
  });
}

// ---- firmware update ----
$('fwBtn').onclick = async () => { if (await ensureUnlocked()) $('fwFile').click(); };
$('fwFile').onchange = () => {
  const f = $('fwFile').files[0];
  $('fwFile').value = '';
  if (!f) return;
  if (!/\.bin$/i.test(f.name) && !confirm('“' + f.name + '” is not a .bin file. Install it anyway?')) return;
  uploadFirmware(f);
};
function uploadFirmware(f) {
  const prog = $('fwProg'), bar = prog.firstElementChild, note = $('fwNote');
  const say = (t, cls) => { note.innerHTML = ''; const s = document.createElement('span'); if (cls) s.className = cls; s.textContent = t; note.appendChild(s); };
  $('fwBtn').disabled = true;
  prog.style.display = 'block'; bar.style.width = '0%';
  say('Uploading ' + f.name + '…');
  const x = new XMLHttpRequest();
  x.open('POST', '/api/update');
  const h = hdrs();
  Object.keys(h).forEach(k => x.setRequestHeader(k, h[k]));
  x.upload.onprogress = e => { if (e.lengthComputable) bar.style.width = (e.loaded / e.total * 100).toFixed(1) + '%'; };
  x.onload = async () => {
    let j = null;
    try { j = JSON.parse(x.responseText); } catch (e) {}
    if (x.status === 200 && j && j.ok) {
      bar.style.width = '100%';
      say('Installed. The detector is restarting…', 'okc');
      clearTimeout(pollTimer);
      await sleep(5000);
      for (let i = 0; i < 45; i++) {
        try { await api('/api/state'); location.reload(); return; } catch (e) {}
        await sleep(2000);
      }
      say('The detector has not come back yet. Reload this page in a minute.', 'badc');
    } else {
      if (x.status === 401) setToken('');
      prog.style.display = 'none';
      say('Update failed: ' + ((j && j.err) || 'HTTP ' + x.status) + '. The detector is still running the old firmware.', 'badc');
      $('fwBtn').disabled = false;
    }
  };
  x.onerror = () => {
    prog.style.display = 'none';
    say('The upload was interrupted. The detector is still running the old firmware.', 'badc');
    $('fwBtn').disabled = false;
  };
  const fd = new FormData();
  fd.append('firmware', f, f.name);
  x.send(fd);
}
$('moreBtn').onclick = () => { logLimit += 300; renderLog(inWindow()); };

// ---- window + test toggle ----
WINDOWS.forEach(([sec, name]) => {
  const b = document.createElement('button'); b.type = 'button'; b.textContent = name;
  if (sec === winSec) b.className = 'on';
  b.onclick = () => {
    winSec = sec; lsSet('ld.win', sec); logLimit = 200;
    [...$('winSeg').children].forEach(c => c.classList.toggle('on', c === b));
    dirty = true; renderData();
  };
  $('winSeg').appendChild(b);
});
$('showTests').checked = showTests;
$('showTests').onchange = e => { showTests = e.target.checked; lsSet('ld.tests', showTests ? '1' : '0'); dirty = true; renderData(); };
window.addEventListener('resize', () => { drawChart(inWindow()); });

// ===================================================================
//  Help mode
// ===================================================================
(function help() {
  const tip = $('tip'), btn = $('helpBtn');
  btn.onclick = () => { document.body.classList.toggle('help'); btn.classList.toggle('on'); tip.classList.remove('show'); };
  function show(el, x, y) {
    const [t, b] = el.getAttribute('data-help').split('|');
    tip.innerHTML = '';
    const h = document.createElement('b'); h.textContent = t; tip.appendChild(h);
    tip.appendChild(document.createTextNode(b || ''));
    tip.classList.add('show');
    const w = tip.offsetWidth, hh = tip.offsetHeight;
    tip.style.left = Math.max(8, Math.min(x + 14, innerWidth - w - 8)) + 'px';
    tip.style.top = (y + hh + 24 > innerHeight ? y - hh - 12 : y + 18) + 'px';
  }
  document.addEventListener('mouseover', e => {
    if (!document.body.classList.contains('help')) return;
    const el = e.target.closest('[data-help]');
    if (el) show(el, e.clientX, e.clientY); else tip.classList.remove('show');
  });
  // In help mode a tap explains instead of acting, so it is safe on phones.
  document.addEventListener('click', e => {
    if (!document.body.classList.contains('help') || e.target.closest('#helpBtn')) return;
    const el = e.target.closest('[data-help]');
    if (!el) return;
    e.preventDefault(); e.stopPropagation();
    show(el, e.clientX, e.clientY);
  }, true);
})();

// ===================================================================
//  Start
// ===================================================================
(async function init() {
  renderTune({ busy: false, valid: false, hz: new Array(16).fill(0), target: 500000, tol: 3.5, best: -1, err: '' });
  loadLeaflet();
  for (;;) {
    try {
      CFG = await api('/api/config');
      fillSettings();
      await loadLog();
      break;
    } catch (e) { setLink(false); await sleep(3000); }
  }
  setLink(true);
  showSensor();
  api('/api/tune').then(t => { if (t.valid || t.busy || t.err) renderTune(t); if (t.busy) pollTune(); }).catch(() => {});
  renderStatus();
  renderData();
  setInterval(() => { if (S) renderStatus(); }, 1000);
  schedulePoll(POLL_MS);
  // A token this browser kept from before may have been replaced (password
  // changed elsewhere): forget it so the lock shows the truth.
  if (TOKEN && S && S.hasPass && !S.auth) setToken('');
  // Upgraded from a version without a password: ask for one right away.
  if (S && !S.hasPass) setTimeout(() => pwDialog('create'), 600);
})();
</script>
</body></html>
)HTML";

#endif // WEBPAGES_H
