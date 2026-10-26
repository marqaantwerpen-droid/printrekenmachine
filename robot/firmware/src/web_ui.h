// Webpagina (op /) en de API-beschrijving (op /api/help).
#pragma once
#include <Arduino.h>

const char API_HELP[] PROGMEM = R"TXT(SPIN robot API  (alle hoeken in graden, 90 = neutraal)

GET  /api/status        toestand: actie, servohoeken, hall-sensor, accu, gebeurtenissen, moves
GET  /api/help          deze tekst
GET  /api/move?name=x   een opgeslagen move als JSON
GET  /api/calibration   offsets en draairichting per servo
POST /api/cmd           JSON-commando, of een lijst commando's (worden na elkaar uitgevoerd)
Header X-API-Key of ?key=  alleen als API_KEY in secrets.h is ingesteld

Commando's  {"cmd": ...}
  stop                                         alles direct stoppen, wachtrij leeg
  relax                                        servo's slap (stroom besparen)
  wake                                         opstaan
  pose     name=stand|sit|rest|neutral  [height 35-90] [reach 70-120] [ms]
  height   height=35-90 [ms]                   lichaam hoger/lager
  walk     dir=forward|backward|left|right|turn_left|turn_right
           [steps 1-40] [speed 0.3-2.0] [stride 10-60 mm]
  servo    id=0-13 angle=10-170 [ms]   of   angles=[14 waarden of null] [ms]
  head     [pan 40-140] [tilt 50-130] [ms]
  wait     ms=40-10000
  play     name=<move> [repeat 1-20]
           ingebouwd: wave bow dance shake stretch look_around hello
  face     expr=neutral|happy|sad|angry|surprised|sleep|love|wink   of   text="..." [ms]
  led      mode=off|solid|breathe|blink|rainbow|scan [color "#RRGGBB"|red|green|...] [brightness 0-255]
  save_move name=<a-z0-9_-> frames=[{"ms":300,"a":[14 hoeken of null]}, ...]   (max 64 frames)
  delete_move name=<move>
  calibrate id=0-13 [offset -30..30] [dir 1|-1] [save true]
  calibrate_save | calibrate_reset

Servo-indexen: 0-2 voor-links (heup,dij,scheen)  3-5 voor-rechts  6-8 achter-links
               9-11 achter-rechts  12 hoofd pan  13 hoofd tilt
Bewegingscommando's wachten op elkaar; face/led werken direct (behalve in een lijst).

Voorbeeld:
  curl -X POST http://spin.local/api/cmd -d '[{"cmd":"walk","dir":"forward","steps":3},{"cmd":"play","name":"wave"},{"cmd":"face","expr":"happy"}]'
)TXT";

