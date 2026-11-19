#ifndef WEB_UI_H
#define WEB_UI_H

#include <Arduino.h>

static const char WEB_UI_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1">
<title>PAD2PS3 Control Center</title>
<style>
:root {
  --bg: #090d16;
  --bg2: #111827;
  --card: #161f33;
  --card-hover: #1c2740;
  --border: #26334d;
  --text: #f1f5f9;
  --muted: #94a3b8;
  --primary: #3b82f6;
  --primary-glow: rgba(59,130,246,0.35);
  --accent: #06b6d4;
  --success: #10b981;
  --warn: #f59e0b;
  --danger: #ef4444;
  --ps-red: #f43f5e;
  --radius: 12px;
}
* { box-sizing: border-box; margin: 0; padding: 0; }
body {
  font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Inter, sans-serif;
  background: radial-gradient(circle at 50% 0%, #172554 0%, var(--bg) 55%);
  color: var(--text);
  min-height: 100vh;
  padding-bottom: 92px;
  line-height: 1.45;
}
.container { max-width: 1160px; margin: 0 auto; padding: 16px; }
header {
  display: flex; flex-wrap: wrap; align-items: center; justify-content: space-between;
  gap: 12px; background: rgba(22,31,51,0.85); backdrop-filter: blur(12px);
  border: 1px solid var(--border); border-radius: var(--radius);
  padding: 14px 18px; margin-bottom: 16px;
  box-shadow: 0 10px 30px rgba(0,0,0,0.35);
}
.brand { display: flex; align-items: center; gap: 12px; }
.brand-logo {
  width: 40px; height: 40px; border-radius: 10px;
  background: linear-gradient(135deg, var(--primary), var(--accent));
  display: flex; align-items: center; justify-content: center;
  font-weight: 800; font-size: 16px; letter-spacing: -0.5px; color: #fff;
  box-shadow: 0 0 16px var(--primary-glow);
}
.brand h1 { font-size: 18px; font-weight: 700; letter-spacing: -0.3px; }
.brand p { font-size: 12px; color: var(--muted); }
.status-bar { display: flex; flex-wrap: wrap; gap: 10px; align-items: center; }
.pill {
  display: flex; align-items: center; gap: 8px;
  background: var(--bg2); border: 1px solid var(--border);
  padding: 7px 12px; border-radius: 999px; font-size: 12px; font-weight: 500;
}
.dot { width: 9px; height: 9px; border-radius: 50%; background: var(--muted); }
.dot.ok { background: var(--success); box-shadow: 0 0 8px var(--success); }
.dot.warn { background: var(--warn); box-shadow: 0 0 8px var(--warn); animation: pulse 1.4s infinite; }
.dot.ps { background: var(--ps-red); box-shadow: 0 0 8px var(--ps-red); }
@keyframes pulse { 0%,100% { opacity: 1; } 50% { opacity: 0.35; } }

/* Live Telemetry Deck */
.telemetry-deck {
  background: rgba(22,31,51,0.75); border: 1px solid var(--border);
  border-radius: var(--radius); padding: 16px; margin-bottom: 16px;
  display: grid; grid-template-columns: 1fr 1fr 1.2fr; gap: 16px; align-items: center;
}
@media (max-width: 860px) { .telemetry-deck { grid-template-columns: 1fr; } }
.stick-monitor {
  display: flex; align-items: center; gap: 14px;
  background: var(--bg2); border: 1px solid var(--border);
  border-radius: 10px; padding: 12px;
}
.radar-wrap { position: relative; width: 96px; height: 96px; flex-shrink: 0; }
.radar-canvas { width: 96px; height: 96px; border-radius: 50%; background: #0b1120; border: 1px solid #334155; }
.stick-meta { flex: 1; font-size: 12px; }
.stick-meta h4 { font-size: 13px; font-weight: 600; margin-bottom: 6px; color: var(--text); }
.kv-row { display: flex; justify-content: space-between; color: var(--muted); margin-bottom: 3px; font-family: monospace; font-size: 12px; }
.kv-row span.val { color: var(--text); font-weight: 600; }

.trig-btn-monitor {
  background: var(--bg2); border: 1px solid var(--border);
  border-radius: 10px; padding: 12px; display: flex; flex-direction: column; gap: 10px;
}
.trig-row { display: flex; align-items: center; gap: 8px; font-size: 12px; }
.trig-label { width: 26px; font-weight: 700; color: var(--muted); }
.trig-bar-wrap {
  flex: 1; height: 14px; background: #0b1120; border: 1px solid #334155;
  border-radius: 7px; position: relative; overflow: hidden;
}
.trig-bar-raw {
  position: absolute; top: 0; left: 0; bottom: 0;
  background: rgba(148,163,184,0.28); width: 0%;
}
.trig-bar-out {
  position: absolute; top: 2px; left: 0; bottom: 2px;
  background: linear-gradient(90deg, var(--primary), var(--accent));
  border-radius: 5px; width: 0%;
}
.trig-val { width: 32px; text-align: right; font-family: monospace; font-size: 12px; }
.click-badge {
  font-size: 10px; font-weight: 700; padding: 1px 6px; border-radius: 4px;
  background: #1e293b; color: #64748b; border: 1px solid #334155;
}
.click-badge.active { background: rgba(16,185,129,0.2); color: #34d399; border-color: #10b981; }
.live-btns { display: flex; flex-wrap: wrap; gap: 4px; }
.b-chip {
  font-size: 10px; font-weight: 600; padding: 2px 6px; border-radius: 4px;
  background: #0b1120; color: #64748b; border: 1px solid #1e293b;
}
.b-chip.on { background: var(--primary); color: #fff; border-color: #60a5fa; box-shadow: 0 0 8px var(--primary-glow); }

/* Tabs */
.tabs {
  display: flex; gap: 8px; overflow-x: auto; margin-bottom: 16px;
  border-bottom: 1px solid var(--border); padding-bottom: 10px;
}
.tab-btn {
  background: var(--card); color: var(--muted); border: 1px solid var(--border);
  padding: 9px 16px; border-radius: 8px; font-size: 13px; font-weight: 600;
  cursor: pointer; white-space: nowrap; transition: all 0.15s;
}
.tab-btn:hover { color: var(--text); background: var(--card-hover); }
.tab-btn.active {
  background: var(--primary); color: #fff; border-color: var(--primary);
  box-shadow: 0 4px 14px var(--primary-glow);
}
.tab-panel { display: none; }
.tab-panel.active { display: block; }

/* Grid & Cards */
.grid-2 { display: grid; grid-template-columns: 1fr 1fr; gap: 16px; }
@media (max-width: 780px) { .grid-2 { grid-template-columns: 1fr; } }
.card {
  background: var(--card); border: 1px solid var(--border);
  border-radius: var(--radius); padding: 18px;
}
.card-header {
  display: flex; justify-content: space-between; align-items: center;
  margin-bottom: 14px; padding-bottom: 10px; border-bottom: 1px solid var(--border);
}
.card-header h3 { font-size: 15px; font-weight: 700; display: flex; align-items: center; gap: 8px; }
.curve-box {
  display: flex; gap: 16px; align-items: center; margin-bottom: 16px;
}
@media (max-width: 520px) { .curve-box { flex-direction: column; } }
.curve-canvas {
  width: 190px; height: 190px; background: #090d16;
  border: 1px solid #334155; border-radius: 10px; flex-shrink: 0;
}
.controls-col { flex: 1; width: 100%; display: flex; flex-direction: column; gap: 11px; }
.ctrl-group { display: flex; flex-direction: column; gap: 4px; }
.ctrl-label {
  display: flex; justify-content: space-between; font-size: 12px; color: var(--muted); font-weight: 500;
}
.ctrl-label b { color: var(--text); font-family: monospace; }
input[type=range] {
  -webkit-appearance: none; width: 100%; height: 6px;
  background: #0b1120; border-radius: 4px; border: 1px solid #334155; outline: none;
}
input[type=range]::-webkit-slider-thumb {
  -webkit-appearance: none; width: 16px; height: 16px; border-radius: 50%;
  background: var(--primary); cursor: pointer; border: 2px solid #fff;
  box-shadow: 0 0 8px var(--primary-glow);
}
select, input[type=text], input[type=password] {
  width: 100%; background: #0b1120; color: var(--text);
  border: 1px solid #334155; border-radius: 7px; padding: 8px 10px;
  font-size: 13px; outline: none;
}
select:focus, input[type=text]:focus, input[type=password]:focus { border-color: var(--primary); }
.check-row { display: flex; flex-wrap: wrap; gap: 14px; margin-top: 4px; }
.check-label { display: flex; align-items: center; gap: 6px; font-size: 12px; cursor: pointer; color: var(--text); }
.check-label input { width: 15px; height: 15px; accent-color: var(--primary); }

/* Buttons */
.btn {
  background: var(--bg2); color: var(--text); border: 1px solid var(--border);
  padding: 7px 13px; border-radius: 7px; font-size: 12px; font-weight: 600;
  cursor: pointer; transition: all 0.15s; display: inline-flex; align-items: center; gap: 6px;
}
.btn:hover { background: var(--card-hover); border-color: #475569; }
.btn.primary { background: var(--primary); border-color: var(--primary); color: #fff; }
.btn.primary:hover { background: #2563eb; }
.btn.success { background: var(--success); border-color: var(--success); color: #fff; }
.btn.danger { background: rgba(239,68,68,0.15); border-color: var(--danger); color: #fca5a5; }
.btn.danger:hover { background: var(--danger); color: #fff; }

/* Button Mapping Grid */
.preset-bar {
  display: flex; flex-wrap: wrap; gap: 8px; align-items: center;
  margin-bottom: 16px; background: var(--bg2); padding: 12px; border-radius: 10px; border: 1px solid var(--border);
}
.map-grid {
  display: grid; grid-template-columns: repeat(auto-fill, minmax(230px, 1fr)); gap: 10px;
}
.map-item {
  background: var(--bg2); border: 1px solid var(--border); border-radius: 9px;
  padding: 10px 12px; display: flex; flex-direction: column; gap: 6px; transition: border-color 0.15s, box-shadow 0.15s;
}
.map-item.pressed {
  border-color: var(--accent); box-shadow: 0 0 12px rgba(6,182,212,0.3);
}
.map-item-top { display: flex; justify-content: space-between; align-items: center; font-size: 12px; font-weight: 600; }
.map-src-badge { font-size: 10px; color: var(--muted); background: #090d16; padding: 2px 6px; border-radius: 4px; }

/* Sticky Footer */
.footer-bar {
  position: fixed; bottom: 0; left: 0; right: 0;
  background: rgba(15,23,42,0.92); backdrop-filter: blur(12px);
  border-top: 1px solid var(--border); padding: 12px 20px; z-index: 50;
}
.footer-inner {
  max-width: 1160px; margin: 0 auto; display: flex; justify-content: space-between; align-items: center; gap: 12px;
}
.toast {
  position: fixed; top: 18px; right: 18px; background: #1e293b; color: #fff;
  border-left: 4px solid var(--success); padding: 10px 16px; border-radius: 8px;
  font-size: 13px; font-weight: 600; box-shadow: 0 10px 25px rgba(0,0,0,0.4);
  transform: translateY(-80px); opacity: 0; transition: all 0.25s; z-index: 100;
}
.toast.show { transform: translateY(0); opacity: 1; }
</style>
</head>
<body>
<div id="toast" class="toast">Saved!</div>
<div class="container">
  <header>
    <div class="brand">
      <div class="brand-logo">P3</div>
      <div>
        <h1>PAD2PS3 Control Center</h1>
        <p>Wireless BLE Gamepad &rarr; PlayStation 3 Bridge</p>
      </div>
    </div>
    <div class="status-bar">
      <div class="pill">
        <div id="bleDot" class="dot warn"></div>
        <span id="bleText">BLE: Scanning...</span>
        <button class="btn" style="padding:3px 8px;font-size:11px" onclick="triggerAction('scan')">Rescan</button>
      </div>
      <div class="pill">
        <div id="usbDot" class="dot"></div>
        <span id="usbText">PS3 USB: Waiting</span>
        <button class="btn" style="padding:3px 8px;font-size:11px" onclick="triggerAction('ps_pulse')">Pulse PS</button>
      </div>
    </div>
  </header>

  <!-- Live Telemetry Strip -->
  <div class="telemetry-deck">
    <div class="stick-monitor">
      <div class="radar-wrap"><canvas id="radarL" class="radar-canvas" width="140" height="140"></canvas></div>
      <div class="stick-meta">
        <h4>Left Stick</h4>
        <div class="kv-row"><span>Raw X/Y:</span><span class="val" id="rawLVal">128, 128</span></div>
        <div class="kv-row"><span>PS3 X/Y:</span><span class="val" id="outLVal" style="color:var(--accent)">128, 128</span></div>
        <div class="kv-row"><span>Deflect:</span><span class="val" id="magLVal">0%</span></div>
      </div>
    </div>
    <div class="stick-monitor">
      <div class="radar-wrap"><canvas id="radarR" class="radar-canvas" width="140" height="140"></canvas></div>
      <div class="stick-meta">
        <h4>Right Stick</h4>
        <div class="kv-row"><span>Raw X/Y:</span><span class="val" id="rawRVal">128, 128</span></div>
        <div class="kv-row"><span>PS3 X/Y:</span><span class="val" id="outRVal" style="color:var(--accent)">128, 128</span></div>
        <div class="kv-row"><span>Deflect:</span><span class="val" id="magRVal">0%</span></div>
      </div>
    </div>
    <div class="trig-btn-monitor">
      <div class="trig-row">
        <span class="trig-label">L2</span>
        <div class="trig-bar-wrap">
          <div id="l2RawBar" class="trig-bar-raw"></div>
          <div id="l2OutBar" class="trig-bar-out"></div>
        </div>
        <span id="l2Val" class="trig-val">0</span>
        <span id="l2Click" class="click-badge">DIG</span>
      </div>
      <div class="trig-row">
        <span class="trig-label">R2</span>
        <div class="trig-bar-wrap">
          <div id="r2RawBar" class="trig-bar-raw"></div>
          <div id="r2OutBar" class="trig-bar-out"></div>
        </div>
        <span id="r2Val" class="trig-val">0</span>
        <span id="r2Click" class="click-badge">DIG</span>
      </div>
      <div id="liveBtns" class="live-btns"></div>
    </div>
  </div>

  <!-- Navigation Tabs -->
  <div class="tabs">
    <button class="tab-btn active" onclick="switchTab('sticks', this)">Analog Stick Curves</button>
    <button class="tab-btn" onclick="switchTab('triggers', this)">Trigger Curves (L2/R2)</button>
    <button class="tab-btn" onclick="switchTab('mapping', this)">Button Mappings &amp; D-Pad</button>
    <button class="tab-btn" onclick="switchTab('system', this)">Rumble &amp; WiFi Settings</button>
  </div>

  <!-- TAB 1: ANALOG STICKS -->
  <div id="tab-sticks" class="tab-panel active">
    <div style="display:flex;justify-content:space-between;align-items:center;margin-bottom:12px;flex-wrap:wrap;gap:8px">
      <label class="check-label" style="background:var(--card);padding:8px 14px;border-radius:8px;border:1px solid var(--border)">
        <input type="checkbox" id="swapSticks" onchange="onUiChange()">
        <b>Swap Left &amp; Right Analog Sticks</b>
      </label>
      <button class="btn" onclick="copyStick('LtoR')">Copy Left Stick Settings &rarr; Right Stick</button>
    </div>
    <div class="grid-2">
      <!-- Left Stick Card -->
      <div class="card">
        <div class="card-header">
          <h3>Left Analog Stick</h3>
          <span style="font-size:11px;color:var(--muted)">Movement / Steering</span>
        </div>
        <div class="curve-box">
          <canvas id="curveCanvasL" class="curve-canvas" width="220" height="220"></canvas>
          <div class="controls-col">
            <div class="ctrl-group">
              <div class="ctrl-label"><span>Activation Curve</span></div>
              <select id="ls_type" onchange="onStickTypeChange('ls')">
                <option value="0">Linear (1:1 Standard)</option>
                <option value="1">Exponential (Fine Center Precision)</option>
                <option value="2">S-Curve (Smooth Center &amp; Edge)</option>
                <option value="3">Aggressive (Quick / Instant Ramp)</option>
                <option value="4">Custom Power Exponent</option>
              </select>
            </div>
            <div class="ctrl-group">
              <div class="ctrl-label"><span>Curve Exponent / Steepness</span><b id="ls_param_v">1.60</b></div>
              <input type="range" id="ls_param" min="35" max="300" step="5" value="160" oninput="onUiChange()">
            </div>
            <div class="ctrl-group">
              <div class="ctrl-label"><span>Inner Deadzone</span><b id="ls_dz_in_v">5%</b></div>
              <input type="range" id="ls_dz_in" min="0" max="40" value="5" oninput="onUiChange()">
            </div>
            <div class="ctrl-group">
              <div class="ctrl-label"><span>Outer Saturation Deadzone</span><b id="ls_dz_out_v">2%</b></div>
              <input type="range" id="ls_dz_out" min="0" max="25" value="2" oninput="onUiChange()">
            </div>
            <div class="ctrl-group">
              <div class="ctrl-label"><span>Sensitivity Gain</span><b id="ls_sens_v">100%</b></div>
              <input type="range" id="ls_sens" min="50" max="150" value="100" oninput="onUiChange()">
            </div>
          </div>
        </div>
        <div class="check-row">
          <label class="check-label"><input type="checkbox" id="ls_inv_x" onchange="onUiChange()"> Invert X Axis</label>
          <label class="check-label"><input type="checkbox" id="ls_inv_y" onchange="onUiChange()"> Invert Y Axis</label>
        </div>
      </div>

      <!-- Right Stick Card -->
      <div class="card">
        <div class="card-header">
          <h3>Right Analog Stick</h3>
          <span style="font-size:11px;color:var(--muted)">Camera / Aiming</span>
        </div>
        <div class="curve-box">
          <canvas id="curveCanvasR" class="curve-canvas" width="220" height="220"></canvas>
          <div class="controls-col">
            <div class="ctrl-group">
              <div class="ctrl-label"><span>Activation Curve</span></div>
              <select id="rs_type" onchange="onStickTypeChange('rs')">
                <option value="0">Linear (1:1 Standard)</option>
                <option value="1">Exponential (Fine Aim Precision)</option>
                <option value="2">S-Curve (Smooth Center &amp; Edge)</option>
                <option value="3">Aggressive (Fast Turn Response)</option>
                <option value="4">Custom Power Exponent</option>
              </select>
            </div>
            <div class="ctrl-group">
              <div class="ctrl-label"><span>Curve Exponent / Steepness</span><b id="rs_param_v">1.60</b></div>
              <input type="range" id="rs_param" min="35" max="300" step="5" value="160" oninput="onUiChange()">
            </div>
            <div class="ctrl-group">
              <div class="ctrl-label"><span>Inner Deadzone</span><b id="rs_dz_in_v">5%</b></div>
              <input type="range" id="rs_dz_in" min="0" max="40" value="5" oninput="onUiChange()">
            </div>
            <div class="ctrl-group">
              <div class="ctrl-label"><span>Outer Saturation Deadzone</span><b id="rs_dz_out_v">2%</b></div>
              <input type="range" id="rs_dz_out" min="0" max="25" value="2" oninput="onUiChange()">
            </div>
            <div class="ctrl-group">
              <div class="ctrl-label"><span>Sensitivity Gain</span><b id="rs_sens_v">100%</b></div>
              <input type="range" id="rs_sens" min="50" max="150" value="100" oninput="onUiChange()">
            </div>
          </div>
        </div>
        <div class="check-row">
          <label class="check-label"><input type="checkbox" id="rs_inv_x" onchange="onUiChange()"> Invert X Axis</label>
          <label class="check-label"><input type="checkbox" id="rs_inv_y" onchange="onUiChange()"> Invert Y Axis</label>
        </div>
      </div>
    </div>
  </div>

  <!-- TAB 2: TRIGGERS -->
  <div id="tab-triggers" class="tab-panel">
    <div style="display:flex;justify-content:space-between;align-items:center;margin-bottom:12px;flex-wrap:wrap;gap:8px">
      <label class="check-label" style="background:var(--card);padding:8px 14px;border-radius:8px;border:1px solid var(--border)">
        <input type="checkbox" id="swapTriggers" onchange="onUiChange()">
        <b>Swap L2 &amp; R2 Triggers</b>
      </label>
      <button class="btn" onclick="copyTriggerLtoR()">Copy L2 Settings &rarr; R2 Trigger</button>
    </div>
    <div class="grid-2">
      <!-- L2 Trigger -->
      <div class="card">
        <div class="card-header">
          <h3>L2 Left Trigger</h3>
          <span style="font-size:11px;color:var(--muted)">Brake / Aim Down Sights</span>
        </div>
        <div class="curve-box">
          <canvas id="curveCanvasL2" class="curve-canvas" width="220" height="220"></canvas>
          <div class="controls-col">
            <div class="ctrl-group">
              <div class="ctrl-label"><span>Trigger Response Curve</span></div>
              <select id="l2_type" onchange="onTrigTypeChange('l2')">
                <option value="0">Linear (Standard)</option>
                <option value="1">Soft / Racing (Progressive Modulation)</option>
                <option value="2">S-Curve (Balanced)</option>
                <option value="3">Hair Trigger / Shooter (Fast Ramp)</option>
                <option value="4">Custom Power Curve</option>
              </select>
            </div>
            <div class="ctrl-group">
              <div class="ctrl-label"><span>Curve Exponent</span><b id="l2_param_v">1.50</b></div>
              <input type="range" id="l2_param" min="35" max="300" step="5" value="150" oninput="onUiChange()">
            </div>
            <div class="ctrl-group">
              <div class="ctrl-label"><span>Start Deadzone (Min)</span><b id="l2_min_v">4</b></div>
              <input type="range" id="l2_min" min="0" max="100" value="4" oninput="onUiChange()">
            </div>
            <div class="ctrl-group">
              <div class="ctrl-label"><span>Full Pull Saturation (Max)</span><b id="l2_max_v">252</b></div>
              <input type="range" id="l2_max" min="150" max="255" value="252" oninput="onUiChange()">
            </div>
            <div class="ctrl-group">
              <div class="ctrl-label"><span>Digital Button Click Point</span><b id="l2_dig_v">30</b></div>
              <input type="range" id="l2_dig" min="1" max="250" value="30" oninput="onUiChange()">
            </div>
          </div>
        </div>
      </div>

      <!-- R2 Trigger -->
      <div class="card">
        <div class="card-header">
          <h3>R2 Right Trigger</h3>
          <span style="font-size:11px;color:var(--muted)">Throttle / Fire Weapon</span>
        </div>
        <div class="curve-box">
          <canvas id="curveCanvasR2" class="curve-canvas" width="220" height="220"></canvas>
          <div class="controls-col">
            <div class="ctrl-group">
              <div class="ctrl-label"><span>Trigger Response Curve</span></div>
              <select id="r2_type" onchange="onTrigTypeChange('r2')">
                <option value="0">Linear (Standard)</option>
                <option value="1">Soft / Racing (Progressive Modulation)</option>
                <option value="2">S-Curve (Balanced)</option>
                <option value="3">Hair Trigger / Shooter (Fast Ramp)</option>
                <option value="4">Custom Power Curve</option>
              </select>
            </div>
            <div class="ctrl-group">
              <div class="ctrl-label"><span>Curve Exponent</span><b id="r2_param_v">1.50</b></div>
              <input type="range" id="r2_param" min="35" max="300" step="5" value="150" oninput="onUiChange()">
            </div>
            <div class="ctrl-group">
              <div class="ctrl-label"><span>Start Deadzone (Min)</span><b id="r2_min_v">4</b></div>
              <input type="range" id="r2_min" min="0" max="100" value="4" oninput="onUiChange()">
            </div>
            <div class="ctrl-group">
              <div class="ctrl-label"><span>Full Pull Saturation (Max)</span><b id="r2_max_v">252</b></div>
              <input type="range" id="r2_max" min="150" max="255" value="252" oninput="onUiChange()">
            </div>
            <div class="ctrl-group">
              <div class="ctrl-label"><span>Digital Button Click Point</span><b id="r2_dig_v">30</b></div>
              <input type="range" id="r2_dig" min="1" max="250" value="30" oninput="onUiChange()">
            </div>
          </div>
        </div>
      </div>
    </div>
  </div>

  <!-- TAB 3: BUTTON MAPPING -->
  <div id="tab-mapping" class="tab-panel">
    <div class="preset-bar">
      <span style="font-size:12px;font-weight:700;color:var(--muted);margin-right:4px">Quick Presets:</span>
      <button class="btn" onclick="applyMapPreset('default')">Default (1:1 PS3)</button>
      <button class="btn" onclick="applyMapPreset('nintendo')">Nintendo Swap (A&harr;B, X&harr;Y)</button>
      <button class="btn" onclick="applyMapPreset('shooter')">PS3 Shooter (Flip L1&harr;L2 &amp; R1&harr;R2)</button>
      <div style="margin-left:auto;display:flex;align-items:center;gap:8px">
        <span style="font-size:12px;color:var(--muted)">D-Pad Mode:</span>
        <select id="dpadMode" style="width:auto" onchange="onUiChange()">
          <option value="0">Standard PS3 D-Pad</option>
          <option value="1">D-Pad Drives Left Analog Stick</option>
          <option value="2">Both (D-Pad + Left Stick)</option>
        </select>
      </div>
    </div>
    <p style="font-size:12px;color:var(--muted);margin-bottom:12px">
      Tip: Press any button on your wireless controller to highlight its card below in real time. You can also assign Stadia's <b>Assistant</b> and <b>Capture</b> buttons to any PS3 function!
    </p>
    <div id="mapGrid" class="map-grid"></div>
  </div>

  <!-- TAB 4: RUMBLE & SYSTEM -->
  <div id="tab-system" class="tab-panel">
    <div class="grid-2">
      <div class="card">
        <div class="card-header">
          <h3>Haptic &amp; Rumble Feedback</h3>
          <button class="btn primary" onclick="triggerAction('rumble_test')">Test Vibration</button>
        </div>
        <div class="controls-col">
          <div class="ctrl-group">
            <div class="ctrl-label"><span>Master Rumble Intensity</span><b id="rmb_master_v">60%</b></div>
            <input type="range" id="rmb_master" min="0" max="100" value="60" oninput="onUiChange()">
          </div>
          <div class="ctrl-group">
            <div class="ctrl-label"><span>Left Heavy Motor Scale (Low-Freq)</span><b id="rmb_left_v">100%</b></div>
            <input type="range" id="rmb_left" min="0" max="100" value="100" oninput="onUiChange()">
          </div>
          <div class="ctrl-group">
            <div class="ctrl-label"><span>Right Light Motor Scale (High-Freq)</span><b id="rmb_right_v">100%</b></div>
            <input type="range" id="rmb_right" min="0" max="100" value="100" oninput="onUiChange()">
          </div>
        </div>
        <div style="margin-top:18px;padding-top:14px;border-top:1px solid var(--border)">
          <h4 style="font-size:13px;margin-bottom:10px">PS3 Console &amp; Bluetooth Commands</h4>
          <div style="display:flex;flex-wrap:wrap;gap:8px">
            <button class="btn" onclick="triggerAction('ps_pulse')">Tap PS Button</button>
            <button class="btn" onclick="triggerAction('ps_hold')">Hold PS (1.5s Menu)</button>
            <button class="btn" onclick="triggerAction('scan')">Reconnect / Scan BLE</button>
          </div>
        </div>
      </div>

      <div class="card">
        <div class="card-header">
          <h3>WiFi Hotspot &amp; Profile Backup</h3>
        </div>
        <div class="controls-col">
          <div class="ctrl-group">
            <div class="ctrl-label"><span>Hotspot SSID Name</span></div>
            <input type="text" id="wifi_ssid" maxlength="31" value="PAD2PS3-Config">
          </div>
          <div class="ctrl-group">
            <div class="ctrl-label"><span>WPA2 Password (min 8 chars, or leave blank to keep)</span></div>
            <input type="password" id="wifi_pass" maxlength="31" placeholder="Unchanged (default: ps3controller)">
          </div>
          <div class="ctrl-group">
            <div class="ctrl-label"><span>WiFi Channel (1-11)</span><b id="wifi_ch_v">6</b></div>
            <input type="range" id="wifi_ch" min="1" max="11" value="6" oninput="document.getElementById('wifi_ch_v').textContent=this.value">
          </div>
        </div>
        <div style="margin-top:18px;padding-top:14px;border-top:1px solid var(--border)">
          <h4 style="font-size:13px;margin-bottom:10px">Profile Export / Import</h4>
          <div style="display:flex;flex-wrap:wrap;gap:8px">
            <button class="btn" onclick="exportProfile()">Export JSON</button>
            <button class="btn" onclick="document.getElementById('importFile').click()">Import JSON</button>
            <input type="file" id="importFile" accept=".json" style="display:none" onchange="importProfile(event)">
            <button class="btn danger" onclick="resetDefaults()">Factory Reset</button>
          </div>
        </div>
      </div>
    </div>
  </div>
</div>

<!-- Sticky Save Footer -->
<div class="footer-bar">
  <div class="footer-inner">
    <div style="font-size:12px;color:var(--muted)">
      <span id="liveSyncStatus">Live changes auto-apply immediately while tuning.</span>
    </div>
    <div style="display:flex;gap:10px">
      <button class="btn" onclick="sendConfig(false)">Apply Live</button>
      <button class="btn primary" onclick="sendConfig(true)">Save to Flash (NVS)</button>
    </div>
  </div>
</div>

<script>
const BTN_DEFS = [
  { idx: 0,  name: "Cross (X)",       src: "Stadia A" },
  { idx: 1,  name: "Circle (O)",      src: "Stadia B" },
  { idx: 2,  name: "Square ([])",     src: "Stadia X" },
  { idx: 3,  name: "Triangle (/\\)",  src: "Stadia Y" },
  { idx: 4,  name: "L1 Bumper",       src: "Left Bumper" },
  { idx: 5,  name: "R1 Bumper",       src: "Right Bumper" },
  { idx: 6,  name: "L2 Trigger Click",src: "Left Trigger" },
  { idx: 7,  name: "R2 Trigger Click",src: "Right Trigger" },
  { idx: 8,  name: "Select",          src: "Options (...)" },
  { idx: 9,  name: "Start",           src: "Menu (===)" },
  { idx: 10, name: "L3 Stick Click",  src: "Left Thumb" },
  { idx: 11, name: "R3 Stick Click",  src: "Right Thumb" },
  { idx: 12, name: "PS Home Button",  src: "Stadia Logo" },
  { idx: 13, name: "Extra 1",         src: "Assistant Btn" },
  { idx: 14, name: "Extra 2",         src: "Capture Btn" }
];

const TARGET_OPTS = [
  { val: 0,  label: "PS3 Cross (X)" },
  { val: 1,  label: "PS3 Circle (O)" },
  { val: 2,  label: "PS3 Square ([])" },
  { val: 3,  label: "PS3 Triangle (/\\)" },
  { val: 4,  label: "PS3 L1 Bumper" },
  { val: 5,  label: "PS3 R1 Bumper" },
  { val: 6,  label: "PS3 L2 Trigger" },
  { val: 7,  label: "PS3 R2 Trigger" },
  { val: 8,  label: "PS3 Select" },
  { val: 9,  label: "PS3 Start" },
  { val: 10, label: "PS3 L3 Click" },
  { val: 11, label: "PS3 R3 Click" },
  { val: 12, label: "PS3 Home (PS)" },
  { val: 255, label: "Disabled / Unmapped" }
];

let buttonMap = [0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15];
let liveState = {
  raw: { lx:128, ly:128, rx:128, ry:128, l2:0, r2:0, b1:0, b2:0, dpad:0 },
  out: { lx:128, ly:128, rx:128, ry:128, l2:0, r2:0, b1:0, b2:0, dpad:0 }
};
let applyTimer = null;

function showToast(msg, isErr) {
  const t = document.getElementById('toast');
  t.textContent = msg;
  t.style.borderLeftColor = isErr ? 'var(--danger)' : 'var(--success)';
  t.classList.add('show');
  setTimeout(() => t.classList.remove('show'), 2200);
}

function switchTab(id, btn) {
  document.querySelectorAll('.tab-panel').forEach(p => p.classList.remove('active'));
  document.querySelectorAll('.tab-btn').forEach(b => b.classList.remove('active'));
  document.getElementById('tab-' + id).classList.add('active');
  btn.classList.add('active');
  drawAllCurves();
}

function initMappingGrid() {
  const grid = document.getElementById('mapGrid');
  grid.innerHTML = '';
  BTN_DEFS.forEach(b => {
    const div = document.createElement('div');
    div.className = 'map-item';
    div.id = 'map-card-' + b.idx;
    let optsHtml = TARGET_OPTS.map(o => `<option value="${o.val}">${o.label}</option>`).join('');
    if (b.idx >= 13) {
      optsHtml += `<option value="${b.idx}">Unassigned (${b.src})</option>`;
    }
    div.innerHTML = `
      <div class="map-item-top">
        <span>${b.src}</span>
        <span class="map-src-badge">Physical #${b.idx + 1}</span>
      </div>
      <select id="bmap_${b.idx}" onchange="onMapSelectChange(${b.idx}, this.value)">${optsHtml}</select>
    `;
    grid.appendChild(div);
  });

  const liveBtns = document.getElementById('liveBtns');
  liveBtns.innerHTML = '';
  const shortNames = ['X','O','[]','/\\','L1','R1','L2','R2','SEL','STA','L3','R3','PS'];
  shortNames.forEach((n, idx) => {
    const sp = document.createElement('span');
    sp.className = 'b-chip';
    sp.id = 'chip-' + idx;
    sp.textContent = n;
    liveBtns.appendChild(sp);
  });
}

function onMapSelectChange(idx, val) {
  buttonMap[idx] = parseInt(val, 10);
  onUiChange();
}

function applyMapPreset(preset) {
  buttonMap = [0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15];
  if (preset === 'nintendo') {
    buttonMap[0] = 1; buttonMap[1] = 0; // Swap A/B
    buttonMap[2] = 3; buttonMap[3] = 2; // Swap X/Y
  } else if (preset === 'shooter') {
    buttonMap[4] = 6; buttonMap[6] = 4; // Swap L1 <-> L2
    buttonMap[5] = 7; buttonMap[7] = 5; // Swap R1 <-> R2
  }
  syncMapDropdowns();
  onUiChange();
  showToast('Preset applied!');
}

function syncMapDropdowns() {
  BTN_DEFS.forEach(b => {
    const sel = document.getElementById('bmap_' + b.idx);
    if (sel) sel.value = buttonMap[b.idx];
  });
}

function evalNormCurve(x, type, paramVal) {
  x = Math.max(0, Math.min(1, x));
  if (x <= 0) return 0;
  if (x >= 1) return 1;
  const p = Math.max(0.3, Math.min(3.5, paramVal / 100.0));
  switch (parseInt(type, 10)) {
    case 0: return x;
    case 1: return Math.pow(x, p < 1.05 ? 1.65 : p);
    case 2: {
      const s = x * x * (3 - 2 * x);
      const b = Math.max(0.2, Math.min(1.0, (p - 0.5) / 2.0));
      return x * (1 - b) + s * b;
    }
    case 3: {
      let fe = p > 0.95 ? (1.0 / p) : p;
      fe = Math.max(0.35, Math.min(0.85, fe));
      return Math.pow(x, fe);
    }
    case 4: return Math.pow(x, p);
    default: return x;
  }
}

function onStickTypeChange(prefix) {
  const t = parseInt(document.getElementById(prefix + '_type').value, 10);
  const pSlider = document.getElementById(prefix + '_param');
  if (t === 0) pSlider.value = 100;
  else if (t === 1) pSlider.value = 165;
  else if (t === 2) pSlider.value = 180;
  else if (t === 3) pSlider.value = 160;
  onUiChange();
}

function onTrigTypeChange(prefix) {
  const t = parseInt(document.getElementById(prefix + '_type').value, 10);
  const pSlider = document.getElementById(prefix + '_param');
  if (t === 0) pSlider.value = 100;
  else if (t === 1) pSlider.value = 175;
  else if (t === 2) pSlider.value = 170;
  else if (t === 3) pSlider.value = 170;
  onUiChange();
}

function copyStick(dir) {
  if (dir === 'LtoR') {
    ['type','param','dz_in','dz_out','sens'].forEach(k => {
      document.getElementById('rs_' + k).value = document.getElementById('ls_' + k).value;
    });
    document.getElementById('rs_inv_x').checked = document.getElementById('ls_inv_x').checked;
    document.getElementById('rs_inv_y').checked = document.getElementById('ls_inv_y').checked;
    onUiChange();
    showToast('Copied Left Stick to Right Stick');
  }
}

function copyTriggerLtoR() {
  ['type','param','min','max','dig'].forEach(k => {
    document.getElementById('r2_' + k).value = document.getElementById('l2_' + k).value;
  });
  onUiChange();
  showToast('Copied L2 to R2 Trigger');
}

function updateLabels() {
  ['ls','rs'].forEach(p => {
    document.getElementById(p + '_param_v').textContent = (document.getElementById(p + '_param').value / 100).toFixed(2);
    document.getElementById(p + '_dz_in_v').textContent = document.getElementById(p + '_dz_in').value + '%';
    document.getElementById(p + '_dz_out_v').textContent = document.getElementById(p + '_dz_out').value + '%';
    document.getElementById(p + '_sens_v').textContent = document.getElementById(p + '_sens').value + '%';
  });
  ['l2','r2'].forEach(p => {
    document.getElementById(p + '_param_v').textContent = (document.getElementById(p + '_param').value / 100).toFixed(2);
    document.getElementById(p + '_min_v').textContent = document.getElementById(p + '_min').value;
    document.getElementById(p + '_max_v').textContent = document.getElementById(p + '_max').value;
    document.getElementById(p + '_dig_v').textContent = document.getElementById(p + '_dig').value;
  });
  document.getElementById('rmb_master_v').textContent = document.getElementById('rmb_master').value + '%';
  document.getElementById('rmb_left_v').textContent = document.getElementById('rmb_left').value + '%';
  document.getElementById('rmb_right_v').textContent = document.getElementById('rmb_right').value + '%';
}

function drawStickCurveCanvas(canvasId, prefix, rawX, rawY) {
  const c = document.getElementById(canvasId);
  if (!c) return;
  const ctx = c.getContext('2d');
  const w = c.width, h = c.height, pad = 22;
  const pw = w - pad * 2, ph = h - pad * 2;
  ctx.clearRect(0, 0, w, h);

  const type = parseInt(document.getElementById(prefix + '_type').value, 10);
  const param = parseInt(document.getElementById(prefix + '_param').value, 10);
  const dzIn = parseInt(document.getElementById(prefix + '_dz_in').value, 10) / 100;
  const dzOut = 1.0 - (parseInt(document.getElementById(prefix + '_dz_out').value, 10) / 100);
  const sens = parseInt(document.getElementById(prefix + '_sens').value, 10) / 100;

  // Grid
  ctx.strokeStyle = '#1e293b';
  ctx.lineWidth = 1;
  for (let i = 0; i <= 4; i++) {
    const gx = pad + (pw * i) / 4;
    const gy = pad + (ph * i) / 4;
    ctx.beginPath(); ctx.moveTo(gx, pad); ctx.lineTo(gx, pad + ph); ctx.stroke();
    ctx.beginPath(); ctx.moveTo(pad, gy); ctx.lineTo(pad + pw, gy); ctx.stroke();
  }

  // Linear reference diagonal
  ctx.strokeStyle = 'rgba(148,163,184,0.22)';
  ctx.setLineDash([4, 4]);
  ctx.beginPath(); ctx.moveTo(pad, pad + ph); ctx.lineTo(pad + pw, pad); ctx.stroke();
  ctx.setLineDash([]);

  // Response Curve
  ctx.strokeStyle = '#3b82f6';
  ctx.lineWidth = 2.5;
  ctx.beginPath();
  for (let i = 0; i <= 100; i++) {
    const x = i / 100;
    let y = 0;
    const effOut = Math.max(dzIn + 0.05, dzOut);
    if (x > dzIn) {
      const norm = Math.min(1, (x - dzIn) / (effOut - dzIn));
      y = Math.min(1, evalNormCurve(norm, type, param) * sens);
    }
    const px = pad + x * pw;
    const py = pad + ph - y * ph;
    if (i === 0) ctx.moveTo(px, py); else ctx.lineTo(px, py);
  }
  ctx.stroke();

  // Live input point on curve
  const dx = (rawX - 128) / 127, dy = (rawY - 128) / 127;
  const mag = Math.min(1, Math.sqrt(dx * dx + dy * dy));
  let outMag = 0;
  const effOut = Math.max(dzIn + 0.05, dzOut);
  if (mag > dzIn) {
    outMag = Math.min(1, evalNormCurve(Math.min(1, (mag - dzIn) / (effOut - dzIn)), type, param) * sens);
  }
  const dotX = pad + mag * pw;
  const dotY = pad + ph - outMag * ph;
  ctx.fillStyle = '#06b6d4';
  ctx.beginPath(); ctx.arc(dotX, dotY, 5, 0, Math.PI * 2); ctx.fill();
  ctx.strokeStyle = '#fff'; ctx.lineWidth = 1.5; ctx.stroke();
}

function drawTrigCurveCanvas(canvasId, prefix, rawVal) {
  const c = document.getElementById(canvasId);
  if (!c) return;
  const ctx = c.getContext('2d');
  const w = c.width, h = c.height, pad = 22;
  const pw = w - pad * 2, ph = h - pad * 2;
  ctx.clearRect(0, 0, w, h);

  const type = parseInt(document.getElementById(prefix + '_type').value, 10);
  const param = parseInt(document.getElementById(prefix + '_param').value, 10);
  const minV = parseInt(document.getElementById(prefix + '_min').value, 10);
  const maxV = Math.max(minV + 5, parseInt(document.getElementById(prefix + '_max').value, 10));
  const digV = parseInt(document.getElementById(prefix + '_dig').value, 10);

  ctx.strokeStyle = '#1e293b'; ctx.lineWidth = 1;
  for (let i = 0; i <= 4; i++) {
    const gx = pad + (pw * i) / 4, gy = pad + (ph * i) / 4;
    ctx.beginPath(); ctx.moveTo(gx, pad); ctx.lineTo(gx, pad + ph); ctx.stroke();
    ctx.beginPath(); ctx.moveTo(pad, gy); ctx.lineTo(pad + pw, gy); ctx.stroke();
  }

  // Digital actuation threshold line
  const digX = pad + (digV / 255) * pw;
  ctx.strokeStyle = 'rgba(16,185,129,0.45)';
  ctx.setLineDash([3, 3]);
  ctx.beginPath(); ctx.moveTo(digX, pad); ctx.lineTo(digX, pad + ph); ctx.stroke();
  ctx.setLineDash([]);

  // Curve
  ctx.strokeStyle = '#06b6d4'; ctx.lineWidth = 2.5;
  ctx.beginPath();
  for (let i = 0; i <= 255; i += 2) {
    let y = 0;
    if (i > minV) {
      const norm = Math.min(1, (i - minV) / (maxV - minV));
      y = evalNormCurve(norm, type, param);
    }
    const px = pad + (i / 255) * pw;
    const py = pad + ph - y * ph;
    if (i === 0) ctx.moveTo(px, py); else ctx.lineTo(px, py);
  }
  ctx.stroke();

  let outY = 0;
  if (rawVal > minV) {
    outY = evalNormCurve(Math.min(1, (rawVal - minV) / (maxV - minV)), type, param);
  }
  const dotX = pad + (rawVal / 255) * pw;
  const dotY = pad + ph - outY * ph;
  ctx.fillStyle = '#3b82f6';
  ctx.beginPath(); ctx.arc(dotX, dotY, 5, 0, Math.PI * 2); ctx.fill();
  ctx.strokeStyle = '#fff'; ctx.lineWidth = 1.5; ctx.stroke();
}

function drawRadar(canvasId, rx, ry, ox, oy, dzPct) {
  const c = document.getElementById(canvasId);
  if (!c) return;
  const ctx = c.getContext('2d');
  const w = c.width, h = c.height, cx = w / 2, cy = h / 2, r = w / 2 - 8;
  ctx.clearRect(0, 0, w, h);

  ctx.strokeStyle = '#1e293b'; ctx.lineWidth = 1;
  ctx.beginPath(); ctx.moveTo(cx, 4); ctx.lineTo(cx, h - 4); ctx.stroke();
  ctx.beginPath(); ctx.moveTo(4, cy); ctx.lineTo(w - 4, cy); ctx.stroke();

  if (dzPct > 0) {
    ctx.strokeStyle = 'rgba(244,63,94,0.35)';
    ctx.beginPath(); ctx.arc(cx, cy, r * (dzPct / 100), 0, Math.PI * 2); ctx.stroke();
  }

  // Raw dot
  const rpx = cx + ((rx - 128) / 127) * r;
  const rpy = cy + ((ry - 128) / 127) * r;
  ctx.fillStyle = 'rgba(148,163,184,0.5)';
  ctx.beginPath(); ctx.arc(rpx, rpy, 4, 0, Math.PI * 2); ctx.fill();

  // Processed output dot
  const opx = cx + ((ox - 128) / 127) * r;
  const opy = cy + ((oy - 128) / 127) * r;
  ctx.fillStyle = '#06b6d4';
  ctx.beginPath(); ctx.arc(opx, opy, 5.5, 0, Math.PI * 2); ctx.fill();
}

function drawAllCurves() {
  updateLabels();
  drawStickCurveCanvas('curveCanvasL', 'ls', liveState.raw.lx, liveState.raw.ly);
  drawStickCurveCanvas('curveCanvasR', 'rs', liveState.raw.rx, liveState.raw.ry);
  drawTrigCurveCanvas('curveCanvasL2', 'l2', liveState.raw.l2);
  drawTrigCurveCanvas('curveCanvasR2', 'r2', liveState.raw.r2);
}

function onUiChange() {
  drawAllCurves();
  if (applyTimer) clearTimeout(applyTimer);
  applyTimer = setTimeout(() => sendConfig(false, true), 140);
}

function buildConfigObject() {
  const obj = {
    leftStick: {
      curveType: parseInt(document.getElementById('ls_type').value, 10),
      deadzoneInner: parseInt(document.getElementById('ls_dz_in').value, 10),
      deadzoneOuter: parseInt(document.getElementById('ls_dz_out').value, 10),
      sensitivity: parseInt(document.getElementById('ls_sens').value, 10),
      curveParam: parseInt(document.getElementById('ls_param').value, 10),
      invertX: document.getElementById('ls_inv_x').checked,
      invertY: document.getElementById('ls_inv_y').checked
    },
    rightStick: {
      curveType: parseInt(document.getElementById('rs_type').value, 10),
      deadzoneInner: parseInt(document.getElementById('rs_dz_in').value, 10),
      deadzoneOuter: parseInt(document.getElementById('rs_dz_out').value, 10),
      sensitivity: parseInt(document.getElementById('rs_sens').value, 10),
      curveParam: parseInt(document.getElementById('rs_param').value, 10),
      invertX: document.getElementById('rs_inv_x').checked,
      invertY: document.getElementById('rs_inv_y').checked
    },
    swapSticks: document.getElementById('swapSticks').checked,
    triggerL2: {
      curveType: parseInt(document.getElementById('l2_type').value, 10),
      deadzoneMin: parseInt(document.getElementById('l2_min').value, 10),
      deadzoneMax: parseInt(document.getElementById('l2_max').value, 10),
      digitalThreshold: parseInt(document.getElementById('l2_dig').value, 10),
      curveParam: parseInt(document.getElementById('l2_param').value, 10)
    },
    triggerR2: {
      curveType: parseInt(document.getElementById('r2_type').value, 10),
      deadzoneMin: parseInt(document.getElementById('r2_min').value, 10),
      deadzoneMax: parseInt(document.getElementById('r2_max').value, 10),
      digitalThreshold: parseInt(document.getElementById('r2_dig').value, 10),
      curveParam: parseInt(document.getElementById('r2_param').value, 10)
    },
    swapTriggers: document.getElementById('swapTriggers').checked,
    dpadMode: parseInt(document.getElementById('dpadMode').value, 10),
    buttonMap: buttonMap,
    rumbleIntensity: parseInt(document.getElementById('rmb_master').value, 10),
    rumbleLeftScale: parseInt(document.getElementById('rmb_left').value, 10),
    rumbleRightScale: parseInt(document.getElementById('rmb_right').value, 10),
    wifiEnabled: true,
    wifiChannel: parseInt(document.getElementById('wifi_ch').value, 10),
    wifiSsid: document.getElementById('wifi_ssid').value.trim() || "PAD2PS3-Config"
  };
  const pass = document.getElementById('wifi_pass').value;
  if (pass.length >= 8) obj.wifiPassword = pass;
  return obj;
}

function applyConfigToUi(c) {
  if (!c) return;
  if (c.leftStick) {
    document.getElementById('ls_type').value = c.leftStick.curveType;
    document.getElementById('ls_dz_in').value = c.leftStick.deadzoneInner;
    document.getElementById('ls_dz_out').value = c.leftStick.deadzoneOuter;
    document.getElementById('ls_sens').value = c.leftStick.sensitivity;
    document.getElementById('ls_param').value = c.leftStick.curveParam;
    document.getElementById('ls_inv_x').checked = !!c.leftStick.invertX;
    document.getElementById('ls_inv_y').checked = !!c.leftStick.invertY;
  }
  if (c.rightStick) {
    document.getElementById('rs_type').value = c.rightStick.curveType;
    document.getElementById('rs_dz_in').value = c.rightStick.deadzoneInner;
    document.getElementById('rs_dz_out').value = c.rightStick.deadzoneOuter;
    document.getElementById('rs_sens').value = c.rightStick.sensitivity;
    document.getElementById('rs_param').value = c.rightStick.curveParam;
    document.getElementById('rs_inv_x').checked = !!c.rightStick.invertX;
    document.getElementById('rs_inv_y').checked = !!c.rightStick.invertY;
  }
  document.getElementById('swapSticks').checked = !!c.swapSticks;
  if (c.triggerL2) {
    document.getElementById('l2_type').value = c.triggerL2.curveType;
    document.getElementById('l2_min').value = c.triggerL2.deadzoneMin;
    document.getElementById('l2_max').value = c.triggerL2.deadzoneMax;
    document.getElementById('l2_dig').value = c.triggerL2.digitalThreshold;
    document.getElementById('l2_param').value = c.triggerL2.curveParam;
  }
  if (c.triggerR2) {
    document.getElementById('r2_type').value = c.triggerR2.curveType;
    document.getElementById('r2_min').value = c.triggerR2.deadzoneMin;
    document.getElementById('r2_max').value = c.triggerR2.deadzoneMax;
    document.getElementById('r2_dig').value = c.triggerR2.digitalThreshold;
    document.getElementById('r2_param').value = c.triggerR2.curveParam;
  }
  document.getElementById('swapTriggers').checked = !!c.swapTriggers;
  if (typeof c.dpadMode === 'number') document.getElementById('dpadMode').value = c.dpadMode;
  if (Array.isArray(c.buttonMap) && c.buttonMap.length >= 15) {
    buttonMap = c.buttonMap.slice(0, 16);
    syncMapDropdowns();
  }
  if (typeof c.rumbleIntensity === 'number') document.getElementById('rmb_master').value = c.rumbleIntensity;
  if (typeof c.rumbleLeftScale === 'number') document.getElementById('rmb_left').value = c.rumbleLeftScale;
  if (typeof c.rumbleRightScale === 'number') document.getElementById('rmb_right').value = c.rumbleRightScale;
  if (typeof c.wifiChannel === 'number') {
    document.getElementById('wifi_ch').value = c.wifiChannel;
    document.getElementById('wifi_ch_v').textContent = c.wifiChannel;
  }
  if (c.wifiSsid) document.getElementById('wifi_ssid').value = c.wifiSsid;
  drawAllCurves();
}

async function loadConfig() {
  try {
    const res = await fetch('/api/config');
    const data = await res.json();
    applyConfigToUi(data);
  } catch (e) {}
}

async function sendConfig(save, silent) {
  try {
    const payload = buildConfigObject();
    const res = await fetch('/api/config?save=' + (save ? '1' : '0'), {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(payload)
    });
    if (res.ok && !silent) {
      showToast(save ? 'Configuration saved to NVS Flash!' : 'Configuration applied live!');
    }
  } catch (e) {
    if (!silent) showToast('Connection error', true);
  }
}

async function resetDefaults() {
  if (!confirm('Reset all sticks, triggers, and button mappings to factory defaults?')) return;
  try {
    const res = await fetch('/api/reset', { method: 'POST' });
    const data = await res.json();
    applyConfigToUi(data);
    showToast('Factory defaults restored!');
  } catch (e) {
    showToast('Reset failed', true);
  }
}

async function triggerAction(act) {
  try {
    await fetch('/api/action?cmd=' + encodeURIComponent(act), { method: 'POST' });
    showToast('Action triggered: ' + act);
  } catch (e) {}
}

function exportProfile() {
  const dataStr = "data:text/json;charset=utf-8," + encodeURIComponent(JSON.stringify(buildConfigObject(), null, 2));
  const a = document.createElement('a');
  a.setAttribute('href', dataStr);
  a.setAttribute('download', 'pad2ps3-profile.json');
  document.body.appendChild(a);
  a.click();
  a.remove();
}

function importProfile(ev) {
  const file = ev.target.files[0];
  if (!file) return;
  const reader = new FileReader();
  reader.onload = e => {
    try {
      const cfg = JSON.parse(e.target.result);
      applyConfigToUi(cfg);
      sendConfig(false);
      showToast('Profile imported!');
    } catch (err) {
      showToast('Invalid JSON file', true);
    }
  };
  reader.readAsText(file);
}

async function pollTelemetry() {
  try {
    const res = await fetch('/api/status');
    const s = await res.json();
    liveState = s;

    // BLE status
    const bleDot = document.getElementById('bleDot');
    const bleText = document.getElementById('bleText');
    if (s.ble.connected) {
      bleDot.className = 'dot ok';
      bleText.textContent = `${s.ble.name} (${s.ble.rssi} dBm)`;
    } else {
      bleDot.className = 'dot warn';
      bleText.textContent = s.ble.scanning ? 'BLE: Scanning...' : 'BLE: Idle';
    }

    // USB status
    const usbDot = document.getElementById('usbDot');
    const usbText = document.getElementById('usbText');
    if (s.usb.paired) {
      usbDot.className = 'dot ps';
      usbText.textContent = `PS3 Active ${s.usb.player > 0 ? '(Player ' + s.usb.player + ')' : ''}`;
    } else if (s.usb.mounted) {
      usbDot.className = 'dot warn';
      usbText.textContent = 'USB Mounted (Handshaking)';
    } else {
      usbDot.className = 'dot';
      usbText.textContent = 'PS3 USB: Disconnected';
    }

    // Sticks
    document.getElementById('rawLVal').textContent = `${s.raw.lx}, ${s.raw.ly}`;
    document.getElementById('outLVal').textContent = `${s.out.lx}, ${s.out.ly}`;
    const magL = Math.min(100, Math.round(Math.hypot(s.out.lx - 128, s.out.ly - 128) / 1.27));
    document.getElementById('magLVal').textContent = magL + '%';

    document.getElementById('rawRVal').textContent = `${s.raw.rx}, ${s.raw.ry}`;
    document.getElementById('outRVal').textContent = `${s.out.rx}, ${s.out.ry}`;
    const magR = Math.min(100, Math.round(Math.hypot(s.out.rx - 128, s.out.ry - 128) / 1.27));
    document.getElementById('magRVal').textContent = magR + '%';

    drawRadar('radarL', s.raw.lx, s.raw.ly, s.out.lx, s.out.ly, parseInt(document.getElementById('ls_dz_in').value, 10));
    drawRadar('radarR', s.raw.rx, s.raw.ry, s.out.rx, s.out.ry, parseInt(document.getElementById('rs_dz_in').value, 10));

    // Triggers
    document.getElementById('l2RawBar').style.width = ((s.raw.l2 / 255) * 100) + '%';
    document.getElementById('l2OutBar').style.width = ((s.out.l2 / 255) * 100) + '%';
    document.getElementById('l2Val').textContent = s.out.l2;
    document.getElementById('l2Click').className = 'click-badge' + ((s.out.b1 & (1 << 6)) ? ' active' : '');

    document.getElementById('r2RawBar').style.width = ((s.raw.r2 / 255) * 100) + '%';
    document.getElementById('r2OutBar').style.width = ((s.out.r2 / 255) * 100) + '%';
    document.getElementById('r2Val').textContent = s.out.r2;
    document.getElementById('r2Click').className = 'click-badge' + ((s.out.b1 & (1 << 7)) ? ' active' : '');

    // Highlight active PS3 buttons & physical button cards
    const outMask = s.out.b1 | (s.out.b2 << 8);
    const rawMask = s.raw.b1 | (s.raw.b2 << 8);
    for (let i = 0; i < 13; i++) {
      const ch = document.getElementById('chip-' + i);
      if (ch) ch.className = 'b-chip' + ((outMask & (1 << i)) ? ' on' : '');
    }
    for (let i = 0; i < 15; i++) {
      const card = document.getElementById('map-card-' + i);
      if (card) card.className = 'map-item' + ((rawMask & (1 << i)) ? ' pressed' : '');
    }

    drawAllCurves();
  } catch (e) {}
}

window.addEventListener('DOMContentLoaded', () => {
  initMappingGrid();
  loadConfig();
  setInterval(pollTelemetry, 120);
});
</script>
</body>
</html>
)rawliteral";

#endif // WEB_UI_H
