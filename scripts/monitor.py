#!/usr/bin/env python3
"""
Booze Barometer serial monitor — browser UI (Tk is unreliable on macOS Aqua).

Usage:
  ./scripts/run-monitor.sh
  ./scripts/run-monitor.sh --port /dev/cu.usbserial-1110
"""

from __future__ import annotations

import argparse
import json
import threading
import time
import webbrowser
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from typing import Any, Optional
from urllib.parse import parse_qs, urlparse

try:
    import serial
    from serial.tools import list_ports
except ImportError:
    raise SystemExit(
        "pyserial is required. Install with:\n"
        "  /usr/bin/python3 -m pip install --user pyserial"
    )


BAUD = 115200
HTTP_PORT = 8765

HTML = r"""<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8"/>
<meta name="viewport" content="width=device-width, initial-scale=1"/>
<title>Booze Barometer Monitor</title>
<style>
  :root { --bg:#111; --card:#1c1c1e; --ink:#f5f5f7; --muted:#a1a1a6; --line:#333; --on:#30d158; --off:#636366; --accent:#0a84ff; --warn:#ffd60a; --bad:#ff453a; }
  * { box-sizing: border-box; }
  body { margin:0; font-family: ui-sans-serif, system-ui, -apple-system, sans-serif; background:var(--bg); color:var(--ink); }
  header { padding:16px 20px; border-bottom:1px solid var(--line); display:flex; flex-wrap:wrap; gap:12px; align-items:center; }
  h1 { font-size:18px; margin:0; font-weight:650; letter-spacing:-0.02em; }
  .row { display:flex; flex-wrap:wrap; gap:8px; align-items:center; }
  select, button { font: inherit; padding:8px 12px; border-radius:8px; border:1px solid var(--line); background:#2c2c2e; color:var(--ink); }
  button { cursor:pointer; background:var(--accent); border-color:transparent; font-weight:600; }
  button.danger { background:#ff453a; }
  button.secondary { background:#2c2c2e; border-color:var(--line); }
  button:disabled { opacity:.45; cursor:not-allowed; }
  #status { color:var(--muted); font-size:13px; }
  #status.stale { color:var(--bad); font-weight:650; }
  #staleBanner {
    display:none; margin:0 20px; margin-top:12px; padding:10px 14px; border-radius:10px;
    background:rgba(255,69,58,.15); border:1px solid rgba(255,69,58,.45); color:#ff8a80; font-size:13px; font-weight:600;
  }
  #staleBanner.show { display:block; }
  main { padding:16px 20px 32px; display:grid; gap:14px; }
  .state { background:var(--card); border:1px solid var(--line); border-radius:14px; padding:18px 20px; }
  .state strong { display:block; font-size:12px; color:var(--muted); text-transform:uppercase; letter-spacing:.06em; margin-bottom:6px; }
  #gameState { font-size:40px; font-weight:750; letter-spacing:-.03em; }
  #nextHint { margin-top:10px; font-size:15px; color:var(--warn); font-weight:600; }
  #nextHint.idle { color:var(--muted); font-weight:500; }
  .controls, .scripts, .dipsim { background:var(--card); border:1px solid var(--line); border-radius:14px; padding:16px 18px; }
  .controls h2, .scripts h2, .dipsim h2, .leds h2, section h2 {
    margin:0 0 6px; font-size:12px; color:var(--muted); text-transform:uppercase; letter-spacing:.06em; font-weight:650;
  }
  .controls p, .scripts p, .dipsim p { margin:0 0 12px; font-size:12px; color:var(--muted); }
  .hit-row, .script-row, .dip-row { display:flex; flex-wrap:wrap; gap:8px; }
  .hit-row button { min-width:96px; font-size:15px; padding:12px 14px; }
  .hit-row button.coin { background:#ff9f0a; color:#1c1200; }
  .hit-row button.start { background:#64d2ff; color:#001825; }
  .hit-row button.fail { background:#ff453a; }
  .hit-row button.fail.holding { outline:2px solid #fff; outline-offset:2px; }
  .hit-row button.win { background:#30d158; color:#032110; }
  .hit-row button.reset { background:#2c2c2e; border:1px solid var(--line); }
  .script-row button { background:#bf5af2; }
  .script-row button.failrun { background:#ff453a; }
  .dip-toggle { min-width:100px; }
  .dip-toggle.forced-on { background:rgba(48,209,88,.22); border-color:var(--on); color:var(--on); }
  .dip-toggle.forced-off { background:rgba(255,69,58,.15); border-color:#ff453a; color:#ff8a80; }
  .flag .raw { display:block; font-size:10px; font-weight:500; color:var(--muted); margin-top:1px; }
  .grid { display:grid; grid-template-columns:repeat(auto-fit,minmax(240px,1fr)); gap:14px; }
  section { background:var(--card); border:1px solid var(--line); border-radius:14px; padding:14px 16px; }
  .pill { display:flex; justify-content:space-between; align-items:center; padding:7px 0; border-bottom:1px solid #2a2a2c; font-family: ui-monospace, Menlo, monospace; font-size:13px; }
  .pill:last-child { border-bottom:0; }
  .flag { min-width:64px; text-align:center; padding:3px 8px; border-radius:999px; font-weight:700; font-size:12px; background:#2c2c2e; color:var(--off); }
  .flag.on { background:rgba(48,209,88,.18); color:var(--on); }
  #meta { white-space:pre-wrap; font-family: ui-monospace, Menlo, monospace; font-size:13px; color:var(--ink); margin:0 0 8px; line-height:1.45; }
  .events-head { display:flex; justify-content:space-between; align-items:center; gap:8px; margin-bottom:8px; }
  .events-head h2 { margin:0; }
  #events { list-style:none; margin:0; padding:0; max-height:240px; overflow:auto; font-family: ui-monospace, Menlo, monospace; font-size:12px; }
  #events li { padding:5px 0; border-bottom:1px solid #2a2a2c; color:#d1d1d6; }
  .leds { background:var(--card); border:1px solid var(--line); border-radius:14px; padding:16px 18px; }
  .board-leds { display:flex; flex-wrap:wrap; gap:18px; margin-bottom:16px; }
  .bulb-wrap { display:flex; flex-direction:column; align-items:center; gap:8px; min-width:88px; }
  .bulb-wrap span { font-size:11px; color:var(--muted); font-family: ui-monospace, Menlo, monospace; }
  .bulb {
    width:44px; height:44px; border-radius:50%;
    background:#2a2a2c; border:2px solid #3a3a3c;
    box-shadow: inset 0 -6px 12px rgba(0,0,0,.45);
    transition: background .05s linear, box-shadow .05s linear, border-color .05s linear;
  }
  .bulb.on-amber { background:#ff8a1a; border-color:#ffb340; box-shadow:0 0 18px #ff8a1a, 0 0 40px rgba(255,138,26,.5), inset 0 -4px 8px rgba(255,255,255,.25); }
  .bulb.on-yellow { background:#ffd60a; border-color:#ffe566; box-shadow:0 0 18px #ffd60a, 0 0 40px rgba(255,214,10,.5), inset 0 -4px 8px rgba(255,255,255,.3); }
  .bulb.on-red { background:#ff2d1a; border-color:#ff6961; box-shadow:0 0 18px #ff2d1a, 0 0 40px rgba(255,45,26,.5), inset 0 -4px 8px rgba(255,255,255,.25); }
  .bulb.on-orange { background:#ff9f0a; border-color:#ffb340; box-shadow:0 0 18px #ff9f0a, 0 0 40px rgba(255,159,10,.45), inset 0 -4px 8px rgba(255,255,255,.25); }
  #gameState.ATTRACT { color:#ff8a1a; }
  #gameState.WAIT_START, #gameState.PLAYING { color:#ffd60a; }
  #gameState.WIN { color:#ffb340; }
  #gameState.FAIL { color:#ff2d1a; }
  .strip-block { margin-top:10px; }
  .strip-label { font-size:11px; color:var(--muted); margin-bottom:6px; font-family: ui-monospace, Menlo, monospace; }
  .strip {
    display:flex; gap:3px; flex-wrap:nowrap; overflow:hidden;
    padding:10px 8px; border-radius:10px; background:#0b0b0c; border:1px solid #222;
  }
  .px {
    width:14px; height:14px; border-radius:3px; flex:0 0 auto;
    background:#1a1a1c; box-shadow: inset 0 0 0 1px #2a2a2c;
  }
  .header-strip { gap:10px; flex-wrap:wrap; overflow:visible; }
  .hdr-sec { display:flex; flex-direction:column; gap:5px; }
  .hdr-sec .px-row { display:flex; gap:3px; }
  .hdr-sec span { font-size:10px; color:var(--muted); font-family: ui-monospace, Menlo, monospace; white-space:nowrap; }
  kbd { font: 11px ui-monospace, Menlo, monospace; padding:1px 5px; border-radius:4px; background:#000; border:1px solid #444; }
</style>
</head>
<body>
<header>
  <h1>Booze Barometer</h1>
  <div class="row">
    <select id="port"></select>
    <button class="secondary" id="refresh">Refresh</button>
    <button id="connect">Connect</button>
    <button class="secondary" id="previewBtn" type="button">Preview (no board)</button>
  </div>
  <div id="status">Loading…</div>
</header>
<div id="staleBanner">Telemetry stale — no status from board (check USB / baud / Connect)</div>
<main>
  <div class="state">
    <strong>Game state</strong>
    <div id="gameState">—</div>
    <div id="attemptsBadge" style="margin-top:8px;font-size:18px;font-weight:700;color:#64d2ff;">Attempts left: —</div>
    <div id="nextHint" class="idle">Connect or use Preview to test</div>
  </div>
  <div class="scripts">
    <h2>Guided playthrough</h2>
    <p>Auto-sends the switch sequence with short delays. Works on the board or in Preview.</p>
    <div class="script-row">
      <button id="runWin" type="button">Full win run</button>
      <button id="runFail" class="failrun" type="button">1 fail → back to Start</button>
      <button id="runThreeFail" class="failrun" type="button">3 fails → game over</button>
      <button id="runStop" class="secondary" type="button">Stop script</button>
    </div>
  </div>
  <div class="controls">
    <h2>Switch controls</h2>
    <p>Keys <kbd>1</kbd>–<kbd>4</kbd>, <kbd>R</kbd> reset. Fail: click = tap, hold mouse = hold fail.</p>
    <div class="hit-row">
      <button class="coin" id="hitCoin" type="button">Coin <kbd>1</kbd></button>
      <button class="start" id="hitStart" type="button">Start <kbd>2</kbd></button>
      <button class="fail" id="hitFail" type="button">Fail <kbd>3</kbd></button>
      <button class="win" id="hitWin" type="button">Win <kbd>4</kbd></button>
      <button class="reset" id="hitReset" type="button">Reset <kbd>R</kbd></button>
    </div>
  </div>
  <div class="dipsim">
    <h2>DIP / header simulation</h2>
    <p>Forces DIP state in firmware (overrides hardware until cleared). Dip1+Dip2 → coins needed.</p>
    <div class="dip-row">
      <button class="secondary dip-toggle" id="forceDip1" type="button" data-dip="1">Force Dip1</button>
      <button class="secondary dip-toggle" id="forceDip2" type="button" data-dip="2">Force Dip2</button>
      <button class="secondary dip-toggle" id="forceDip3" type="button" data-dip="3">Force Dip3</button>
      <button class="secondary dip-toggle" id="forceDip4" type="button" data-dip="4">Force Dip4</button>
      <button class="secondary" id="dipsClear" type="button">Clear DIP forces</button>
      <button class="secondary" id="headerTestToggle" type="button">Header test</button>
    </div>
  </div>
  <div class="leds">
    <h2>LED simulation</h2>
    <div class="board-leds">
      <div class="bulb-wrap"><div class="bulb" id="bulbAttract"></div><span>Attract D4</span></div>
      <div class="bulb-wrap"><div class="bulb" id="bulbGame"></div><span>Game D5</span></div>
      <div class="bulb-wrap"><div class="bulb" id="bulbRelay"></div><span>Relay D6</span></div>
    </div>
    <div class="strip-block">
      <div class="strip-label">Header WS2812 · D7 · 38 px · Insert Token / Proceed / Go! / Left / Attempts / 3 / 2 / 1</div>
      <div class="strip header-strip" id="stripHeader"></div>
    </div>
    <div class="strip-block">
      <div class="strip-label">Front WS2812 · D2 · 30 px</div>
      <div class="strip" id="stripFront"></div>
    </div>
    <div class="strip-block">
      <div class="strip-label">Back WS2812 · D3 · 30 px</div>
      <div class="strip" id="stripBack"></div>
    </div>
  </div>
  <div class="grid">
    <section>
      <h2>Switches (debounced)</h2>
      <div id="sw"></div>
    </section>
    <section>
      <h2>Raw pins</h2>
      <div id="raw"></div>
    </section>
    <section>
      <h2>DIP switches</h2>
      <div id="dip"></div>
    </section>
    <section>
      <h2>Outputs / counters</h2>
      <pre id="meta">coins 0 / need 1
attempts 1
A6=—  A7=—</pre>
      <div id="out"></div>
    </section>
  </div>
  <section>
    <div class="events-head">
      <h2>Events</h2>
      <button class="secondary" id="exportLog" type="button">Download log</button>
    </div>
    <ul id="events"><li>Waiting for serial data…</li></ul>
  </section>
</main>
<script>
const NUM = 30;
const STALE_MS = 500;
const GROUPS = {
  sw: [["coin","Coin  A0"],["start","Start A1"],["fail","Fail  A2"],["win","Win   A3"]],
  raw: [["coin_raw","Coin raw"],["start_raw","Start raw"],["fail_raw","Fail raw"],["win_raw","Win raw"]],
  dip: [["dip1","Dip1 A7"],["dip2","Dip2 A6"],["dip3","Dip3 A5"],["dip4","Dip4 A4"]],
  out: [["attract","Attract LED D4"],["game","Game LED D5"],["relay","Relay D6"],["hdr_test","Header test D7"]],
};
// Header sections in chain order (matches HDR_LEN in firmware)
const HEADER_SECTIONS = [
  ["Insert Token", 11], ["Proceed to Start", 12], ["Go!", 5],
  ["Left", 2], ["Attempts", 5], ["3", 1], ["2", 1], ["1", 1],
];
const HDR = { TOKEN:0, START:1, GO:2, LEFT:3, ATTEMPTS:4, D3:5, D2:6, D1:7 };
const ATTEMPTS_PER_CREDIT = 3;
const NEXT = {
  ATTRACT: "Next: hit Coin → WAIT_START (header: Insert Token blink + Left/Attempts/3)",
  WAIT_START: "Next: clear Fail if held, then hit Start (must return to start each attempt)",
  PLAYING: "Next: Win, or Fail (digit drops 3→2→1; after 3rd fail → game over)",
  WIN: "Waiting — win lightshow, then back to ATTRACT",
  FAIL: "Fail anim — if attempts left → WAIT_START; else ATTRACT",
};
const flags = {};
function buildGroup(id, items) {
  const root = document.getElementById(id);
  root.innerHTML = "";
  for (const [key, title] of items) {
    const row = document.createElement("div");
    row.className = "pill";
    const showRaw = key === "dip1" || key === "dip2";
    row.innerHTML = showRaw
      ? `<span>${title}</span><span class="flag" data-key="${key}">OFF<span class="raw">raw —</span></span>`
      : `<span>${title}</span><span class="flag" data-key="${key}">OFF</span>`;
    root.appendChild(row);
    flags[key] = row.querySelector(".flag");
  }
}
Object.entries(GROUPS).forEach(([id, items]) => buildGroup(id, items));

function buildStrip(el) {
  el.innerHTML = "";
  const dots = [];
  for (let i = 0; i < NUM; i++) {
    const d = document.createElement("div");
    d.className = "px";
    el.appendChild(d);
    dots.push(d);
  }
  return dots;
}
const frontDots = buildStrip(document.getElementById("stripFront"));
const backDots = buildStrip(document.getElementById("stripBack"));

function buildHeader(el) {
  el.innerHTML = "";
  const sections = [];
  HEADER_SECTIONS.forEach(([label, count], idx) => {
    const sec = document.createElement("div");
    sec.className = "hdr-sec";
    const row = document.createElement("div");
    row.className = "px-row";
    const dots = [];
    for (let i = 0; i < count; i++) {
      const d = document.createElement("div");
      d.className = "px";
      row.appendChild(d);
      dots.push(d);
    }
    const name = document.createElement("span");
    name.textContent = `${idx + 1}. ${label}`;
    sec.appendChild(row);
    sec.appendChild(name);
    el.appendChild(sec);
    sections.push(dots);
  });
  return sections;
}
const headerSections = buildHeader(document.getElementById("stripHeader"));
const bulbAttract = document.getElementById("bulbAttract");
const bulbGame = document.getElementById("bulbGame");
const bulbRelay = document.getElementById("bulbRelay");

const portEl = document.getElementById("port");
const statusEl = document.getElementById("status");
const staleBanner = document.getElementById("staleBanner");
const connectBtn = document.getElementById("connect");
const stateEl = document.getElementById("gameState");
const nextHint = document.getElementById("nextHint");
const metaEl = document.getElementById("meta");
const eventsEl = document.getElementById("events");
const hitFailBtn = document.getElementById("hitFail");
const headerTestBtn = document.getElementById("headerTestToggle");
const previewBtn = document.getElementById("previewBtn");
const attemptsBadge = document.getElementById("attemptsBadge");
let lastEventCount = 0;
let latestEvents = [];
let latest = {};
let connected = false;
let previewMode = false;
let statusAt = performance.now();
let boardMsAtSample = 0;
let animState = "—";
let phaseLocalStart = performance.now();
let scriptToken = 0;
let failHolding = false;
let dipForced = { 1: null, 2: null, 3: null, 4: null }; // null=hardware, true/false=forced
let previewFailUntil = 0;
let previewWinUntil = 0;
let previewMs0 = performance.now();

function previewReset() {
  previewMs0 = performance.now();
  phaseLocalStart = performance.now();
  animState = "ATTRACT";
  latest = {
    type: "status", ms: 0, state: "ATTRACT",
    coin:0, start:0, fail:0, win:0,
    coin_raw:0, start_raw:0, fail_raw:0, win_raw:0,
    dip1:0, dip2:0, dip3:0, dip4:0, a6:900, a7:900,
    coins:0, need:1, attempts: ATTEMPTS_PER_CREDIT,
    attract:1, game:0, relay:0, fail_hold:0, hdr_test:0, hdr:0,
  };
  latestEvents = [];
  lastEventCount = -1;
  previewFailUntil = 0;
  previewWinUntil = 0;
  failHolding = false;
}

function digitMask(attempts) {
  if (attempts >= 3) return 1 << HDR.D3;
  if (attempts === 2) return 1 << HDR.D2;
  if (attempts === 1) return 1 << HDR.D1;
  return 0;
}

function statusMask(attempts) {
  return (1 << HDR.LEFT) | (1 << HDR.ATTEMPTS) | digitMask(attempts);
}

/** Same section logic as firmware updateHeader() */
function simHeaderMask(state, attempts, ms, phaseElapsed, hdrTest) {
  if (hdrTest) return 0xFF;
  const status = statusMask(attempts);
  if (state === "ATTRACT") {
    let m = status;
    if (Math.floor(ms / 500) % 2 === 0) m |= 1 << HDR.TOKEN;
    return m;
  }
  if (state === "WAIT_START") {
    let m = status;
    if (Math.floor(ms / 400) % 2 === 0) m |= 1 << HDR.START;
    return m;
  }
  if (state === "PLAYING") return status | (1 << HDR.GO);
  if (state === "WIN") {
    const step = Math.floor(phaseElapsed / 150) % 10;
    if (step < 8) return (1 << (step + 1)) - 1;
    if (step === 8) return 0xFF;
    return 0;
  }
  if (state === "FAIL") {
    const flip = Math.floor(phaseElapsed / 120) % 2 === 0;
    return flip ? status : 0;
  }
  return 0;
}

async function api(path, opts) {
  const r = await fetch(path, opts);
  return r.json();
}

async function refreshPorts() {
  const data = await api("/api/ports");
  portEl.innerHTML = "";
  for (const p of data.ports) {
    const opt = document.createElement("option");
    opt.value = p.device;
    opt.textContent = p.label;
    portEl.appendChild(opt);
  }
  if (data.selected) portEl.value = data.selected;
}

async function toggleConnect() {
  const data = await api("/api/status");
  if (data.connected) {
    scriptToken++;
    await releaseFail();
    await api("/api/disconnect", { method: "POST" });
  } else {
    await api("/api/connect?port=" + encodeURIComponent(portEl.value), { method: "POST" });
  }
}


function setFlag(key, on, rawText) {
  const el = flags[key];
  if (!el) return;
  if (rawText != null) {
    el.innerHTML = `${on ? "ON" : "OFF"}<span class="raw">${rawText}</span>`;
  } else {
    el.textContent = on ? "ON" : "OFF";
  }
  el.classList.toggle("on", !!on);
}

function setBulb(el, on, cls) {
  el.className = "bulb" + (on ? " " + cls : "");
}

function boardMs() {
  if (!connected || latest.ms == null) return performance.now();
  return boardMsAtSample + (performance.now() - statusAt);
}

function rgb(r, g, b) { return `rgb(${r|0},${g|0},${b|0})`; }

function paintStrip(dots, colors) {
  for (let i = 0; i < dots.length; i++) {
    const [r, g, b] = colors[i] || [0, 0, 0];
    const glow = Math.max(r, g, b);
    dots[i].style.background = rgb(r, g, b);
    dots[i].style.boxShadow = glow > 20
      ? `0 0 8px ${rgb(r, g, b)}, inset 0 0 0 1px rgba(255,255,255,.15)`
      : "inset 0 0 0 1px #2a2a2c";
  }
}

function simPixels(state, ms, phaseElapsed) {
  const front = Array.from({ length: NUM }, () => [0, 0, 0]);
  const back = Array.from({ length: NUM }, () => [0, 0, 0]);
  const ww = [255, 140, 60];  // warm white (matches firmware; low G = not green)

  if (state === "WAIT_START" || state === "PLAYING") {
    for (let i = 0; i < NUM; i++) { front[i] = ww; back[i] = ww; }
  } else if (state === "ATTRACT") {
    // Warm white breathe (~10s full cycle)
    const t = (ms % 10000) / 10000;
    const step = t < 0.5 ? t * 2 : (1 - t) * 2;
    const c = ww.map((v) => Math.round(v * step));
    for (let i = 0; i < NUM; i++) { front[i] = c; back[i] = c; }
  } else if (state === "WIN") {
    const off = Math.floor(ms / 60) % 12;
    for (let i = 0; i < NUM; i++) {
      const pos = (i + off) % 12;
      let c;
      if (pos <= 1) c = [255, 160, 60];
      else if (pos <= 3) c = [255, 100, 0];
      else if (pos <= 5) c = [255, 60, 0];
      else if (pos <= 7) c = [255, 0, 0];
      else c = [50, 0, 0];
      front[i] = c; back[i] = c;
    }
  } else if (state === "FAIL") {
    const flip = Math.floor(phaseElapsed / 120) % 2 === 0;
    const a = flip ? [255, 100, 20] : [255, 0, 0];
    const b = flip ? [255, 0, 0] : [255, 100, 20];
    const mid = Math.floor(NUM / 2);
    for (let i = 0; i < NUM; i++) {
      const c = i < mid ? a : b;
      front[i] = c; back[i] = c;
    }
  }
  return { front, back };
}

function tickLeds() {
  if (previewMode) tickPreview();
  const state = latest.state || "—";
  const ms = previewMode ? (performance.now() - previewMs0) : boardMs();
  const phaseElapsed = performance.now() - phaseLocalStart;
  setBulb(bulbAttract, !!latest.attract, "on-amber");
  setBulb(bulbGame, !!latest.game, "on-yellow");
  setBulb(bulbRelay, !!latest.relay, "on-red");
  const { front, back } = simPixels(state, ms, phaseElapsed);
  paintStrip(frontDots, front);
  paintStrip(backDots, back);
  const attempts = latest.attempts ?? ATTEMPTS_PER_CREDIT;
  const hdrMask = (connected && latest.hdr != null)
    ? (latest.hdr | 0)
    : simHeaderMask(state, attempts, ms, phaseElapsed, !!latest.hdr_test);
  if (previewMode) latest.hdr = hdrMask;
  const hdrColor = state === "FAIL" ? [255, 0, 0] : [255, 140, 60];
  headerSections.forEach((dots, s) => {
    const on = (hdrMask >> s) & 1;
    paintStrip(dots, dots.map(() => (on ? hdrColor : [0, 0, 0])));
  });
  requestAnimationFrame(tickLeds);
}

function updateAttemptsBadge() {
  const n = latest.attempts;
  if (n == null) {
    attemptsBadge.textContent = "Attempts left: —";
    return;
  }
  attemptsBadge.textContent = `Attempts left: ${n}  ·  digit ${n >= 3 ? "3" : n === 2 ? "2" : n === 1 ? "1" : "off"}`;
}

function updateNextHint(state) {
  if (!connected && !previewMode) {
    nextHint.textContent = "Connect to the Nano, or click Preview (no board)";
    nextHint.className = "idle";
    return;
  }
  const coins = latest.coins ?? 0;
  const need = latest.need ?? 1;
  const attempts = latest.attempts ?? ATTEMPTS_PER_CREDIT;
  if (state === "ATTRACT" && coins > 0 && coins < need) {
    nextHint.textContent = `Next: hit Coin again (${coins}/${need})`;
    nextHint.className = "";
    return;
  }
  if (state === "PLAYING") {
    nextHint.textContent = `Playing · ${attempts} attempt${attempts === 1 ? "" : "s"} left — Win or Fail`;
    nextHint.className = "";
    return;
  }
  if (state === "WAIT_START" && attempts < ATTEMPTS_PER_CREDIT) {
    nextHint.textContent = `Return to start, then Start (${attempts} attempt${attempts === 1 ? "" : "s"} left)`;
    nextHint.className = "";
    return;
  }
  nextHint.textContent = NEXT[state] || "Waiting for status…";
  nextHint.className = NEXT[state] ? "" : "idle";
}

function tickPreview() {
  const now = performance.now();
  latest.ms = Math.floor(now - previewMs0);
  latest.fail = failHolding ? 1 : 0;
  latest.fail_hold = failHolding ? 1 : 0;
  latest.fail_raw = failHolding ? 1 : 0;
  if (latest.state === "FAIL" && now >= previewFailUntil) {
    if (latest.attempts > 0) {
      setPreviewState("WAIT_START");
      latest.game = 1; latest.attract = 0; latest.relay = 0;
    } else {
      setPreviewState("ATTRACT");
      latest.attempts = ATTEMPTS_PER_CREDIT;
      latest.coins = 0; latest.game = 0; latest.attract = 1; latest.relay = 0;
    }
  }
  if (latest.state === "WIN" && now >= previewWinUntil) {
    setPreviewState("ATTRACT");
    latest.attempts = ATTEMPTS_PER_CREDIT;
    latest.coins = 0; latest.game = 0; latest.attract = 1; latest.relay = 0;
  }
  if (latest.state === "ATTRACT") {
    latest.attract = Math.floor(latest.ms / 500) % 2;
    latest.game = 0;
  }
}

function setPreviewState(st) {
  latest.state = st;
  animState = st;
  phaseLocalStart = performance.now();
  pushPreviewEvent("state", st);
}

function pushPreviewEvent(name, value) {
  const stamp = new Date().toTimeString().slice(0, 8);
  latestEvents.push(`${stamp}  ${name} = ${value}`);
  if (latestEvents.length > 200) latestEvents = latestEvents.slice(-200);
  lastEventCount = -1;
}

function previewHit(sw) {
  if (!previewMode) return;
  if (sw === "coin" && latest.state === "ATTRACT") {
    latest.coins = (latest.coins || 0) + 1;
    pushPreviewEvent("coin", latest.coins);
    latest.relay = 1;
    setTimeout(() => { if (previewMode) latest.relay = 0; }, 36);
    if (latest.coins >= (latest.need || 1)) {
      setPreviewState("WAIT_START");
      latest.game = 1; latest.attract = 0;
    }
  } else if (sw === "start" && latest.state === "WAIT_START" && !failHolding) {
    pushPreviewEvent("start", "1");
    setPreviewState("PLAYING");
    latest.game = 1; latest.attract = 0; latest.relay = 0;
  } else if (sw === "fail" && latest.state === "PLAYING") {
    pushPreviewEvent("fail", "1");
    latest.attempts = Math.max(0, (latest.attempts || 0) - 1);
    pushPreviewEvent("attempts", latest.attempts);
    setPreviewState("FAIL");
    latest.relay = 1;
    previewFailUntil = performance.now() + 1500;
    setTimeout(() => { if (previewMode) latest.relay = 0; }, 300);
  } else if (sw === "win" && latest.state === "PLAYING") {
    pushPreviewEvent("win", "1");
    setPreviewState("WIN");
    latest.relay = 1;
    previewWinUntil = performance.now() + 5000;
  }
}

function previewCmd(line) {
  if (!previewMode) return;
  if (line === "reset") {
    previewReset();
    pushPreviewEvent("sim", "reset");
  } else if (line === "header test on") {
    latest.hdr_test = 1;
    pushPreviewEvent("sim", "header_test_on");
  } else if (line === "header test off") {
    latest.hdr_test = 0;
    pushPreviewEvent("sim", "header_test_off");
  } else if (line === "hold fail") {
    failHolding = true;
    if (latest.state === "PLAYING") previewHit("fail");
  } else if (line === "release fail") {
    failHolding = false;
  } else if (line.startsWith("hit ")) {
    previewHit(line.slice(4).trim());
  }
}

function syncDipButtons() {
  for (let n = 1; n <= 4; n++) {
    const btn = document.getElementById("forceDip" + n);
    const forced = dipForced[n];
    btn.classList.remove("forced-on", "forced-off", "on");
    if (forced === true) {
      btn.classList.add("forced-on");
      btn.textContent = `Force Dip${n} ON*`;
    } else if (forced === false) {
      btn.classList.add("forced-off");
      btn.textContent = `Force Dip${n} OFF*`;
    } else {
      btn.textContent = `Force Dip${n}`;
    }
  }
  headerTestBtn.classList.toggle("on", !!latest.hdr_test);
  headerTestBtn.textContent = latest.hdr_test ? "Header test ON" : "Header test";
}

function setControlsEnabled(on) {
  for (const id of ["hitCoin","hitStart","hitFail","hitWin","hitReset","runWin","runFail","runThreeFail","forceDip1","forceDip2","forceDip3","forceDip4","dipsClear","headerTestToggle"]) {
    document.getElementById(id).disabled = !on;
  }
}

function render(data) {
  connected = !!data.connected;
  if (connected && previewMode) {
    previewMode = false;
    previewBtn.classList.remove("on");
    previewBtn.textContent = "Preview (no board)";
  }
  const age = data.telemetry_age_ms;
  const stale = connected && (age == null || age > STALE_MS);
  statusEl.classList.toggle("stale", stale);
  if (previewMode) {
    statusEl.textContent = "Preview mode · simulating header + 3 attempts (no board)";
    statusEl.classList.remove("stale");
    staleBanner.classList.remove("show");
  } else if (stale) {
    statusEl.textContent = (data.status_text || "Connected") + ` · telemetry stale (${age ?? "—"} ms)`;
    staleBanner.classList.add("show");
  } else {
    statusEl.textContent = data.status_text || "";
    staleBanner.classList.remove("show");
  }
  connectBtn.textContent = connected ? "Disconnect" : "Connect";
  connectBtn.classList.toggle("danger", connected);
  setControlsEnabled(connected || previewMode);

  if (!previewMode) {
    const s = data.status || {};
    if (s.state && s.state !== animState) {
      animState = s.state;
      phaseLocalStart = performance.now();
    }
    if (s.ms != null) {
      boardMsAtSample = s.ms;
      statusAt = performance.now();
    }
    latest = s;
  }
  stateEl.textContent = latest.state || "—";
  stateEl.className = latest.state || "";
  updateAttemptsBadge();
  updateNextHint(latest.state);

  const s = latest;

  // Use debounced board flags for Dip1/2 (raw alone flickers from ADC crosstalk).
  const a6 = s.a6, a7 = s.a7;
  const dipLive = {
    dip1: dipForced[1] == null ? !!s.dip1 : dipForced[1],
    dip2: dipForced[2] == null ? !!s.dip2 : dipForced[2],
    dip3: dipForced[3] == null ? !!s.dip3 : dipForced[3],
    dip4: dipForced[4] == null ? !!s.dip4 : dipForced[4],
  };

  for (const key of Object.keys(flags)) {
    if (key === "dip1") setFlag(key, dipLive.dip1, `A7=${a7 ?? "—"}`);
    else if (key === "dip2") setFlag(key, dipLive.dip2, `A6=${a6 ?? "—"}`);
    else if (key === "dip3" || key === "dip4") setFlag(key, dipLive[key]);
    else setFlag(key, s[key]);
  }

  let dipHint = "";
  if (typeof a6 === "number" && typeof a7 === "number" && a6 < 120 && a7 < 120) {
    dipHint = "\nA6/A7 both low — open dip may ghost; flags are debounced (10k pull-ups = solid fix)";
  }
  metaEl.textContent =
    `coins ${s.coins ?? 0} / need ${s.need ?? 1}\n` +
    `attempts ${s.attempts ?? ATTEMPTS_PER_CREDIT}\n` +
    `hdr mask ${s.hdr ?? "—"}\n` +
    `A6=${a6 ?? "—"}  A7=${a7 ?? "—"}` + dipHint;
  syncDipButtons();
  hitFailBtn.classList.toggle("holding", failHolding || !!s.fail_hold);

  if (!previewMode) latestEvents = data.events || [];
  if (latestEvents.length !== lastEventCount) {
    eventsEl.innerHTML = "";
    for (const line of latestEvents.slice(-200)) {
      const li = document.createElement("li");
      li.textContent = line;
      eventsEl.appendChild(li);
    }
    eventsEl.scrollTop = eventsEl.scrollHeight;
    lastEventCount = latestEvents.length;
  }
}

function live() { return connected || previewMode; }

async function sendCmd(line) {
  if (previewMode) {
    previewCmd(line);
    return { ok: true };
  }
  if (!connected) return { ok: false, error: "Not connected" };
  return api("/api/cmd?line=" + encodeURIComponent(line), { method: "POST" });
}

async function sendHit(sw) {
  if (!live()) return;
  await sendCmd("hit " + sw);
}

async function sendReset() {
  if (!live()) return;
  failHolding = false;
  await sendCmd("reset");
}

async function holdFail() {
  if (!live() || failHolding) return;
  failHolding = true;
  hitFailBtn.classList.add("holding");
  await sendCmd("hold fail");
}

async function releaseFail() {
  if (!failHolding && !latest.fail_hold) return;
  failHolding = false;
  hitFailBtn.classList.remove("holding");
  if (live()) await sendCmd("release fail");
}

function sleep(ms, token) {
  return new Promise((resolve) => {
    const t = setTimeout(() => resolve(true), ms);
    const iv = setInterval(() => {
      if (token !== scriptToken || !live()) {
        clearTimeout(t);
        clearInterval(iv);
        resolve(false);
      }
    }, 50);
  });
}

async function waitForState(wanted, token, timeoutMs) {
  const start = performance.now();
  while (token === scriptToken && live()) {
    if (latest.state === wanted) return true;
    if (performance.now() - start > timeoutMs) return false;
    await sleep(80, token);
  }
  return false;
}

async function runScript(kind) {
  if (!live()) return;
  const token = ++scriptToken;
  await releaseFail();
  await sendCmd("reset");
  if (!(await sleep(350, token))) return;
  const need = Math.max(1, latest.need || 1);
  for (let i = 0; i < need; i++) {
    await sendHit("coin");
    if (!(await sleep(250, token))) return;
  }
  if (!(await waitForState("WAIT_START", token, 3000))) return;

  const failCount = kind === "threeFail" ? 3 : kind === "fail" ? 1 : 0;
  if (kind === "win") {
    await sendHit("start");
    if (!(await waitForState("PLAYING", token, 3000))) return;
    if (!(await sleep(400, token))) return;
    await sendHit("win");
    return;
  }

  for (let f = 0; f < failCount; f++) {
    await sendHit("start");
    if (!(await waitForState("PLAYING", token, 3000))) return;
    if (!(await sleep(300, token))) return;
    await holdFail();
    await sleep(200, token);
    await releaseFail();
    if (f < failCount - 1) {
      if (!(await waitForState("WAIT_START", token, 4000))) return;
      if (!(await sleep(200, token))) return;
    } else {
      await waitForState("ATTRACT", token, 4000);
    }
  }
}

async function toggleDip(n) {
  if (!live()) return;
  if (previewMode) return;  // DIP force is board-only
  const cur = dipForced[n];
  const live = !!latest["dip" + n];
  // cycle: hardware → force ON → force OFF → hardware
  if (cur == null) {
    dipForced[n] = true;
    await sendCmd(`dip ${n} on`);
  } else if (cur === true) {
    dipForced[n] = false;
    await sendCmd(`dip ${n} off`);
  } else {
    dipForced[n] = null;
    await sendCmd(`dip ${n} clear`);
  }
  syncDipButtons();
  void live;
}

document.getElementById("refresh").onclick = () => refreshPorts();
connectBtn.onclick = () => toggleConnect();
previewBtn.onclick = () => {
  if (connected) return;
  previewMode = !previewMode;
  previewBtn.classList.toggle("on", previewMode);
  previewBtn.textContent = previewMode ? "Exit preview" : "Preview (no board)";
  if (previewMode) {
    previewReset();
    pushPreviewEvent("sim", "preview_on");
    setControlsEnabled(true);
  } else {
    latest = {};
    setControlsEnabled(false);
    stateEl.textContent = "—";
    attemptsBadge.textContent = "Attempts left: —";
    nextHint.textContent = "Connect to the Nano, or click Preview (no board)";
    nextHint.className = "idle";
  }
};
document.getElementById("hitCoin").onclick = () => sendHit("coin");
document.getElementById("hitStart").onclick = () => sendHit("start");
document.getElementById("hitWin").onclick = () => sendHit("win");
document.getElementById("hitReset").onclick = () => sendReset();
document.getElementById("runWin").onclick = () => runScript("win");
document.getElementById("runFail").onclick = () => runScript("fail");
document.getElementById("runThreeFail").onclick = () => runScript("threeFail");
document.getElementById("runStop").onclick = () => { scriptToken++; releaseFail(); };
document.getElementById("dipsClear").onclick = async () => {
  dipForced = { 1: null, 2: null, 3: null, 4: null };
  await sendCmd("dips clear");
  syncDipButtons();
};
headerTestBtn.onclick = async () => {
  if (!live()) return;
  await sendCmd(latest.hdr_test ? "header test off" : "header test on");
};
for (let n = 1; n <= 4; n++) {
  document.getElementById("forceDip" + n).onclick = () => toggleDip(n);
}

let failPressAt = 0;
hitFailBtn.addEventListener("mousedown", (e) => {
  e.preventDefault();
  failPressAt = performance.now();
  holdFail();
});
hitFailBtn.addEventListener("mouseup", async () => {
  const held = performance.now() - failPressAt;
  await releaseFail();
  if (held < 180) await sendHit("fail");  // short click = tap
});
hitFailBtn.addEventListener("mouseleave", () => { if (failHolding) releaseFail(); });
hitFailBtn.addEventListener("touchstart", (e) => {
  e.preventDefault();
  failPressAt = performance.now();
  holdFail();
}, { passive: false });
hitFailBtn.addEventListener("touchend", async () => {
  const held = performance.now() - failPressAt;
  await releaseFail();
  if (held < 180) await sendHit("fail");
});

document.getElementById("exportLog").onclick = () => {
  const text = latestEvents.join("\n") + "\n";
  const blob = new Blob([text], { type: "text/plain" });
  const a = document.createElement("a");
  a.href = URL.createObjectURL(blob);
  a.download = "booze-barometer-events.txt";
  a.click();
  URL.revokeObjectURL(a.href);
};

document.addEventListener("keydown", (e) => {
  if (e.target && (e.target.tagName === "INPUT" || e.target.tagName === "SELECT" || e.target.tagName === "TEXTAREA")) return;
  if (e.key === "1") sendHit("coin");
  else if (e.key === "2") sendHit("start");
  else if (e.key === "3") sendHit("fail");
  else if (e.key === "4") sendHit("win");
  else if (e.key === "r" || e.key === "R") sendReset();
});

setControlsEnabled(false);
refreshPorts();
requestAnimationFrame(tickLeds);
setInterval(async () => {
  try {
    if (previewMode) {
      render({ connected: false, status_text: "Preview", status: latest, events: latestEvents, telemetry_age_ms: 0 });
    } else {
      render(await api("/api/status"));
    }
  } catch (e) {
    if (!previewMode) { statusEl.textContent = "Server unreachable"; statusEl.classList.add("stale"); }
  }
}, 100);
</script>
</body>
</html>
"""