const char INDEX_HTML[] PROGMEM = R"HTML(<!doctype html><html lang="nl"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>SPIN robot</title><style>
:root{--bg:#101418;--card:#1b2128;--fg:#e8edf2;--mut:#8a96a3;--acc:#3d8bff}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--fg);font:15px system-ui,sans-serif}
main{max-width:760px;margin:auto;padding:16px}h1{font-size:22px;margin:4px 0 12px}h2{font-size:15px;color:var(--mut);margin:0 0 8px}
section{background:var(--card);border-radius:12px;padding:14px;margin-bottom:12px}
button{background:#2a323c;color:var(--fg);border:0;border-radius:8px;padding:10px 12px;margin:3px;font-size:14px;cursor:pointer}
button:active{background:var(--acc)}.stop{background:#c62f3a;width:100%;font-weight:700}
.pad{display:grid;grid-template-columns:repeat(3,1fr);gap:4px;max-width:300px}
.row{display:flex;flex-wrap:wrap;align-items:center;gap:6px}label{font-size:13px;color:var(--mut)}
input[type=range]{width:100%}pre{white-space:pre-wrap;font-size:12px;color:var(--mut);max-height:220px;overflow:auto}
.s{display:grid;grid-template-columns:110px 1fr 36px;gap:6px;align-items:center;font-size:13px}
</style></head><body><main>
<h1>SPIN robot</h1>
<section><button class="stop" onclick="cmd({cmd:'stop'})">STOP</button></section>
<section><h2>Lopen</h2><div class="pad">
<button onclick="w('turn_left')">&#8634;</button><button onclick="w('forward')">&#9650;</button><button onclick="w('turn_right')">&#8635;</button>
<button onclick="w('left')">&#9664;</button><button onclick="cmd({cmd:'pose',name:'stand'})">sta</button><button onclick="w('right')">&#9654;</button>
<span></span><button onclick="w('backward')">&#9660;</button><span></span></div>
<div class="row"><label>stappen <input id="steps" type="number" value="3" min="1" max="40" style="width:56px"></label>
<label>snelheid <input id="speed" type="number" value="1" step="0.1" min="0.3" max="2" style="width:56px"></label></div></section>
<section><h2>Houding &amp; moves</h2><div class="row" id="moves"></div>
<div class="row"><button onclick="cmd({cmd:'pose',name:'sit'})">zit</button><button onclick="cmd({cmd:'pose',name:'rest'})">lig</button>
<button onclick="cmd({cmd:'relax'})">slap</button><button onclick="cmd({cmd:'wake'})">wakker</button></div></section>
<section><h2>Gezicht &amp; licht</h2><div class="row">
<button onclick="f('happy')">blij</button><button onclick="f('sad')">verdrietig</button><button onclick="f('angry')">boos</button>
<button onclick="f('surprised')">verrast</button><button onclick="f('love')">verliefd</button><button onclick="f('sleep')">slaap</button>
<input id="txt" placeholder="tekst" style="padding:8px;border-radius:8px;border:0"><button onclick="cmd({cmd:'face',text:txt.value})">toon</button></div>
<div class="row"><input id="col" type="color" value="#3d8bff">
<button onclick="l('solid')">vast</button><button onclick="l('breathe')">ademen</button><button onclick="l('rainbow')">regenboog</button>
<button onclick="l('scan')">scan</button><button onclick="l('off')">uit</button></div></section>
<section><h2>Servo's &amp; moves aanleren</h2><div id="sl"></div>
<div class="row" style="margin-top:8px"><label>frame-tijd ms <input id="fms" type="number" value="400" style="width:70px"></label>
<button onclick="addFrame()">+ frame</button><span id="nf">0 frames</span>
<input id="mname" placeholder="naam" style="padding:8px;border-radius:8px;border:0;width:110px">
<button onclick="saveMove()">opslaan</button><button onclick="frames=[];nf.textContent='0 frames'">wis</button></div></section>
<section><h2>Status</h2><pre id="st">...</pre></section>
</main><script>
const N=['VL heup','VL dij','VL scheen','VR heup','VR dij','VR scheen','AL heup','AL dij','AL scheen','AR heup','AR dij','AR scheen','hoofd pan','hoofd tilt'];
let frames=[],dragging=-1;
async function cmd(c){const r=await fetch('/api/cmd',{method:'POST',body:JSON.stringify(c)});const j=await r.json();if(!j.ok)alert(j.error);return j}
function w(d){cmd({cmd:'walk',dir:d,steps:+steps.value,speed:+speed.value})}
function f(e){cmd({cmd:'face',expr:e})}
function l(m){cmd({cmd:'led',mode:m,color:col.value})}
sl.innerHTML=N.map((n,i)=>`<div class="s"><span>${i} ${n}</span><input type="range" min="10" max="170" value="90" id="r${i}"
 onpointerdown="dragging=${i}" onpointerup="dragging=-1" oninput="v${i}.textContent=this.value;cmd({cmd:'servo',id:${i},angle:+this.value,ms:120})"><b id="v${i}">90</b></div>`).join('');
function addFrame(){frames.push({ms:+fms.value,a:N.map((_,i)=>+document.getElementById('r'+i).value)});nf.textContent=frames.length+' frames'}
async function saveMove(){const j=await cmd({cmd:'save_move',name:mname.value,frames});if(j.ok){frames=[];nf.textContent='opgeslagen';poll()}}
async function poll(){try{const s=await (await fetch('/api/status')).json();
 s.angles.forEach((a,i)=>{if(dragging!==i){document.getElementById('r'+i).value=a;document.getElementById('v'+i).textContent=Math.round(a)}});
 moves.innerHTML=s.moves.map(m=>`<button onclick="cmd({cmd:'play',name:'${m.name}'})">${m.name}</button>`+(m.builtin?'':`<button onclick="if(confirm('verwijder ${m.name}?'))cmd({cmd:'delete_move',name:'${m.name}'})">&#10005;</button>`)).join('');
 delete s.moves;delete s.angles;st.textContent=JSON.stringify(s,null,1)}catch(e){st.textContent='geen verbinding'}}
poll();setInterval(poll,1500);
</script></body></html>)HTML";
