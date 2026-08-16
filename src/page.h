#pragma once

#include <Arduino.h>

// The whole UI, served from flash. Self-contained by choice, not necessity: the phone has
// its own internet, but this page is opened standing next to the pool, where phone signal
// is worst. A CDN round trip is the one thing that would make it hang there. It is ~6 KB,
// so serving it costs nothing.
static const char PAGE_HTML[] PROGMEM = R"HTML(<!DOCTYPE html>
<html lang="en"><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="color-scheme" content="dark">
<title>pool-lights</title>
<style>
:root{--bg:#0e1116;--card:#171c24;--line:#262d38;--fg:#e6eaf0;--dim:#8b95a5;
--accent:#4da3ff;--on:#3ddc84;--off:#4a5464}
*{box-sizing:border-box}
body{margin:0;padding:16px;background:var(--bg);color:var(--fg);
font:16px/1.45 system-ui,-apple-system,"Segoe UI",Roboto,sans-serif;
padding-bottom:calc(16px + env(safe-area-inset-bottom))}
main{max-width:520px;margin:0 auto}
h1{font-size:15px;letter-spacing:.14em;text-transform:uppercase;color:var(--dim);
margin:0 0 14px;font-weight:600}
.card{background:var(--card);border:1px solid var(--line);border-radius:14px;
padding:16px;margin-bottom:14px}
.hdr{display:flex;align-items:center;gap:10px;margin-bottom:14px}
.dot{width:12px;height:12px;border-radius:50%;background:var(--off);flex:none;
transition:background .2s}
.dot.on{background:var(--on);box-shadow:0 0 12px var(--on)}
.hdr b{font-size:20px}
.swatch{margin-left:auto;width:34px;height:34px;border-radius:9px;
border:1px solid var(--line)}
.grid{display:grid;grid-template-columns:repeat(2,1fr);gap:8px 14px}
.grid div{display:flex;justify-content:space-between;gap:8px;
border-bottom:1px solid var(--line);padding:5px 0;font-size:14px}
.grid span{color:var(--dim)}
.row{display:flex;gap:8px;margin-bottom:10px}
.row:last-child{margin-bottom:0}
button{flex:1;min-height:48px;border:1px solid var(--line);border-radius:11px;
background:#1e242e;color:var(--fg);font-size:15px;font-weight:600;cursor:pointer;
-webkit-tap-highlight-color:transparent;transition:transform .08s,background .15s}
button:active{transform:scale(.97)}
button.pri{background:var(--accent);border-color:var(--accent);color:#04121f}
button.sel{border-color:var(--accent);color:var(--accent)}
label{display:block;font-size:13px;color:var(--dim);margin:14px 0 7px}
label b{color:var(--fg);float:right;font-variant-numeric:tabular-nums}
input[type=range]{width:100%;height:30px;margin:0;background:transparent;
-webkit-appearance:none;appearance:none}
input[type=range]::-webkit-slider-runnable-track{height:10px;border-radius:5px;
background:var(--track,var(--line))}
input[type=range]::-moz-range-track{height:10px;border-radius:5px;
background:var(--track,var(--line))}
input[type=range]::-webkit-slider-thumb{-webkit-appearance:none;width:26px;height:26px;
border-radius:50%;background:#fff;border:2px solid #0e1116;margin-top:-8px;
box-shadow:0 1px 4px #0008}
input[type=range]::-moz-range-thumb{width:22px;height:22px;border-radius:50%;
background:#fff;border:2px solid #0e1116}
#hue{--track:linear-gradient(90deg,red,#ff0 17%,#0f0 33%,#0ff 50%,#00f 67%,#f0f 83%,red)}
footer{color:var(--dim);font-size:12px;text-align:center;margin-top:18px}
.stale{opacity:.45}
</style></head><body><main>
<h1>pool-lights</h1>

<div class="card">
  <div class="hdr"><span class="dot" id="dot"></span><b id="power">—</b>
    <span class="swatch" id="swatch"></span></div>
  <div class="grid">
    <div><span>Mode</span><b id="mode">—</b></div>
    <div><span>Brightness</span><b id="vbright">—</b></div>
    <div><span>Hue</span><b id="vhue">—</b></div>
    <div><span id="satlabel">Saturation</span><b id="vsat">—</b></div>
    <div><span>Effect</span><b id="veffect">—</b></div>
    <div><span>Night</span><b id="vnight">—</b></div>
  </div>
</div>

<div class="card">
  <div class="row">
    <button class="pri" onclick="cmd('on=1')">On</button>
    <button onclick="cmd('off=1')">Off</button>
    <button onclick="cmd('white=1')">White</button>
  </div>
  <div class="row">
    <button onclick="cmd('hue=0')">Red</button>
    <button onclick="cmd('hue=85')">Green</button>
    <button onclick="cmd('hue=171')">Blue</button>
  </div>

  <label>Hue <b id="lhue">0</b></label>
  <input type="range" id="hue" min="0" max="255" value="0">

  <label>Brightness <b id="lbright">100</b>%</label>
  <input type="range" id="bright" min="0" max="100" value="100">

  <label><span id="lsatname">Saturation</span> <b id="lsat">100</b>%</label>
  <input type="range" id="sat" min="0" max="100" value="100">

  <label>Effect</label>
  <div class="row" id="fx"></div>
</div>

<footer id="foot">connecting…</footer>
</main><script>
const $=i=>document.getElementById(i);
let ver=-1, held=0, fails=0;

// Ignore polled state for a moment after a local move: the radio burst takes a few
// hundred ms, so an in-flight poll would otherwise snap the slider back under the thumb.
const hold=()=>held=Date.now()+1200;

function cmd(q){hold();fetch('/api/set?'+q,{method:'POST'}).then(load).catch(()=>{})}

for(let i=0;i<5;i++){
  const b=document.createElement('button');
  b.textContent=i;b.id='fx'+i;b.onclick=()=>cmd('effect='+i);
  $('fx').appendChild(b);
}

function bind(id,label,q){
  const el=$(id);
  el.oninput=()=>{hold();$(label).textContent=el.value};
  el.onchange=()=>cmd(q+'='+el.value);
}
bind('hue','lhue','hue');
bind('bright','lbright','brightness');
bind('sat','lsat','sat');

function hsl(h,s){return 'hsl('+Math.round(h*360/256)+' '+s+'% 50%)'}

function render(s){
  $('dot').className='dot'+(s.on?' on':'');
  $('power').textContent=s.on?'On':'Off';
  $('mode').textContent=s.mode;
  $('vbright').textContent=s.brightness+'%';
  $('vhue').textContent=s.hue;
  $('vsat').textContent=(s.mode==='white'?s.kelvin:s.saturation)+'%';
  $('veffect').textContent=s.effect;
  $('vnight').textContent=s.night?'yes':'no';
  const satName=s.mode==='white'?'Kelvin':'Saturation';
  $('satlabel').textContent=satName;
  $('lsatname').textContent=satName;
  $('swatch').style.background=s.on?(s.mode==='white'?'#ffe9c4':hsl(s.hue,s.saturation)):'#000';
  for(let i=0;i<5;i++)$('fx'+i).className=(s.effect===i?'sel':'');

  if(Date.now()>held){
    $('hue').value=s.hue;$('lhue').textContent=s.hue;
    $('bright').value=s.brightness;$('lbright').textContent=s.brightness;
    const satVal=s.mode==='white'?s.kelvin:s.saturation;
    $('sat').value=satVal;$('lsat').textContent=satVal;
  }
  $('foot').textContent=s.host+' · '+s.ip+' · '+s.rssi+' dBm · up '+s.uptime+
    's · heap '+s.heap;
}

function load(){
  fetch('/api/state').then(r=>r.json()).then(s=>{
    fails=0;document.body.classList.remove('stale');
    if(s.version!==ver){ver=s.version;render(s)}
  }).catch(()=>{
    if(++fails>2)document.body.classList.add('stale');
  });
}
load();setInterval(load,500);
</script></body></html>)HTML";