class MonitorState:
    def __init__(self) -> None:
        self.lock = threading.Lock()
        self.ser: Optional[serial.Serial] = None
        self.reader: Optional[threading.Thread] = None
        self.stop_event = threading.Event()
        self.latest: dict[str, Any] = {}
        self.events: list[str] = ["Waiting for serial data…"]
        self.status_text = "Disconnected"
        self.selected_port = ""
        self.preferred_port: Optional[str] = None
        self.last_telemetry_at: float = 0.0

    def ports(self) -> list[dict[str, str]]:
        out = []
        for p in list_ports.comports():
            out.append({"device": p.device, "label": f"{p.device}  ({p.description})"})
        return out

    def pick_default(self) -> str:
        ports = self.ports()
        if not ports:
            return ""

        def is_nano(device: str) -> bool:
            d = device.lower()
            if any(b in d for b in ("debug-console", "bluetooth", "incoming", "wlan")):
                return False
            return ("usbserial" in d) or ("usbmodem" in d) or ("wchusbserial" in d)

        if self.preferred_port:
            for p in ports:
                if self.preferred_port in p["device"] or self.preferred_port in p["label"]:
                    return p["device"]
            return self.preferred_port
        for p in ports:
            if is_nano(p["device"]):
                return p["device"]
        return ""  # no board — use Preview mode

    def log(self, text: str) -> None:
        stamp = time.strftime("%H:%M:%S")
        line = f"{stamp}  {text}"
        with self.lock:
            self.events.append(line)
            if len(self.events) > 300:
                self.events = self.events[-300:]

    def send_line(self, line: str) -> tuple[bool, str]:
        payload = (line.strip() + "\n").encode("ascii", errors="ignore")
        with self.lock:
            if not self.ser or not self.ser.is_open:
                return False, "Not connected"
            try:
                self.ser.write(payload)
                self.ser.flush()
            except serial.SerialException as exc:
                return False, str(exc)
        self.log(f"> {line.strip()}")
        return True, "ok"

    def connect(self, device: str) -> tuple[bool, str]:
        self.disconnect()
        try:
            # Avoid toggling DTR/RTS on open — that resets the Nano and wipes credits.
            ser = serial.Serial()
            ser.port = device
            ser.baudrate = BAUD
            ser.timeout = 0.2
            ser.dsrdtr = False
            ser.rtscts = False
            ser.dtr = False
            ser.rts = False
            ser.open()
        except serial.SerialException as exc:
            self.status_text = f"Connect failed: {exc}"
            return False, self.status_text
        time.sleep(0.4)
        try:
            ser.reset_input_buffer()
        except Exception:
            pass
        with self.lock:
            self.ser = ser
            self.selected_port = device
            self.status_text = f"Connected · {device} @ {BAUD}"
            self.last_telemetry_at = 0.0
            self.latest = {}
        self.stop_event.clear()
        self.reader = threading.Thread(target=self._read_loop, daemon=True)
        self.reader.start()
        self.log(f"connected {device}")
        return True, self.status_text

    def disconnect(self) -> None:
        self.stop_event.set()
        if self.reader and self.reader.is_alive():
            self.reader.join(timeout=1.0)
        self.reader = None
        with self.lock:
            if self.ser:
                try:
                    self.ser.close()
                except Exception:
                    pass
            was = self.ser is not None
            self.ser = None
        if was:
            self.status_text = "Disconnected"
            self.log("disconnected")

    def _read_loop(self) -> None:
        buf = ""
        while not self.stop_event.is_set():
            try:
                with self.lock:
                    ser = self.ser
                if not ser:
                    break
                chunk = ser.read(256)
                if not chunk:
                    continue
                buf += chunk.decode("utf-8", errors="replace")
                while "\n" in buf:
                    line, buf = buf.split("\n", 1)
                    line = line.strip()
                    if line:
                        self._handle_line(line)
            except serial.SerialException as exc:
                self.status_text = f"Serial error: {exc}"
                self.disconnect()
                break
            except Exception:
                continue

    def _handle_line(self, line: str) -> None:
        try:
            msg = json.loads(line)
        except json.JSONDecodeError:
            return
        kind = msg.get("type")
        if kind == "status":
            with self.lock:
                self.latest = msg
                self.last_telemetry_at = time.time()
        elif kind == "event":
            self.log(f"{msg.get('name', '?')} = {msg.get('value', '')}")
        elif kind == "hello":
            self.log("board hello")

    def snapshot(self) -> dict[str, Any]:
        with self.lock:
            connected = bool(self.ser and self.ser.is_open)
            age = None
            if self.last_telemetry_at:
                age = int((time.time() - self.last_telemetry_at) * 1000)
            return {
                "connected": connected,
                "status_text": self.status_text,
                "selected": self.selected_port,
                "status": dict(self.latest),
                "events": list(self.events),
                "telemetry_age_ms": age,
            }


STATE = MonitorState()


class Handler(BaseHTTPRequestHandler):
    def log_message(self, fmt: str, *args: Any) -> None:
        return

    def _json(self, data: Any, code: int = 200) -> None:
        body = json.dumps(data).encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(body)

    def _html(self) -> None:
        body = HTML.encode("utf-8")
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self) -> None:
        path = urlparse(self.path).path
        if path in ("/", "/index.html"):
            self._html()
        elif path == "/api/ports":
            ports = STATE.ports()
            selected = STATE.selected_port or STATE.pick_default()
            self._json({"ports": ports, "selected": selected})
        elif path == "/api/status":
            self._json(STATE.snapshot())
        else:
            self.send_error(404)

    def do_POST(self) -> None:
        parsed = urlparse(self.path)
        path = parsed.path
        qs = parse_qs(parsed.query)
        if path == "/api/connect":
            device = (qs.get("port") or [""])[0] or STATE.pick_default()
            if not device:
                self._json({"ok": False, "error": "No serial port"}, 400)
                return
            ok, msg = STATE.connect(device)
            self._json({"ok": ok, "status_text": msg}, 200 if ok else 500)
        elif path == "/api/disconnect":
            STATE.disconnect()
            self._json({"ok": True})
        elif path == "/api/cmd":
            line = ((qs.get("line") or [""])[0] or "").strip()
            if not line:
                self._json({"ok": False, "error": "empty command"}, 400)
                return
            ok, msg = STATE.send_line(line)
            self._json({"ok": ok, "error": None if ok else msg}, 200 if ok else 500)
        elif path == "/api/hit":
            sw = ((qs.get("sw") or [""])[0] or "").strip().lower()
            if sw not in ("coin", "start", "fail", "win"):
                self._json({"ok": False, "error": "sw must be coin|start|fail|win"}, 400)
                return
            ok, msg = STATE.send_line(f"hit {sw}")
            self._json({"ok": ok, "error": None if ok else msg}, 200 if ok else 500)
        elif path == "/api/reset":
            ok, msg = STATE.send_line("reset")
            self._json({"ok": ok, "error": None if ok else msg}, 200 if ok else 500)
        else:
            self.send_error(404)


def main() -> None:
    parser = argparse.ArgumentParser(description="Booze Barometer serial monitor (browser)")
    parser.add_argument("--port", help="Serial device, e.g. /dev/cu.usbserial-1110")
    parser.add_argument("--http-port", type=int, default=HTTP_PORT)
    parser.add_argument("--no-browser", action="store_true")
    args = parser.parse_args()

    STATE.preferred_port = args.port
    STATE.selected_port = STATE.pick_default()

    server = ThreadingHTTPServer(("127.0.0.1", args.http_port), Handler)
    url = f"http://127.0.0.1:{args.http_port}/"
    print(f"Booze Barometer monitor: {url}")
    print("Leave this terminal open. Press Ctrl+C to quit.")

    # Only auto-connect when a real USB Nano port is present
    if STATE.selected_port and ("usbserial" in STATE.selected_port or "usbmodem" in STATE.selected_port or "wchusbserial" in STATE.selected_port):
        threading.Thread(target=lambda: STATE.connect(STATE.selected_port), daemon=True).start()
    else:
        STATE.status_text = "No Nano found — click Preview (no board) or plug in USB"

    if not args.no_browser:
        threading.Timer(0.4, lambda: webbrowser.open(url)).start()

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nStopping…")
    finally:
        STATE.disconnect()
        server.server_close()


if __name__ == "__main__":
    main()
