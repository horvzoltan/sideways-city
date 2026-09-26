(() => {
const cv = document.getElementById('c'); let ctx = cv.getContext('2d');
let W, H, DPR;
function resize(){ DPR=Math.min(2,window.devicePixelRatio||1); W=innerWidth; H=innerHeight;
  cv.width=W*DPR; cv.height=H*DPR; cv.style.width=W+'px'; cv.style.height=H+'px'; }
addEventListener('resize', resize); resize();

const isTouch = matchMedia('(pointer:coarse)').matches || 'ontouchstart' in window;
if (isTouch) document.body.classList.add('is-touch');

// ---------- world: a little Miami Beach ----------
const TILE=64, BLOCK=8, ROADW=2, NB=8, MW=NB*BLOCK, MH=NB*BLOCK;
const WORLD_W=MW*TILE, WORLD_H=MH*TILE;
const ROAD=0, WALK=1, GRASS=2, LOT=3, SAND=5, WATER=6;   // 4 is GRAVEL, used by the tracks
const BAY_X=7, BEACH_X=58, SEA_X=62;   // tile columns: Biscayne Bay west of BAY_X, promenade at BEACH_X, then sand, ocean from SEA_X
const CAUSEWAYS=[2,5];                 // block rows whose street crosses the bay
const map=new Uint8Array(MW*MH);
const buildings=[], solids=[], scenery=[];
let seed=1337; const rnd=()=>{ seed=(seed*16807)%2147483647; return (seed-1)/2147483646; };
const pick=a=>a[Math.floor(rnd()*a.length)];
const DECO=['#f4a7b9','#9ee0cc','#8fd3e8','#f8c89a','#cbb7e6','#f3e3a0','#f1ede4','#ffb3a1'];   // art deco pastels
const ACCENT=['#2bb5b0','#e2598b','#f39a4a','#5b8fd6','#f1ede4'];
const TOWERS=['#eef0ee','#d9e6ea','#ebe4d6','#d3e2e0'];
const FRONDS=['#3f8a3a','#4a9a3c','#367a34','#52a043'];

function addBuilding(tx,ty,tw,th,tall){
  const pad=6, pool=(tall||tw*th>=8)&&rnd()<0.5;
  const b={x:tx*TILE+pad,y:ty*TILE+pad,w:tw*TILE-pad*2,h:th*TILE-pad*2,
    ht:tall?0.38+rnd()*0.26:0.1+rnd()*0.14, col:pick(tall?TOWERS:DECO), acc:tall?null:pick(ACCENT),
    vents:pool?0:Math.floor(rnd()*3), pool, s:rnd()};
  b.cx=b.x+b.w/2; b.cy=b.y+b.h/2;
  buildings.push(b); solids.push(b); scenery.push(b);
}
function addPalm(x,y){ scenery.push({t:'palm',x,y,cx:x,cy:y,r:22+rnd()*10,ht:0.07+rnd()*0.05,n:7+Math.floor(rnd()*2),a:rnd()*7,col:pick(FRONDS)}); }

for(let y=0;y<MH;y++)for(let x=0;x<MW;x++){
  const streetRow=y%BLOCK<ROADW;
  let t=(x%BLOCK<ROADW || streetRow) ? ROAD : WALK;
  if(x<=BAY_X) t=streetRow && CAUSEWAYS.includes(Math.floor(y/BLOCK)) ? ROAD : x<BAY_X ? WATER : WALK;
  else if(x>=SEA_X) t=WATER;
  else if(x>BEACH_X) t=SAND;
  else if(x===BEACH_X) t=WALK;
  map[y*MW+x]=t;
}
for(let j=0;j<NB;j++)for(let i=1;i<NB-1;i++){
  const x0=i*BLOCK+ROADW, y0=j*BLOCK+ROADW, S=BLOCK-ROADW;
  const r=rnd(), tall=rnd()<(i>=5?0.5:0.15);   // high-rises cluster near the ocean
  if (r<0.14){
    for(let y=1;y<S-1;y++)for(let x=1;x<S-1;x++) map[(y0+y)*MW+x0+x]=GRASS;
    for(let k=0;k<7;k++) addPalm((x0+1.4+rnd()*(S-2.8))*TILE,(y0+1.4+rnd()*(S-2.8))*TILE);
  }
  else if (r<0.28){ for(let y=0;y<S;y++)for(let x=0;x<S;x++) map[(y0+y)*MW+x0+x]=LOT; }
  else {
    const q=rnd();
    if (q<0.35) addBuilding(x0+1,y0+1,4,4,tall);
    else if (q<0.65){ addBuilding(x0+1,y0+1,2,4,tall); addBuilding(x0+3,y0+1,2,4,tall&&rnd()<0.5); }
    else if (q<0.85){ addBuilding(x0+1,y0+1,4,2,tall); addBuilding(x0+1,y0+3,4,2,false); }
    else { addBuilding(x0+1,y0+1,2,2,false); addBuilding(x0+3,y0+1,2,2,false); addBuilding(x0+1,y0+3,4,2,tall); }
    // palms on the sidewalk ring
    for(let k=0;k<S;k+=2){
      if(rnd()<0.45) addPalm((x0+k+0.5)*TILE,(y0+0.5)*TILE);
      if(rnd()<0.45) addPalm((x0+k+0.5)*TILE,(y0+S-0.5)*TILE);
    }
  }
}
// palms along the bayfront walk and the beach promenade
for(let py=40;py<WORLD_H;py+=110+rnd()*40){
  if(map[Math.floor(py/TILE)*MW+BAY_X]===WALK) addPalm((BAY_X+0.5)*TILE,py);
  addPalm((BEACH_X+0.5)*TILE+(rnd()-.5)*12,py+rnd()*30);
}
// beach: umbrellas and the lifeguard stands
const UMB=[['#e2598b','#f6f1e6'],['#2bb5b0','#f6f1e6'],['#f39a4a','#fff3c4'],['#5b8fd6','#f6f1e6'],['#f3d34a','#e2598b']];
for(let py=90;py<WORLD_H-90;py+=70+rnd()*110){
  const px=(BEACH_X+1.3+rnd()*2.2)*TILE;
  if(Math.abs(py%(BLOCK*TILE)-4.4*TILE)<90) continue;   // keep clear of the lifeguard stands
  scenery.push({t:'umb',x:px,y:py,cx:px,cy:py,r:20+rnd()*6,ht:0.04,cols:pick(UMB),a:rnd()*7,towel:pick(ACCENT)});
}
for(let j=0;j<NB;j++){
  const hut={x:(BEACH_X+2.2)*TILE,y:(j*BLOCK+4)*TILE,w:46,h:46,ht:0.09,col:pick(DECO),acc:pick(ACCENT),vents:0,s:rnd()};
  hut.cx=hut.x+23; hut.cy=hut.y+23; solids.push(hut); scenery.push(hut);
}
// the sea and the bay are solid seawalls: one rect per row run of water tiles
for(let y=0;y<MH;y++){ let x=0;
  while(x<MW){ if(map[y*MW+x]!==WATER){ x++; continue; }
    let e=x; while(e<MW&&map[y*MW+e]===WATER) e++;
    solids.push({x:x*TILE,y:y*TILE,w:(e-x)*TILE,h:TILE}); x=e; }
}
function tileAt(px,py){ const x=Math.floor(px/TILE), y=Math.floor(py/TILE);
  if(x<0||y<0||x>=MW||y>=MH) return ROAD; return map[y*MW+x]; }

// Track layouts and geometry live in js/tracks.js (STAGES, buildTrack)

const GRAVEL=4;
const THEMES={
  grass:  {out:'#4d6a3b',gravel:'#a39a7c',asph:'#3b3e44',barrier:['#eeeae0','#c8352b'],deco:['tree'],cols:['#3f5a30','#476836','#35502b']},
  port:   {out:'#77756e',gravel:'#8f8b80',asph:'#3a3c41',barrier:['#eeeae0','#2f6fb5'],deco:['box'],cols:['#b5452f','#2f6b8f','#c9922c','#4d7d44','#8c8f93'],dense:0.7},
  forest: {out:'#2f4a2a',gravel:'#8a8266',asph:'#383a3f',barrier:['#eeeae0','#c8352b'],deco:['tree'],cols:['#27401f','#2e4b25','#223a1c','#355a2c'],dense:2.2},
  stadium:{out:'#5d6068',gravel:'#8a8a86',asph:'#34363b',barrier:['#eeeae0','#d9a21b'],deco:['tires','box'],cols:['#7a3b3b','#3b5a7a','#6b6f78']},
  desert: {out:'#b48a5c',gravel:'#c9a574',asph:'#46433f',barrier:['#eeeae0','#c8352b'],deco:['rock'],cols:['#8f6a45','#a07a50','#7d5c3c']},
  night:  {out:'#1c2a1f',gravel:'#5b574a',asph:'#2c2e33',barrier:['#d8d4ca','#b3302a'],deco:['tree'],cols:['#15241a','#1a2d1f','#203626'],dense:1.6,night:true},
};
let mode='city', T=null, theme=null, decos=[], run=null, state='menu';
function nearestOn(x,y,i0,i1){
  let bd=1e18, bi=0;
  for(let j=i0;j<=i1;j++){ const i=((j%T.N)+T.N)%T.N, p=T.pts[i], d=(p[0]-x)*(p[0]-x)+(p[1]-y)*(p[1]-y); if(d<bd){ bd=d; bi=i; } }
  const a=T.pts[bi]; let best=Math.sqrt(bd), px=a[0], py=a[1];
  for(const q of [T.pts[(bi+1)%T.N],T.pts[(bi-1+T.N)%T.N]]){
    const dx=q[0]-a[0], dy=q[1]-a[1], u=Math.max(0,Math.min(1,((x-a[0])*dx+(y-a[1])*dy)/(dx*dx+dy*dy)));
    const qx=a[0]+dx*u, qy=a[1]+dy*u, d=Math.hypot(x-qx,y-qy);
    if(d<best){ best=d; px=qx; py=qy; }
  }
  return {i:bi,d:best,px,py};
}
const nearestAll=(x,y)=>nearestOn(x,y,0,T.N-1);
function surfAt(x,y){ if(mode==='city') return tileAt(x,y); return nearestAll(x,y).d<T.half+7?ROAD:GRAVEL; }
function collideTrack(cx,cy,r){
  const n=nearestAll(cx,cy); if(n.d+r<=T.edge) return null;
  const d=n.d||0.001; return {nx:(n.px-cx)/d, ny:(n.py-cy)/d, pen:n.d+r-T.edge};
}
function makeDeco(x,y){
  const kind=theme.deco[Math.floor(rnd()*theme.deco.length)], col=theme.cols[Math.floor(rnd()*theme.cols.length)];
  if(kind==='box'){ const horiz=rnd()<0.5, w=horiz?130:52, h=horiz?52:130;
    return {t:'box',x:x-w/2,y:y-h/2,w,h,ht:0.05+rnd()*0.09,col,vents:0,s:rnd(),cx:x,cy:y,clear:110}; }
  if(kind==='rock') return {t:'rock',x,y,r:18+rnd()*34,ht:0.03+rnd()*0.03,col,clear:70};
  if(kind==='tires') return {t:'tires',x,y,r:15,ht:0.05,col,clear:40};
  return {t:'tree',x,y,r:24+rnd()*24,ht:0.05+rnd()*0.06,col,clear:60};
}
function loadStage(i){
  const def=STAGES[i]; mode='track'; T=buildTrack(def); theme=THEMES[def.theme];
  T.path=new Path2D(); T.pts.forEach((p,j)=>j?T.path.lineTo(p[0],p[1]):T.path.moveTo(p[0],p[1])); T.path.closePath();
  T.zones.forEach(z=>{ z.path=new Path2D(); for(let q=z.a;q<=z.b;q++){ const p=T.pts[q]; q===z.a?z.path.moveTo(p[0],p[1]):z.path.lineTo(p[0],p[1]); } });
  let per=0; for(const z of T.zones) per+=z.len*0.12+250+z.clips.length*150;
  T.pass=Math.round(per*def.laps*(1+i*0.06)/100)*100;
  T.limit=Math.round(def.laps*T.L/def.v);
  T.clipR=52-i*3;
  seed=4242+i*977; decos=[];
  const [x0,y0,x1,y1]=T.bounds, m=500, count=Math.round((x1-x0+2*m)*(y1-y0+2*m)/60000*(theme.dense||1));
  for(let k=0;k<count;k++){
    const x=x0-m+rnd()*(x1-x0+2*m), y=y0-m+rnd()*(y1-y0+2*m), d=makeDeco(x,y);
    if(nearestAll(x,y).d>T.edge+d.clear) decos.push(d);
  }
  buildMini();
}

// ---------- car ----------
const START={x:3*BLOCK*TILE+TILE, y:3*BLOCK*TILE+TILE*5, a:-Math.PI/2};
const car={x:0,y:0,a:0,vx:0,vy:0,w:0};
function resetCar(){
  if(mode==='track'){ const i=run?run.pi:3, p=T.pts[i], t=T.tan[i]; Object.assign(car,{x:p[0],y:p[1],a:Math.atan2(t[1],t[0]),vx:0,vy:0,w:0}); }
  else Object.assign(car,{x:START.x,y:START.y,a:START.a,vx:0,vy:0,w:0});
  prevWheels=null;
}

const keys={up:false,down:false,left:false,right:false,hand:false};
const KMAP={ArrowUp:'up',KeyW:'up',ArrowDown:'down',KeyS:'down',ArrowLeft:'left',KeyA:'left',ArrowRight:'right',KeyD:'right',Space:'hand'};
addEventListener('keydown',e=>{ if(KMAP[e.code]){keys[KMAP[e.code]]=true; e.preventDefault();} if(e.code==='KeyR' && (state==='race'||state==='free')){ wreck(); resetCar(); } if(e.code==='Escape' && state!=='menu') showMenu(); });
addEventListener('keyup',e=>{ if(KMAP[e.code]){keys[KMAP[e.code]]=false; e.preventDefault();} });
document.querySelectorAll('.touch button').forEach(b=>{
  const k=b.dataset.k;
  const on=e=>{e.preventDefault(); keys[k]=true; b.classList.add('on');};
  const off=e=>{e.preventDefault(); keys[k]=false; b.classList.remove('on');};
  b.addEventListener('pointerdown',on); b.addEventListener('pointerup',off);
  b.addEventListener('pointercancel',off); b.addEventListener('pointerleave',off);
});
// analog steering slider for touch screens: drag left or right, springs back to centre on release
let touchSteer=0, steerPid=null;
const steerEl=document.getElementById('steer'), knobEl=document.getElementById('knob');
function setSteer(v){
  touchSteer=v;
  const travel=steerEl.clientWidth/2-32;
  knobEl.style.transform='translateX('+(v*travel)+'px)';
  steerEl.setAttribute('aria-valuenow',Math.round(v*100));
}
function steerFromPointer(e){
  const r=steerEl.getBoundingClientRect(), travel=r.width/2-32;
  setSteer(Math.max(-1,Math.min(1,(e.clientX-(r.left+r.width/2))/travel)));
}
steerEl.addEventListener('pointerdown',e=>{ e.preventDefault(); steerPid=e.pointerId; try{ steerEl.setPointerCapture(e.pointerId); }catch(_){} steerEl.classList.add('dragging'); steerFromPointer(e); });
steerEl.addEventListener('pointermove',e=>{ if(e.pointerId===steerPid) steerFromPointer(e); });
const steerEnd=e=>{ if(e.pointerId!==steerPid) return; steerPid=null; steerEl.classList.remove('dragging'); setSteer(0); };
steerEl.addEventListener('pointerup',steerEnd); steerEl.addEventListener('pointercancel',steerEnd);
function steerInput(){
  const digital=(keys.left?-1:0)+(keys.right?1:0);
  if(digital) return digital;
  const a=Math.abs(touchSteer), dz=0.06;
  if(a<dz) return 0;
  return Math.sign(touchSteer)*Math.pow((a-dz)/(1-dz),1.4);   // finer control near the centre
}
addEventListener('blur',()=>{ for(const k in keys) keys[k]=false; setSteer(0); });

// ---------- effects ----------
const skids=[]; const MAX_SKIDS=3000; let prevWheels=null;
const smoke=[];
let shake=0;

// ---------- sound: real Supra recording, played back with granular synthesis ----------
// The recording is a dyno pull. For any rpm, we find the moment in the recording where the engine
// was at that pitch and keep replaying short overlapping slices from there, nudging their speed to match.
let AC=null, master, comp, noiseBuf, skidFilt, skidGain;
let engBuf=null, engBus, layerOn, layerOff, nextGrain=0, hann;
let muted=false, vol=6;
try{ muted=localStorage.getItem('sc_muted')==='1'; const v=localStorage.getItem('sc_vol'); if(v!==null&&v!=='') vol=Math.max(0,Math.min(10,+v)); }catch(e){}
const aIn={speed:0,slip:0,surf:0,throttle:0};
const GEAR_TOP=[150,245,340,440,545,660];
const IDLE=1100, REDLINE=7000, LIMIT=7200;
const es={rpm:IDLE,gear:0,cut:0,boost:0,load:0,prevThr:0,limT:0};
const masterLevel=()=>muted?0:Math.pow(vol/10,1.6);
const ENG={"on":[[1.971,101.0],[2.071,103.0],[2.171,105.0],[2.271,110.0],[2.371,119.0],[2.471,121.0],[2.571,122.0],[2.871,130.0],[2.971,131.0],[3.071,133.0],[3.171,140.0],[3.271,152.0],[3.371,153.0],[3.471,159.0],[11.371,163.0],[11.471,163.0],[11.571,163.0],[11.671,163.0],[11.771,163.0],[11.871,163.0],[11.971,163.0],[12.071,163.0],[12.171,163.0],[12.271,163.0],[12.371,164.0],[12.471,164.0],[12.571,164.0],[12.671,165.0],[12.771,165.0],[12.871,165.0],[12.971,165.0],[13.071,165.0],[13.171,166.0],[13.271,166.0],[13.371,167.0],[13.471,167.0],[13.571,167.0],[13.671,167.0],[13.771,168.0],[13.871,168.0],[13.971,168.0],[14.071,169.0],[14.171,169.0],[14.271,169.0],[14.371,170.0],[14.471,170.0],[14.571,170.0],[14.671,171.0],[14.771,171.0],[14.871,171.0],[14.971,172.0],[15.071,172.0],[15.171,172.0],[15.271,172.0],[15.371,172.0],[15.471,173.0],[15.571,174.0],[15.671,174.0],[15.771,174.0],[15.871,174.0],[15.971,174.0],[16.071,175.0],[16.171,175.0],[16.271,175.0],[16.371,175.0],[16.471,175.0],[16.571,176.0],[16.671,176.0],[16.771,177.0],[16.871,177.0],[16.971,177.0],[17.071,177.0],[17.171,177.0],[17.271,178.0],[17.371,178.0],[17.471,178.0],[17.571,178.0],[17.671,178.0],[17.771,179.0],[17.871,180.0],[17.971,180.0],[18.071,180.0],[18.171,180.0],[18.271,181.0],[18.371,181.0],[18.471,182.0],[18.571,183.0],[18.671,184.0],[18.771,184.0],[18.871,185.0],[18.971,185.0],[19.071,187.0],[19.171,187.0],[19.271,188.0],[19.371,189.0],[19.471,191.0],[19.571,193.0],[19.671,195.0],[19.771,195.0],[19.871,198.0],[19.971,199.0],[20.071,201.0],[20.171,204.0],[20.271,206.0],[20.371,208.0],[20.471,209.0],[20.571,212.0],[20.671,214.0],[20.771,215.0],[20.871,217.0],[20.971,218.0],[21.071,224.0],[21.171,224.0],[21.271,229.0],[21.371,231.0],[21.471,234.0],[21.571,237.0],[21.671,239.0],[21.771,245.0],[21.871,246.0],[21.971,250.0],[22.071,251.0],[22.171,257.0],[22.271,261.0],[22.371,267.0],[22.471,269.0],[22.571,279.0],[22.671,281.0],[22.771,285.0],[22.971,308.0],[23.071,308.0],[23.171,317.0],[23.271,318.0],[23.371,326.0],[23.471,329.0],[23.571,339.0],[23.671,341.0],[23.771,349.0],[23.871,352.0],[23.971,356.0],[24.071,362.0],[24.171,371.0],[24.271,372.0],[24.371,375.0],[24.471,381.0],[24.571,387.0],[24.671,390.0],[24.771,395.0]],"off":[[26.271,301.0],[26.371,299.0],[26.471,297.0],[26.571,285.0],[26.671,267.0],[26.771,264.0],[26.871,250.0],[26.971,246.0],[27.071,246.0],[27.171,226.0],[27.271,217.0],[27.371,202.0],[27.471,202.0],[27.571,192.0],[28.171,144.0],[28.271,141.0],[28.471,131.0],[28.571,121.0],[28.671,113.0],[28.771,108.0],[28.871,107.0],[28.971,93.0],[29.071,93.0],[29.171,93.0],[29.271,90.0],[29.371,89.0],[29.471,87.0],[29.571,87.0],[29.671,86.0],[29.771,85.0],[29.871,85.0],[29.971,81.0],[30.071,80.0],[30.171,80.0],[30.271,78.0],[30.371,78.0],[30.471,78.0],[30.571,78.0],[30.671,76.0],[30.771,75.0],[30.871,72.0],[30.971,72.0],[31.071,72.0]]};                    // [seconds into recording, engine pitch in Hz]
const IDLE_SPAN=[31.75,33.4], IDLE_F=65; // a stretch of steady idle near the end of the clip
const GRAIN=0.11, HOP=GRAIN/3;
const rpmToHz=r=>r/18;

function initAudio(){
  if(AC){ if(AC.state==='suspended') AC.resume(); return; }
  try{ AC=new (window.AudioContext||window.webkitAudioContext)(); }catch(e){ return; }
  const G=v=>{ const g=AC.createGain(); g.gain.value=v; return g; };
  master=G(masterLevel()); master.connect(AC.destination);
  comp=AC.createDynamicsCompressor(); comp.threshold.value=-16; comp.ratio.value=4; comp.connect(master);
  noiseBuf=AC.createBuffer(1,AC.sampleRate*2,AC.sampleRate);
  const d=noiseBuf.getChannelData(0); for(let i=0;i<d.length;i++) d[i]=Math.random()*2-1;
  engBus=G(1); engBus.connect(comp);
  layerOn=G(0); layerOff=G(0); layerOn.connect(engBus); layerOff.connect(engBus);
  hann=new Float32Array(64); for(let i=0;i<64;i++) hann[i]=Math.pow(Math.sin(Math.PI*i/63),2)/1.5;
  fetch('assets/supra-engine.mp3')
    .then(r=>r.arrayBuffer())
    .then(ab=>new Promise((res,rej)=>{ const pr=AC.decodeAudioData(ab,res,rej); if(pr&&pr.then) pr.then(res,rej); }))
    .then(b=>{ engBuf=b; })
    .catch(e=>console.warn('Engine sound could not load. Run the game from a local server.',e));
  // tyres
  skidFilt=AC.createBiquadFilter(); skidFilt.type='bandpass'; skidFilt.frequency.value=1700; skidFilt.Q.value=7;
  skidGain=G(0);
  const n=AC.createBufferSource(); n.buffer=noiseBuf; n.loop=true;
  n.connect(skidFilt).connect(skidGain).connect(comp); n.start();
}

function lookup(tab,hz){
  for(let i=0;i<tab.length-1;i++){
    const f0=tab[i][1], f1=tab[i+1][1], lo=Math.min(f0,f1), hi=Math.max(f0,f1);
    if(hz>=lo && hz<=hi && hi>lo){ const k=(hz-f0)/(f1-f0); return [tab[i][0]+k*(tab[i+1][0]-tab[i][0]), hz]; }
  }
  let best=tab[0], bd=1e9;
  for(const p of tab){ const dd=Math.abs(p[1]-hz); if(dd<bd){ bd=dd; best=p; } }
  return best;
}
function grain(tab,hz,dest,t,isOff){
  let pos, rec;
  if(isOff && hz<70){ pos=IDLE_SPAN[0]+Math.random()*(IDLE_SPAN[1]-IDLE_SPAN[0]); rec=IDLE_F; }
  else { const p=lookup(tab,hz); pos=p[0]; rec=p[1]; }
  const rate=Math.max(0.5,Math.min(2,hz/rec));
  const src=AC.createBufferSource(); src.buffer=engBuf; src.playbackRate.value=rate;
  const g=AC.createGain(); g.gain.value=0; g.gain.setValueCurveAtTime(hann,t,GRAIN);
  src.connect(g).connect(dest);
  src.start(t,Math.max(0,pos-GRAIN*rate/2+(Math.random()-.5)*0.02));
  src.stop(t+GRAIN+0.01);
}
function scheduleGrains(){
  if(!engBuf) return;
  const now=AC.currentTime;
  if(nextGrain<now) nextGrain=now+0.01;
  const hz=rpmToHz(es.rpm), on=es.load, off=1-es.load;
  while(nextGrain<now+0.08){
    if(on>0.01) grain(ENG.on,hz,layerOn,nextGrain,false);
    if(off>0.01) grain(ENG.off,hz,layerOff,nextGrain,true);
    nextGrain+=HOP;
  }
}

function updateAudio(dt){
  const s=aIn.speed; let thr=aIn.throttle;
  if(es.cut>0){ es.cut-=dt; thr=0; }
  let wr=s/GEAR_TOP[es.gear]*REDLINE;
  if(wr>6750 && es.gear<5 && aIn.throttle){ es.gear++; es.cut=0.16; if(Math.random()<0.6) crackle(); }
  else if(es.gear>0 && wr<2600 && s/GEAR_TOP[es.gear-1]*REDLINE<6000) es.gear--;
  wr=s/GEAR_TOP[es.gear]*REDLINE;
  let target=Math.max(IDLE,wr);
  if(thr && s<60) target=Math.max(target,4300);
  if(thr && aIn.slip>150) target+=1000*Math.min(1,(aIn.slip-150)/200);
  target=Math.min(LIMIT,target);
  es.rpm+=(target-es.rpm)*Math.min(1,(target>es.rpm?9:5)*dt);
  es.load+=(thr-es.load)*Math.min(1,10*dt);
  if(!AC) return;

  const t=AC.currentTime;
  let lim=1;
  if(thr && es.rpm>6950){ es.limT+=dt; lim=(Math.floor(es.limT*24)%2)?0.3:1; } else es.limT=0;
  layerOn.gain.setTargetAtTime(es.load*0.9,t,0.03);
  layerOff.gain.setTargetAtTime((1-es.load)*0.75,t,0.03);
  engBus.gain.setTargetAtTime(lim,t,0.01);
  scheduleGrains();

  // blow-off valve and pops still come from the synth, layered on top of the recording
  const bt=thr?Math.min(1,Math.max(0,(es.rpm-2500)/3000)):0;
  es.boost+=(bt-es.boost)*Math.min(1,(bt>es.boost?1.4:5)*dt);
  if(es.prevThr && !thr && es.boost>0.3) blowOff(es.boost);
  es.prevThr=thr;
  if(!aIn.throttle && es.rpm>3400 && Math.random()<dt*5) crackle();

  const onGrass=aIn.surf===GRASS;
  let sk=Math.min(1,Math.max(0,(aIn.slip-90)/280)); if(onGrass) sk*=0.25;
  skidGain.gain.setTargetAtTime(sk*0.26,t,0.05);
  skidFilt.frequency.setTargetAtTime((onGrass?500:1500)+aIn.slip*0.9+Math.sin(t*37)*120,t,0.03);
}

function burst(t,dur,type,f0,f1,q,vol){
  const n=AC.createBufferSource(); n.buffer=noiseBuf;
  const f=AC.createBiquadFilter(); f.type=type; f.Q.value=q;
  f.frequency.setValueAtTime(f0,t); f.frequency.exponentialRampToValueAtTime(f1,t+dur);
  const g=AC.createGain(); g.gain.setValueAtTime(vol,t); g.gain.exponentialRampToValueAtTime(0.0008,t+dur);
  n.connect(f).connect(g).connect(comp); n.start(t,Math.random()*1.5); n.stop(t+dur+0.02);
}
function blowOff(b){ if(!AC) return; const t=AC.currentTime;
  burst(t,0.45,'bandpass',3200,700,1.4,0.35*b);
  for(let i=0;i<5;i++) burst(t+0.05+i*0.045,0.04,'bandpass',1800-i*200,900,3,0.12*b); } // flutter
function crackle(){ if(!AC) return; const t=AC.currentTime, k=1+Math.floor(Math.random()*3);
  for(let i=0;i<k;i++) burst(t+i*0.035+Math.random()*0.02,0.05,'highpass',700,400,0.7,0.25+Math.random()*0.25); }

function sfxCrash(power){
  if(!AC||muted) return;
  const t=AC.currentTime, v=Math.min(1,power);
  burst(t,0.35,'lowpass',900,300,0.7,0.7*v);
  const o=AC.createOscillator(); o.type='sine';
  o.frequency.setValueAtTime(110,t); o.frequency.exponentialRampToValueAtTime(38,t+0.25);
  const og=AC.createGain(); og.gain.setValueAtTime(0.8*v,t); og.gain.exponentialRampToValueAtTime(0.001,t+0.3);
  o.connect(og).connect(master); o.start(t); o.stop(t+0.32);
}
function sfxNotes(freqs,type,step,v){
  if(!AC||muted) return;
  const t0=AC.currentTime;
  freqs.forEach((fr,i)=>{
    const t=t0+i*step, o=AC.createOscillator(), g=AC.createGain();
    o.type=type; o.frequency.value=fr;
    g.gain.setValueAtTime(0.0001,t); g.gain.exponentialRampToValueAtTime(v,t+0.01);
    g.gain.exponentialRampToValueAtTime(0.0001,t+step*1.8);
    o.connect(g).connect(master); o.start(t); o.stop(t+step*2);
  });
}
const sfxBank=size=>sfxNotes(size>1500?[523,659,784,1047]:size>500?[523,659,784]:[587,784],'square',0.08,0.1);
const sfxLost=()=>sfxNotes([330,247,185],'sawtooth',0.09,0.08);

// volume and mute controls
const muteBtn=document.getElementById('mute'), volEl=document.getElementById('vol');
function drawVolMeter(){   // touch settings panel: 10-step meter under the volume label
  volEl.innerHTML='<span>Volume '+vol+'</span><span class="vol-meter" aria-hidden="true">'+'<i></i>'.repeat(10)+'</span>';
  volEl.querySelectorAll('.vol-meter i').forEach((el,k)=>el.classList.toggle('on',k<vol));
}
function applyLevel(){ if(master) master.gain.setTargetAtTime(masterLevel(),AC.currentTime,0.03); }
function setMuted(m){
  muted=m; try{ localStorage.setItem('sc_muted',m?'1':'0'); }catch(e){}
  muteBtn.textContent=m?'Sound off':'Sound on'; muteBtn.setAttribute('aria-pressed',String(m)); applyLevel();
}
function setVol(v){
  vol=Math.max(0,Math.min(10,v)); try{ localStorage.setItem('sc_vol',vol); }catch(e){}
  volEl.textContent='Volume '+vol; if(isTouch) drawVolMeter();
  if(muted && vol>0) setMuted(false); else applyLevel();
}
muteBtn.addEventListener('click',()=>{ initAudio(); setMuted(!muted); muteBtn.blur(); });
document.getElementById('volup').addEventListener('click',e=>{ initAudio(); setVol(vol+1); e.currentTarget.blur(); });
document.getElementById('voldown').addEventListener('click',e=>{ initAudio(); setVol(vol-1); e.currentTarget.blur(); });
addEventListener('keydown',e=>{
  if(e.code==='KeyM'){ initAudio(); setMuted(!muted); }
  if(e.code==='Equal'||e.code==='NumpadAdd'){ initAudio(); setVol(vol+1); }
  if(e.code==='Minus'||e.code==='NumpadSubtract'){ initAudio(); setVol(vol-1); }
});
volEl.textContent='Volume '+vol; setMuted(muted);
if(isTouch) drawVolMeter();
addEventListener('blur',()=>{ if(AC) AC.suspend(); });
addEventListener('focus',()=>{ if(AC && running) AC.resume(); });

// ---------- scoring ----------
let score=0, best=0;
try{ best=+localStorage.getItem('sc_best')||0; }catch(e){}
const chain={active:false,pts:0,time:0,mult:1,grace:0};
const $=id=>document.getElementById(id);
const comboEl=$('combo'), toastEl=$('toast');
let toastT=0;
function toast(t,color){ toastEl.textContent=t; toastEl.style.color=color; toastEl.style.opacity=1; toastT=1.3; }
function bank(){
  const p=Math.round(chain.pts);
  if(p>30){ score+=p; toast('+'+p.toLocaleString(),'var(--accent)'); sfxBank(p);
    if(mode==='city' && p>best){ best=p; try{localStorage.setItem('sc_best',best);}catch(e){} } }
  chain.active=false; chain.pts=0; chain.time=0; chain.mult=1;
}
function wreck(){
  if(chain.active && chain.pts>30){ toast('Chain lost','var(--bad)'); sfxLost(); }
  chain.active=false; chain.pts=0; chain.time=0; chain.mult=1;
  if(mode==='track' && run && state==='race'){
    score=Math.max(0,score-100); msg('Wall  \u2212100','var(--bad)');
    if(run.zi>=0) run.zs[run.zi].dirty=true;
  }
}
const zmsgEl=$('zmsg'); let zmsgT=0;
function msg(t,color){ zmsgEl.textContent=t; zmsgEl.style.color=color; zmsgEl.style.opacity=1; zmsgT=1.4; }

// ---------- stage logic: laps, drift zones, clipping points ----------
function newLapZones(){ run.zs=T.zones.map(()=>({dist:0,drift:0,dirty:false,done:false})); run.clipHit=new Set(); }
function judgeZone(k){
  const s=run.zs[k], z=T.zones[k]; if(s.done) return; s.done=true;
  if(s.dist<z.len*0.5) return;
  if(!s.dirty && s.drift/s.dist>=0.75){ score+=500; msg('Clean zone  +500','var(--accent)'); sfxNotes([659,988],'triangle',0.07,0.12); }
  else msg(s.dirty?'Zone failed':'Not sideways enough','var(--bad)');
}
function trackTick(dt,drifting,surf){
  const prev=run.pi;
  let n=nearestOn(car.x,car.y,prev-30,prev+30);
  if(n.d>T.edge+40) n=nearestAll(car.x,car.y);
  run.pi=n.i;
  const q=Math.floor(run.pi/(T.N/4)); if(q>=1&&q<=3) run.flags|=1<<q;
  const t=T.tan[run.pi], along=car.vx*t[0]+car.vy*t[1], sp=Math.hypot(car.vx,car.vy);
  run.wrongT=(sp>120 && along<-0.5*sp)?run.wrongT+dt:0;
  let zi=-1; T.zones.forEach((z,k)=>{ if(run.pi>=z.a&&run.pi<=z.b) zi=k; });
  if(run.zi>=0 && zi!==run.zi && run.pi>T.zones[run.zi].b) judgeZone(run.zi);
  run.zi=zi;
  if(zi>=0 && !run.zs[zi].done){
    const s=run.zs[zi], ds=Math.max(0,along)*dt;
    s.dist+=ds; if(drifting) s.drift+=ds; if(surf===GRAVEL) s.dirty=true;
    const rx=car.x-Math.cos(car.a)*16, ry=car.y-Math.sin(car.a)*16;
    for(const c of T.zones[zi].clips){
      if(!run.clipHit.has(c) && drifting && Math.hypot(c.x-rx,c.y-ry)<T.clipR){
        run.clipHit.add(c); const pts=150*chain.mult; chain.pts+=pts;
        msg('Clip  +'+pts,'#7fe08a'); sfxNotes([988,1319],'square',0.06,0.09);
      }
    }
  }
  if(prev>T.N*0.85 && run.pi<T.N*0.15 && run.flags===14){
    run.lap++; run.flags=0; newLapZones();
    if(run.lap>=run.def.laps) finish(true); else { msg('Lap '+(run.lap+1)+' of '+run.def.laps,'var(--ink)'); sfxNotes([784],'triangle',0.1,0.1); }
  }
}

// ---------- collision ----------
function collideCircle(cx,cy,r){
  if(mode==='track') return collideTrack(cx,cy,r);
  let hit=null;
  for(const b of solids){
    if(cx+r<b.x||cx-r>b.x+b.w||cy+r<b.y||cy-r>b.y+b.h) continue;
    const px=Math.max(b.x,Math.min(cx,b.x+b.w)), py=Math.max(b.y,Math.min(cy,b.y+b.h));
    let dx=cx-px, dy=cy-py, d=Math.hypot(dx,dy);
    if(d<r){
      if(d<0.001){ dx=0; dy=-1; d=1; }
      const nx=dx/d, ny=dy/d, pen=r-d;
      hit={nx,ny,pen}; break;
    }
  }
  if(!hit){
    if(cx<r) hit={nx:1,ny:0,pen:r-cx};
    else if(cx>WORLD_W-r) hit={nx:-1,ny:0,pen:cx-(WORLD_W-r)};
    else if(cy<r) hit={nx:0,ny:1,pen:r-cy};
    else if(cy>WORLD_H-r) hit={nx:0,ny:-1,pen:cy-(WORLD_H-r)};
  }
  return hit;
}

// ---------- update ----------
const GRIP={[ROAD]:1,[WALK]:0.9,[GRASS]:0.55,[LOT]:1,[GRAVEL]:0.5,[SAND]:0.5,[WATER]:0.5};
const soft=s=>s===GRASS||s===GRAVEL||s===SAND;
function update(dt){
  const fx=Math.cos(car.a), fy=Math.sin(car.a), rx=-fy, ry=fx;
  let vf=car.vx*fx+car.vy*fy, vr=car.vx*rx+car.vy*ry;
  const surf=surfAt(car.x,car.y), g=GRIP[surf];

  if(keys.up) vf += (vf<0?1100:540)*dt*(0.6+0.4*g);
  if(keys.down) vf -= (vf>20?950:320)*dt;
  vf = Math.max(-220, Math.min(640, vf));
  vf -= vf*(0.5 + (soft(surf)?1.2:0))*dt;
  if(keys.hand) vf -= Math.sign(vf)*Math.min(Math.abs(vf),170*dt);
  if(!keys.up && !keys.down && Math.abs(vf)<8) vf=0;

  let grip = keys.hand ? 1.0 : (keys.up && vf>260 ? 3.0 : 7.5);
  grip*=g; vr *= Math.exp(-grip*dt);

  const speed=Math.hypot(vf,vr);
  const steer=steerInput();
  const target = steer*2.8*Math.min(1,Math.abs(vf)/150)*(vf>=0?1:-1)*(keys.hand?1.4:1);
  car.w += (target-car.w)*Math.min(1,9*dt);
  car.a += car.w*dt;

  car.vx=fx*vf+rx*vr; car.vy=fy*vf+ry*vr;
  car.x+=car.vx*dt; car.y+=car.vy*dt;

  // collisions: two circles along the body
  for(const off of [13,-13]){
    const nfx=Math.cos(car.a), nfy=Math.sin(car.a);
    const h=collideCircle(car.x+nfx*off, car.y+nfy*off, 12);
    if(h){
      car.x+=h.nx*h.pen; car.y+=h.ny*h.pen;
      const vn=car.vx*h.nx+car.vy*h.ny;
      if(vn<0){
        car.vx-=1.35*vn*h.nx; car.vy-=1.35*vn*h.ny;
        car.vx*=0.8; car.vy*=0.8; car.w*=0.5;
        if(-vn>170){ shake=Math.min(14,-vn/25); sfxCrash(-vn/500); wreck(); }
        else if(-vn>60) sfxCrash(-vn/900);
      }
    }
  }

  // drift detection
  const ang=Math.atan2(Math.abs(vr),Math.abs(vf));
  const drifting = speed>170 && ang>0.26 && ang<1.95 && vf>-40;
  if(drifting){
    if(!chain.active){ chain.active=true; chain.pts=0; chain.time=0; }
    chain.time+=dt; chain.mult=Math.min(8,1+Math.floor(chain.time/1.5));
    chain.pts += ang*speed*dt*0.1*chain.mult*((mode==='track' && run && run.zi<0)?0.25:1);
    chain.grace=0.7;
  } else if(chain.active){ chain.grace-=dt; if(chain.grace<=0) bank(); }
  if(mode==='track' && state==='race') trackTick(dt,drifting,surf);

  // skids and smoke
  const skidding = Math.abs(vr)>110 || (keys.hand && speed>90) || (keys.up && Math.abs(vf)<120 && Math.abs(vf)>5 && !soft(surf));
  const nfx=Math.cos(car.a), nfy=Math.sin(car.a);
  const bx=car.x-nfx*14, by=car.y-nfy*14;
  const wl={x:bx-nfy*9, y:by+nfx*9}, wr={x:bx+nfy*9, y:by-nfx*9};
  if(skidding && !soft(surf)){
    if(prevWheels){
      skids.push([prevWheels[0].x,prevWheels[0].y,wl.x,wl.y],[prevWheels[1].x,prevWheels[1].y,wr.x,wr.y]);
      if(skids.length>MAX_SKIDS) skids.splice(0,skids.length-MAX_SKIDS);
    }
    prevWheels=[wl,wr];
    if(Math.random()<0.6) smoke.push({x:(Math.random()<.5?wl:wr).x,y:(Math.random()<.5?wl:wr).y,
      vx:(Math.random()-.5)*30, vy:(Math.random()-.5)*30, r:6+Math.random()*6, life:1});
  } else prevWheels=null;
  for(let i=smoke.length-1;i>=0;i--){ const s=smoke[i]; s.life-=dt*1.4; s.r+=dt*22; s.x+=s.vx*dt; s.y+=s.vy*dt; if(s.life<=0) smoke.splice(i,1); }

  shake=Math.max(0,shake-dt*30);
  aIn.speed=Math.abs(vf); aIn.slip=Math.abs(vr)+(keys.hand&&speed>90?120:0); aIn.surf=soft(surf)?GRASS:surf; aIn.throttle=keys.up?1:0;
  return speed;
}

// ---------- render ----------
const cam={x:0,y:0,z:1};
const COLORS={[ROAD]:'#3d3f46',[WALK]:'#d9cfbe',[GRASS]:'#5c9a45',[LOT]:'#4a4c53',[SAND]:'#eedba9',[WATER]:'#2c8db0'};
const SEA='#2690b8';

function bodyPath(ox,oy){
  const P=(x,y)=>[x+ox,y+oy];
  ctx.beginPath();
  ctx.moveTo(...P(24,-7));
  ctx.quadraticCurveTo(...P(25.2,0),...P(24,7));
  ctx.quadraticCurveTo(...P(21,10.5),...P(15,10.5));
  ctx.lineTo(...P(-9,10.5));
  ctx.quadraticCurveTo(...P(-14.5,12),...P(-20,11));
  ctx.quadraticCurveTo(...P(-24,10),...P(-23.6,0));
  ctx.quadraticCurveTo(...P(-24,-10),...P(-20,-11));
  ctx.quadraticCurveTo(...P(-14.5,-12),...P(-9,-10.5));
  ctx.lineTo(...P(15,-10.5));
  ctx.quadraticCurveTo(...P(21,-10.5),...P(24,-7));
  ctx.closePath();
}
function poly(pts){ ctx.beginPath(); ctx.moveTo(pts[0][0],pts[0][1]); for(let i=1;i<pts.length;i++) ctx.lineTo(pts[i][0],pts[i][1]); ctx.closePath(); }
// ---------- paint ----------
const PAINTS=[
  ['Candy orange','#f0631a'],['Racing red','#c4221f'],['Sunburst yellow','#f2c21b'],['Lime green','#74bf1e'],
  ['Teal metallic','#1f7392'],['Electric blue','#1f5fd6'],['Midnight black','#1d2026'],['Pearl white','#e9e6df']];
const rgbOf=h=>{ const n=parseInt(h.slice(1),16); return [n>>16,(n>>8)&255,n&255]; };
const mixC=(h,t,k)=>'rgb('+rgbOf(h).map((v,i)=>Math.round(v+(t[i]-v)*k)).join(',')+')';
let paint;
function makePaint(hex){
  const [r,g,b]=rgbOf(hex), lum=(0.299*r+0.587*g+0.114*b)/255, W=[255,255,255], K=[0,0,0];
  return {hex, light:mixC(hex,W,.28), base:hex, dark:mixC(hex,K,.38), roof:mixC(hex,W,.1),
    trim:mixC(hex,K,.22), vent:mixC(hex,K,.6), stripe: lum>0.62?'#2a2d33':'#f1ecdf'};
}
function setPaint(hex){ paint=makePaint(hex); }

// ---------- ghost: replay of the best completed run on each stage ----------
const GHOST_DT=0.05, ghostPaint=makePaint('#bfe9ff');
const loadGhost=i=>{ try{ return JSON.parse(localStorage.getItem('sc_ghost_'+i)||'null'); }catch(e){ return null; } };
const saveGhost=(i,g)=>{ try{ localStorage.setItem('sc_ghost_'+i,JSON.stringify(g)); return true; }catch(e){ return false; } };
function recordGhost(){   // samples are flat [x, y, angle*1000, ...] every GHOST_DT seconds of race time
  const r=run.rec; while(r.length/3*GHOST_DT<=run.t) r.push(Math.round(car.x),Math.round(car.y),Math.round(car.a*1000));
}
function ghostAt(t){
  const g=run&&run.ghost; if(!g) return null;
  const d=g.d, n=d.length/3, f=Math.max(0,t)/GHOST_DT, k=Math.floor(f);
  if(k>=n-1) return null;
  const u=f-k, j=k*3, a0=d[j+2]/1000; let da=d[j+5]/1000-a0;
  da=Math.atan2(Math.sin(da),Math.cos(da));
  return {x:d[j]+(d[j+3]-d[j])*u, y:d[j+1]+(d[j+4]-d[j+1])*u, a:a0+da*u};
}
function drawGhost(){
  const g=ghostAt(state==='count'?0:run.t); if(!g) return;
  const own=paint; paint=ghostPaint; ctx.save(); ctx.globalAlpha=0.42;
  drawCar(g.x,g.y,g.a,0,false);
  ctx.restore(); paint=own;
}
function drawCar(x,y,a,w,braking){
  ctx.save(); ctx.translate(x,y); ctx.rotate(a);
  ctx.fillStyle='rgba(0,0,0,.35)'; bodyPath(2,3); ctx.fill();
  // tyres peek out from under the wide arches; fronts turn with the steering
  ctx.fillStyle='#141414';
  ctx.fillRect(-18,-13,10,3); ctx.fillRect(-18,10,10,3);
  ctx.save(); ctx.translate(12.5,0); ctx.rotate(w*0.14);
  ctx.fillRect(-4.5,-12.5,9,3); ctx.fillRect(-4.5,9.5,9,3); ctx.restore();
  // metallic paint
  const g=ctx.createLinearGradient(0,-12,0,12);
  g.addColorStop(0,paint.light); g.addColorStop(.45,paint.base); g.addColorStop(1,paint.dark);
  ctx.fillStyle=g; bodyPath(0,0); ctx.fill();
  ctx.strokeStyle='rgba(0,0,0,.35)'; ctx.lineWidth=0.8; ctx.stroke();
  // mirrors
  ctx.fillStyle=paint.trim; ctx.fillRect(4,-12.6,3,2.4); ctx.fillRect(4,10.2,3,2.4);
  // glass: wraparound windshield, side windows, rear hatch
  ctx.fillStyle='#161b22';
  poly([[11.5,-7.2],[11.5,7.2],[3,8.8],[3,-8.8]]); ctx.fill();
  poly([[3,-8.8],[-9,-8.4],[-13.5,-6.8],[-13.5,6.8],[-9,8.4],[3,8.8]]); ctx.fill();
  // roof panel over the glass
  ctx.fillStyle=paint.roof; roundRect(-8.5,-7.2,11,14.4,2); ctx.fill();
  ctx.fillStyle='rgba(255,255,255,.18)'; ctx.fillRect(9,-6,1.5,5);   // windshield glint
  // twin stripes on hood, roof and deck
  ctx.fillStyle=paint.stripe;
  for(const [x0,x1] of [[11.5,24.4],[-8.5,2.5],[-23.2,-13.5]]){ ctx.fillRect(x0,-3.4,x1-x0,2); ctx.fillRect(x0,1.4,x1-x0,2); }
  // hood vents
  ctx.fillStyle=paint.vent; ctx.fillRect(15,-7,5,1.4); ctx.fillRect(15,5.6,5,1.4);
  // slim headlights
  ctx.fillStyle='#fff3cc'; ctx.fillRect(22.3,-8,2,3.2); ctx.fillRect(22.3,4.8,2,3.2);
  // low lip spoiler on the tail
  ctx.fillStyle='#0e1215'; ctx.fillRect(-22.8,-9.5,1.6,19);
  // tail lights
  ctx.fillStyle=braking?'#ff3b2f':'#8a1f1a';
  ctx.fillRect(-24,-9,1.3,4.5); ctx.fillRect(-24,4.5,1.3,4.5);
  ctx.restore();
}
function roundRect(x,y,w,h,r){ ctx.beginPath(); ctx.moveTo(x+r,y); ctx.arcTo(x+w,y,x+w,y+h,r); ctx.arcTo(x+w,y+h,x,y+h,r); ctx.arcTo(x,y+h,x,y,r); ctx.arcTo(x,y,x+w,y,r); ctx.closePath(); }
function shade(hex,f){ const n=parseInt(hex.slice(1),16); const c=[n>>16,(n>>8)&255,n&255].map(v=>Math.round(v*f)); return `rgb(${c[0]},${c[1]},${c[2]})`; }

function render(speed){
  ctx.setTransform(DPR,0,0,DPR,0,0);
  ctx.fillStyle=mode==='track'?theme.out:SEA; ctx.fillRect(0,0,W,H);
  const scaleBase=Math.min(1.15,Math.max(0.75,Math.min(W,H)/620))*(mode==='track'?0.8:1);
  const tz=scaleBase*(1.1-Math.min(0.38,speed/1500));
  cam.z+=(tz-cam.z)*0.04;
  const tx=car.x+car.vx*0.35, ty=car.y+car.vy*0.35;
  cam.x+=(tx-cam.x)*0.1; cam.y+=(ty-cam.y)*0.1;
  const sx=(Math.random()-.5)*shake, sy=(Math.random()-.5)*shake;

  ctx.translate(W/2+sx,H/2+sy); ctx.scale(cam.z,cam.z); ctx.translate(-cam.x,-cam.y);
  const hw=W/2/cam.z+TILE, hh=H/2/cam.z+TILE;
  if(mode==='city') renderCity(hw,hh); else renderTrack(hw,hh);
  if(mode==='track' && theme.night){
    ctx.setTransform(DPR,0,0,DPR,0,0);
    const fx=car.x+Math.cos(car.a)*70, fy=car.y+Math.sin(car.a)*70;
    const sx2=W/2+(fx-cam.x)*cam.z, sy2=H/2+(fy-cam.y)*cam.z;
    const gr=ctx.createRadialGradient(sx2,sy2,70*cam.z,sx2,sy2,480*cam.z);
    gr.addColorStop(0,'rgba(4,6,14,0)'); gr.addColorStop(1,'rgba(4,6,14,0.86)');
    ctx.fillStyle=gr; ctx.fillRect(0,0,W,H);
  }
}

function drawBlock(b){
    const k=b.ht;
    const P=(px,py)=>[px+(px-cam.x)*k, py+(py-cam.y)*k];
    const c=[[b.x,b.y],[b.x+b.w,b.y],[b.x+b.w,b.y+b.h],[b.x,b.y+b.h]];
    const r=c.map(p=>P(p[0],p[1]));
    const wallShades=[0.55,0.72,0.62,0.8];
    for(let i=0;i<4;i++){
      const j=(i+1)%4;
      ctx.fillStyle=shade(b.col,wallShades[i]);
      ctx.beginPath(); ctx.moveTo(c[i][0],c[i][1]); ctx.lineTo(c[j][0],c[j][1]); ctx.lineTo(r[j][0],r[j][1]); ctx.lineTo(r[i][0],r[i][1]); ctx.closePath(); ctx.fill();
    }
    ctx.fillStyle=b.col; ctx.beginPath(); ctx.moveTo(r[0][0],r[0][1]); for(let i=1;i<4;i++) ctx.lineTo(r[i][0],r[i][1]); ctx.closePath(); ctx.fill();
    const rw=r[1][0]-r[0][0], rh=r[3][1]-r[0][1];
    ctx.strokeStyle=b.acc||shade(b.col,0.8); ctx.lineWidth=b.acc?4:3; ctx.strokeRect(r[0][0]+6,r[0][1]+6,rw-12,rh-12);
    if(b.pool){   // rooftop pool with a deck
      const pw=rw*0.5, ph=rh*0.3, px=r[0][0]+rw*(0.15+b.s*0.2), py=r[0][1]+rh*(0.18+((b.s*5)%1)*0.4);
      ctx.fillStyle='#f5efe2'; ctx.fillRect(px-6,py-6,pw+12,ph+12);
      ctx.fillStyle='#4fd0e3'; ctx.fillRect(px,py,pw,ph);
      ctx.fillStyle='rgba(255,255,255,.35)'; ctx.fillRect(px+pw*0.1,py+ph*0.2,pw*0.5,3); ctx.fillRect(px+pw*0.35,py+ph*0.6,pw*0.45,3);
    }
    ctx.fillStyle=shade(b.col,0.7);
    for(let v=0;v<b.vents;v++){ const vx=r[0][0]+rw*(0.2+((b.s*7+v*0.31)%0.6)), vy=r[0][1]+rh*(0.2+((b.s*3+v*0.43)%0.6)); ctx.fillRect(vx,vy,14,14); }
}
function drawPalm(p){
  const ox=p.x+(p.x-cam.x)*p.ht, oy=p.y+(p.y-cam.y)*p.ht;
  ctx.fillStyle='rgba(0,0,0,.12)'; ctx.beginPath(); ctx.ellipse(p.x+7,p.y+9,p.r*0.55,p.r*0.4,0.6,0,7); ctx.fill();
  ctx.strokeStyle='#8a6a45'; ctx.lineWidth=6; ctx.lineCap='round'; ctx.beginPath(); ctx.moveTo(p.x,p.y); ctx.lineTo(ox,oy); ctx.stroke();
  for(let i=0;i<p.n;i++){
    const a=p.a+i*Math.PI*2/p.n, ca=Math.cos(a), sa=Math.sin(a), L=p.r;
    ctx.fillStyle=i%2?p.col:shade(p.col,0.8);
    ctx.beginPath(); ctx.moveTo(ox,oy);
    ctx.quadraticCurveTo(ox+ca*L*.5-sa*L*.3, oy+sa*L*.5+ca*L*.3, ox+ca*L, oy+sa*L);
    ctx.quadraticCurveTo(ox+ca*L*.5+sa*L*.1, oy+sa*L*.5-ca*L*.1, ox, oy);
    ctx.fill();
  }
  ctx.fillStyle='#6b4a2a'; ctx.beginPath(); ctx.arc(ox,oy,4.5,0,7); ctx.fill();
}
function drawUmb(u){
  ctx.save(); ctx.translate(u.x,u.y); ctx.rotate(u.a);
  ctx.fillStyle=u.towel; ctx.fillRect(u.r*0.3,-9,u.r*1.4,18);
  ctx.restore();
  const ox=u.x+(u.x-cam.x)*u.ht, oy=u.y+(u.y-cam.y)*u.ht;
  ctx.fillStyle='rgba(0,0,0,.18)'; ctx.beginPath(); ctx.arc(u.x+6,u.y+8,u.r,0,7); ctx.fill();
  for(let i=0;i<8;i++){
    ctx.fillStyle=u.cols[i%2]; ctx.beginPath(); ctx.moveTo(ox,oy);
    ctx.arc(ox,oy,u.r,u.a+i*Math.PI/4,u.a+(i+1)*Math.PI/4); ctx.closePath(); ctx.fill();
  }
  ctx.fillStyle='#fff'; ctx.beginPath(); ctx.arc(ox,oy,2.5,0,7); ctx.fill();
}
function renderCity(hw,hh){
  const x0=Math.max(0,Math.floor((cam.x-hw)/TILE)), x1=Math.min(MW-1,Math.floor((cam.x+hw)/TILE));
  const y0=Math.max(0,Math.floor((cam.y-hh)/TILE)), y1=Math.min(MH-1,Math.floor((cam.y+hh)/TILE));
  const now=performance.now()/1000;

  for(let y=y0;y<=y1;y++)for(let x=x0;x<=x1;x++){
    const t=map[y*MW+x];
    ctx.fillStyle=t===WATER && x>=SEA_X ? (x===SEA_X?'#3fbfd2':'#2aa3c8') : COLORS[t];
    ctx.fillRect(x*TILE,y*TILE,TILE+1,TILE+1);
    if(t===WALK){ ctx.strokeStyle='rgba(0,0,0,.08)'; ctx.lineWidth=1; ctx.strokeRect(x*TILE+.5,y*TILE+.5,TILE-1,TILE-1); }
    else if(t===SAND && x===SEA_X-1){ ctx.fillStyle='#dcc38e'; ctx.fillRect(x*TILE+TILE*0.55,y*TILE,TILE*0.45+1,TILE+1); }
    else if(t===WATER && (x*5+y*3)%4===0){   // ripples
      const o=Math.sin(now*0.8+x+y*1.7)*8;
      ctx.strokeStyle='rgba(255,255,255,.16)'; ctx.lineWidth=2; ctx.beginPath(); ctx.arc(x*TILE+32+o,y*TILE+40,12,Math.PI*1.15,Math.PI*1.85); ctx.stroke();
    }
    else if(t===ROAD && x<BAY_X){   // causeway railings
      ctx.fillStyle='#efe9dc';
      if(y%BLOCK===0) ctx.fillRect(x*TILE,y*TILE,TILE+1,5);
      if(y%BLOCK===ROADW-1) ctx.fillRect(x*TILE,y*TILE+TILE-5,TILE+1,5);
    }
  }
  // seawall along the bay, surf along the beach
  ctx.fillStyle='#efe9dc'; ctx.fillRect(BAY_X*TILE-4,y0*TILE,5,(y1-y0+1)*TILE);
  if(x1>=SEA_X-1){
    for(const [dx,al,ph] of [[4,.75,0],[26,.35,2.1]]){
      ctx.strokeStyle=`rgba(255,255,255,${al})`; ctx.lineWidth=5; ctx.lineCap='round'; ctx.beginPath();
      for(let yy=y0*TILE;yy<=(y1+1)*TILE;yy+=12){
        const xx=SEA_X*TILE+dx+Math.sin(yy*0.025+now*1.3+ph)*6+Math.sin(now*0.9+ph)*5;
        yy===y0*TILE?ctx.moveTo(xx,yy):ctx.lineTo(xx,yy);
      }
      ctx.stroke();
    }
  }
  // road markings
  ctx.fillStyle='#e8c85a';
  for(let y=y0;y<=y1;y++)for(let x=x0;x<=x1;x++){
    const bx=x%BLOCK, by=y%BLOCK, t=map[y*MW+x];
    if(t===ROAD && bx===0 && by>=ROADW){ for(let k=0;k<2;k++) ctx.fillRect(x*TILE+TILE-2, y*TILE+k*32+6, 4, 18); }
    if(t===ROAD && by===0 && (bx>=ROADW || x<BAY_X)){ for(let k=0;k<2;k++) ctx.fillRect(x*TILE+k*32+6, y*TILE+TILE-2, 18, 4); }
    if(t===LOT && (x-ROADW)%BLOCK%2===0){ ctx.fillStyle='rgba(240,236,220,.55)'; ctx.fillRect(x*TILE,y*TILE+8,3,48); ctx.fillStyle='#e8c85a'; }
  }
  // skids
  ctx.strokeStyle='rgba(18,18,20,.38)'; ctx.lineWidth=4; ctx.lineCap='round'; ctx.beginPath();
  for(const s of skids){ ctx.moveTo(s[0],s[1]); ctx.lineTo(s[2],s[3]); }
  ctx.stroke();

  drawCar(car.x,car.y,car.a,car.w,keys.down);
  for(const s of smoke){ ctx.fillStyle=`rgba(225,222,215,${s.life*0.28})`; ctx.beginPath(); ctx.arc(s.x,s.y,s.r,0,7); ctx.fill(); }

  // buildings, palms and umbrellas with fake perspective: tops pushed away from the camera, far ones first
  const vis=scenery.filter(d=>Math.abs(d.cx-cam.x)<hw+300 && Math.abs(d.cy-cam.y)<hh+300);
  vis.sort((a,b)=>Math.hypot(b.cx-cam.x,b.cy-cam.y)-Math.hypot(a.cx-cam.x,a.cy-cam.y));
  for(const d of vis){ if(d.t==='palm') drawPalm(d); else if(d.t==='umb') drawUmb(d); else drawBlock(d); }
}
function acrossBar(q,col,thick){
  const p=T.pts[q], t=T.tan[q], nx=-t[1], ny=t[0];
  ctx.beginPath(); ctx.moveTo(p[0]-nx*T.half,p[1]-ny*T.half); ctx.lineTo(p[0]+nx*T.half,p[1]+ny*T.half);
  ctx.lineWidth=thick; ctx.strokeStyle=col; ctx.stroke();
}
function renderTrack(hw,hh){
  const th=theme;
  ctx.lineJoin='round'; ctx.lineCap='butt';
  ctx.lineWidth=T.edge*2+22; ctx.strokeStyle=th.barrier[0]; ctx.stroke(T.path);
  ctx.setLineDash([30,30]); ctx.strokeStyle=th.barrier[1]; ctx.stroke(T.path); ctx.setLineDash([]);
  ctx.lineWidth=T.edge*2; ctx.strokeStyle=th.gravel; ctx.stroke(T.path);
  ctx.lineWidth=T.w+14; ctx.strokeStyle='#e9e5da'; ctx.stroke(T.path);
  ctx.setLineDash([18,18]); ctx.strokeStyle='#c8352b'; ctx.stroke(T.path); ctx.setLineDash([]);
  ctx.lineWidth=T.w; ctx.strokeStyle=th.asph; ctx.stroke(T.path);
  ctx.lineCap='butt';
  for(const z of T.zones){
    ctx.lineWidth=T.w; ctx.strokeStyle='rgba(255,207,58,0.08)'; ctx.stroke(z.path);
    acrossBar(z.a,'rgba(255,207,58,.85)',7); acrossBar(z.b,'rgba(255,207,58,.45)',4);
  }
  // start / finish line
  { const p=T.pts[0], t=T.tan[0], sq=T.w/10; ctx.save(); ctx.translate(p[0],p[1]); ctx.rotate(Math.atan2(t[1],t[0]));
    for(let c=0;c<10;c++) for(let r=0;r<2;r++){ ctx.fillStyle=(c+r)%2?'#111':'#eee'; ctx.fillRect(r*sq-sq,-T.half+c*sq,sq,sq); }
    ctx.restore(); }
  ctx.lineCap='round';
  ctx.strokeStyle='rgba(18,18,20,.4)'; ctx.lineWidth=4; ctx.beginPath();
  for(const sk of skids){ ctx.moveTo(sk[0],sk[1]); ctx.lineTo(sk[2],sk[3]); }
  ctx.stroke();
  // clipping points: dashed ring shows how close the rear has to get
  for(const z of T.zones) for(const c of z.clips){
    const hit=run && run.clipHit && run.clipHit.has(c);
    ctx.setLineDash([8,8]); ctx.lineWidth=2; ctx.strokeStyle=hit?'rgba(127,224,138,.5)':'rgba(255,255,255,.22)';
    ctx.beginPath(); ctx.arc(c.x,c.y,T.clipR,0,7); ctx.stroke(); ctx.setLineDash([]);
    ctx.fillStyle='rgba(0,0,0,.3)'; ctx.beginPath(); ctx.arc(c.x+2,c.y+3,10,0,7); ctx.fill();
    ctx.fillStyle=hit?'#4fd66a':'#ff7a1a'; ctx.beginPath(); ctx.arc(c.x,c.y,10,0,7); ctx.fill();
    ctx.fillStyle='#fff'; ctx.beginPath(); ctx.arc(c.x,c.y,4,0,7); ctx.fill();
  }
  if(run) drawGhost();
  drawCar(car.x,car.y,car.a,car.w,keys.down);
  for(const sm of smoke){ ctx.fillStyle=`rgba(225,222,215,${sm.life*0.28})`; ctx.beginPath(); ctx.arc(sm.x,sm.y,sm.r,0,7); ctx.fill(); }
  const vis=decos.filter(d=>{ const x=d.cx??d.x, y=d.cy??d.y; return Math.abs(x-cam.x)<hw+200 && Math.abs(y-cam.y)<hh+200; });
  vis.sort((a,b)=>Math.hypot((b.cx??b.x)-cam.x,(b.cy??b.y)-cam.y)-Math.hypot((a.cx??a.x)-cam.x,(a.cy??a.y)-cam.y));
  for(const d of vis){
    if(d.t==='box'){ drawBlock(d); continue; }
    const ox=d.x+(d.x-cam.x)*d.ht, oy=d.y+(d.y-cam.y)*d.ht;
    ctx.fillStyle='rgba(0,0,0,.28)'; ctx.beginPath(); ctx.arc(d.x+8,d.y+10,d.r,0,7); ctx.fill();
    if(d.t==='tree'){
      ctx.fillStyle=d.col; ctx.beginPath(); ctx.arc(ox,oy,d.r,0,7); ctx.fill();
      ctx.fillStyle=mixC(d.col,[255,255,255],.12); ctx.beginPath(); ctx.arc(ox-d.r*.25,oy-d.r*.25,d.r*.55,0,7); ctx.fill();
    } else if(d.t==='rock'){
      ctx.fillStyle=mixC(d.col,[0,0,0],.25); ctx.beginPath(); ctx.arc(d.x,d.y,d.r,0,7); ctx.fill();
      ctx.fillStyle=d.col; ctx.beginPath(); ctx.arc(ox,oy,d.r*.85,0,7); ctx.fill();
    } else {
      ctx.fillStyle='#161616'; ctx.beginPath(); ctx.arc(d.x,d.y,d.r,0,7); ctx.fill();
      ctx.fillStyle='#232323'; ctx.beginPath(); ctx.arc(ox,oy,d.r,0,7); ctx.fill();
      ctx.strokeStyle='#4a4a4a'; ctx.lineWidth=3; ctx.beginPath(); ctx.arc(ox,oy,d.r*.5,0,7); ctx.stroke();
    }
  }
}

// ---------- minimap ----------
const miniEl=$('mini'), mctx=miniEl.getContext('2d'), miniBg=document.createElement('canvas');
miniBg.width=miniBg.height=280; let mm={s:1,ox:0,oy:0};
function buildMini(){
  const c=miniBg.getContext('2d'); c.setTransform(1,0,0,1,0,0); c.clearRect(0,0,280,280);
  if(mode==='city'){
    const s=260/WORLD_W; mm={s,ox:10,oy:10};
    const MC={[ROAD]:'#55585e',[WALK]:'#b3aa9a',[GRASS]:'#5c9a45',[LOT]:'#55585e',[SAND]:'#eedba9',[WATER]:'#2a9cc2'}, ts=TILE*s;
    for(let y=0;y<MH;y++)for(let x=0;x<MW;x++){ c.fillStyle=MC[map[y*MW+x]]; c.fillRect(10+x*ts,10+y*ts,ts+.5,ts+.5); }
    for(const b of buildings){ c.fillStyle=shade(b.col,0.75); c.fillRect(10+b.x*s,10+b.y*s,b.w*s,b.h*s); }
    return;
  }
  let x0=1e9,y0=1e9,x1=-1e9,y1=-1e9; for(const p of T.pts){ x0=Math.min(x0,p[0]);y0=Math.min(y0,p[1]);x1=Math.max(x1,p[0]);y1=Math.max(y1,p[1]); }
  const pad=T.edge+60, s=Math.min(250/(x1-x0+2*pad),250/(y1-y0+2*pad));
  mm={s, ox:140-(x0+x1)/2*s, oy:140-(y0+y1)/2*s};
  c.setTransform(s,0,0,s,mm.ox,mm.oy); c.lineJoin='round';
  c.lineWidth=Math.max(T.w,5/s); c.strokeStyle='rgba(243,234,210,.75)'; c.stroke(T.path);
  c.lineWidth=Math.max(T.w,5/s)*0.55; c.strokeStyle='#ffcf3a'; T.zones.forEach(z=>c.stroke(z.path));
  const p=T.pts[0], t=T.tan[0]; c.lineWidth=5/s; c.strokeStyle='#fff'; c.beginPath();
  c.moveTo(p[0]+t[1]*T.w*1.2,p[1]-t[0]*T.w*1.2); c.lineTo(p[0]-t[1]*T.w*1.2,p[1]+t[0]*T.w*1.2); c.stroke();
}
function drawMini(){
  mctx.setTransform(1,0,0,1,0,0); mctx.clearRect(0,0,280,280); mctx.drawImage(miniBg,0,0);
  const gh=mode==='track'&&ghostAt(state==='count'?0:run&&run.t);
  if(gh){ mctx.fillStyle='rgba(191,233,255,.85)'; mctx.beginPath(); mctx.arc(gh.x*mm.s+mm.ox,gh.y*mm.s+mm.oy,6,0,7); mctx.fill(); }
  const x=car.x*mm.s+mm.ox, y=car.y*mm.s+mm.oy;
  mctx.translate(x,y); mctx.rotate(car.a); mctx.scale(1.4,1.4);
  mctx.fillStyle=paint.base; mctx.strokeStyle='#fff'; mctx.lineWidth=2.5;
  mctx.beginPath(); mctx.moveTo(12,0); mctx.lineTo(-8,-8); mctx.lineTo(-4,0); mctx.lineTo(-8,8); mctx.closePath(); mctx.fill(); mctx.stroke();
}

// ---------- loop ----------
const gearEl=$('gear'), scoreEl=$('score'), sub1El=$('sub1'), sub2El=$('sub2'), speedEl=$('speed'), cptsEl=$('cpts'), cmultEl=$('cmult');
const bannerEl=$('banner'), wrongEl=$('wrong');
let running=false, last=0, countT=0, goT=0, lastBeep=0;
const fmtT=t=>{ t=Math.max(0,Math.ceil(t)); return Math.floor(t/60)+':'+String(t%60).padStart(2,'0'); };
function silence(){
  es.load=0; aIn.throttle=0;
  if(AC){ const t=AC.currentTime; layerOn.gain.setTargetAtTime(0,t,0.05); layerOff.gain.setTargetAtTime(0,t,0.05); skidGain.gain.setTargetAtTime(0,t,0.05); }
}
function frame(t){
  const dt=Math.min(1/30,(t-last)/1000||0); last=t;
  let speed=Math.hypot(car.vx,car.vy);
  if(state==='race'||state==='free'){
    speed=update(dt); updateAudio(dt);
    if(state==='race'){ run.t+=dt; recordGhost(); if(run.t>=T.limit) finish(false); }
  } else if(state==='count'){
    countT-=dt; aIn.speed=0; aIn.slip=0; aIn.throttle=keys.up?1:0; updateAudio(dt);
    const n=Math.ceil(countT);
    if(n!==lastBeep && n<=3 && n>0){ lastBeep=n; sfxNotes([523],'square',0.12,0.1); }
    $('bn').textContent=n>3?'':String(n);
    if(countT<=0){ state='race'; goT=0.8; $('bn').textContent='GO'; sfxNotes([1047],'square',0.2,0.12); }
  }
  if(goT>0){ goT-=dt; if(goT<=0) bannerEl.style.display='none'; }
  running=state==='race'||state==='free'||state==='count';
  render(speed);
  miniEl.style.visibility=state==='menu'?'hidden':'visible';
  if(state!=='menu') drawMini();
  scoreEl.textContent=score.toLocaleString();
  speedEl.textContent=Math.round(speed*0.32);
  gearEl.textContent='Gear '+(es.gear+1)+'\u2003'+(Math.round(es.rpm/100)*100).toLocaleString()+' rpm';
  if(mode==='track' && run){
    sub1El.textContent='Target '+T.pass.toLocaleString();
    const left=T.limit-run.t;
    sub2El.textContent='Lap '+Math.min(run.lap+1,run.def.laps)+'/'+run.def.laps+'\u2003'+fmtT(left);
    sub2El.classList.toggle('warn',left<10);
    wrongEl.style.display=run.wrongT>0.7&&state==='race'?'block':'none';
  } else { sub1El.textContent='Best drift '+best.toLocaleString(); sub2El.textContent=''; wrongEl.style.display='none'; }
  if(chain.active){ comboEl.style.opacity=1; cptsEl.textContent=Math.round(chain.pts).toLocaleString(); cmultEl.textContent='x'+chain.mult; }
  else comboEl.style.opacity=0;
  if(toastT>0){ toastT-=dt; if(toastT<=0) toastEl.style.opacity=0; }
  if(zmsgT>0){ zmsgT-=dt; if(zmsgT<=0) zmsgEl.style.opacity=0; }
  requestAnimationFrame(frame);
}

// ---------- menus ----------
const loadProg=()=>{ try{ return JSON.parse(localStorage.getItem('sc_stages')||'{}'); }catch(e){ return {}; } };
const saveProg=p=>{ try{ localStorage.setItem('sc_stages',JSON.stringify(p)); }catch(e){} };
const starStr=n=>'\u2605'.repeat(n)+'\u2606'.repeat(3-n);
function clearFx(){ skids.length=0; smoke.length=0; chain.active=false; chain.pts=0; chain.mult=1; chain.time=0; prevWheels=null; score=0; toastEl.style.opacity=0; zmsgEl.style.opacity=0; }
function hideOverlays(){ $('start').style.display='none'; $('result').style.display='none'; }
function startStage(i){
  hideOverlays(); loadStage(i); clearFx();
  const def=STAGES[i];
  run={i,def,lap:0,t:0,pi:3,flags:0,zi:-1,zs:[],clipHit:new Set(),wrongT:0,rec:[],ghost:loadGhost(i)}; newLapZones();
  resetCar(); cam.x=car.x; cam.y=car.y; es.gear=0; es.rpm=IDLE;
  state='count'; countT=3.99; lastBeep=0; goT=0;
  $('bn').textContent=''; $('bt').textContent='Stage '+(i+1)+': '+def.name;
  $('bd').textContent=def.desc+' Target '+T.pass.toLocaleString()+' in '+def.laps+' laps, '+fmtT(T.limit)+' on the clock.'+(run.ghost?' Racing your ghost ('+run.ghost.score.toLocaleString()+').':'');
  bannerEl.style.display='block';
  initAudio();
}
function startCity(){
  hideOverlays(); mode='city'; T=null; run=null; clearFx(); buildMini();
  resetCar(); cam.x=car.x; cam.y=car.y; state='free'; bannerEl.style.display='none'; initAudio();
}
function showMenu(){
  state='menu'; pausedFrom=null; silence(); bannerEl.style.display='none'; $('result').style.display='none'; $('settings').style.display='none';
  renderStageGrid(); $('start').style.display='flex';
}
function renderStageGrid(){
  const prog=loadProg(), grid=$('stages'); grid.innerHTML='';
  STAGES.forEach((d,i)=>{
    const open=i===0||(prog[i-1]&&prog[i-1].stars>0), p=prog[i]||{best:0,stars:0};
    const b=document.createElement('button'); b.className='stage'; b.disabled=!open; b.title=d.desc;
    b.innerHTML='<span class="num">Stage '+(i+1)+'</span><span class="nm"></span><span class="st" aria-label="'+p.stars+' of 3 stars">'+starStr(p.stars)+'</span><span class="bs">'+(open?(p.best?'Best '+p.best.toLocaleString():'Not cleared yet'):'Locked: clear stage '+i)+'</span>';
    b.querySelector('.nm').textContent=d.name;
    b.addEventListener('click',()=>startStage(i)); grid.appendChild(b);
  });
}
function finish(completed){
  if(chain.active) bank();
  state='done'; silence(); wrongEl.style.display='none';
  const i=run.i, pass=T.pass;
  const stars=!completed?0:score>=pass*2?3:score>=pass*1.45?2:score>=pass?1:0;
  const prog=loadProg(), old=prog[i]||{best:0,stars:0};
  prog[i]={best:Math.max(old.best,stars?score:0),stars:Math.max(old.stars,stars)}; saveProg(prog);
  $('rTitle').textContent=!completed?'Out of time':stars?'Stage clear':'Not enough points';
  $('rTitle').style.color=stars?'var(--accent)':'var(--bad)';
  $('rStars').textContent=starStr(stars); $('rStars').setAttribute('aria-label',stars+' of 3 stars');
  $('rScore').textContent='Score '+score.toLocaleString()+' of '+pass.toLocaleString()+' needed. Two stars at '+Math.round(pass*1.45).toLocaleString()+', three at '+(pass*2).toLocaleString()+'.';
  let ghostMsg='';
  if(completed && score>0 && (!run.ghost || score>run.ghost.score)){
    recordGhost(); ghostMsg=saveGhost(i,{score,d:run.rec})?' New ghost saved.':'';
  }
  $('rBest').textContent='Your best here: '+(prog[i].best?prog[i].best.toLocaleString():'none yet')+'.'+ghostMsg;
  const hasNext=stars>0 && i<STAGES.length-1;
  $('rNext').style.display=hasNext?'':'none';
  $('rNext').onclick=()=>startStage(i+1); $('rRetry').onclick=()=>startStage(i); $('rMenu').onclick=showMenu;
  setTimeout(()=>{ if(state==='done'){ $('result').style.display='flex'; (hasNext?$('rNext'):$('rRetry')).focus(); } },700);
  sfxNotes(stars?[523,659,784,1047]:[392,330,262],stars?'square':'sawtooth',0.12,0.1);
}
$('menuBtn').addEventListener('click',e=>{ e.currentTarget.blur(); if(state!=='menu') showMenu(); });
// pause + settings panel (touch devices)
let pausedFrom=null;
function pause(){ if(state!=='race'&&state!=='free'&&state!=='count') return;
  pausedFrom=state; state='pause'; for(const k in keys) keys[k]=false; setSteer(0); silence();
  document.querySelectorAll('.touch button.on').forEach(b=>b.classList.remove('on'));
  $('settings').style.display='flex'; }
function resume(){ if(state!=='pause') return; state=pausedFrom; pausedFrom=null; $('settings').style.display='none'; }
$('pauseBtn').addEventListener('click',pause);
$('resume').addEventListener('click',resume);
$('resetT').addEventListener('click',e=>{ e.currentTarget.blur(); if(state==='race'||state==='free'){ wreck(); resetCar(); } });
document.addEventListener('visibilitychange',()=>{ if(document.hidden && isTouch) pause(); });
if(isTouch){
  $('sheetSlot').appendChild($('audio'));
  $('menuBtn').textContent='Quit to stages'; $('resetBtn').textContent='Reset car';
}
$('free').addEventListener('click',startCity);
$('resetBtn').addEventListener('click',e=>{ e.currentTarget.blur(); resume(); if(state==='race'||state==='free'){ wreck(); resetCar(); } });

// ---------- garage: pick a paint before driving ----------
const pv=$('preview'), pctx=pv.getContext('2d');
function drawPreview(){
  const main=ctx; ctx=pctx;
  ctx.setTransform(1,0,0,1,0,0);
  ctx.fillStyle='#3b3e44'; ctx.fillRect(0,0,pv.width,pv.height);
  ctx.fillStyle='#d9cf9f'; for(let x=10;x<pv.width;x+=70) ctx.fillRect(x,pv.height/2+78,36,6);
  ctx.setTransform(4.2,0,0,4.2,pv.width/2,pv.height/2);
  drawCar(0,0,-0.32,0,false);
  ctx.setTransform(1,0,0,1,0,0); ctx=main;
}
let paintHex='#f0631a';
try{ const h=localStorage.getItem('sc_paint'); if(/^#[0-9a-f]{6}$/i.test(h)) paintHex=h; }catch(e){}
const swWrap=$('swatches'), nameEl=$('paintName'), swBtns=[];
PAINTS.forEach(([name,hex])=>{
  const b=document.createElement('button');
  b.className='swatch'; b.style.background=hex; b.setAttribute('aria-label',name); b.title=name;
  b.addEventListener('click',()=>choosePaint(hex,name));
  swWrap.appendChild(b); swBtns.push([b,hex]);
});
const custom=document.createElement('label');
custom.className='swatch custom'; custom.title='Custom color';
custom.innerHTML='<input type="color" aria-label="Custom color">';
swWrap.appendChild(custom);
const customIn=custom.querySelector('input');
customIn.addEventListener('input',()=>choosePaint(customIn.value,'Custom'));
function choosePaint(hex,name){
  paintHex=hex.toLowerCase(); setPaint(paintHex);
  try{ localStorage.setItem('sc_paint',paintHex); }catch(e){}
  const preset=PAINTS.find(p=>p[1]===paintHex);
  nameEl.textContent=preset?preset[0]:(name||'Custom');
  swBtns.forEach(([b,h])=>b.setAttribute('aria-pressed',String(h===paintHex)));
  custom.style.boxShadow=preset?'':'0 0 0 3px var(--accent)';
  customIn.value=paintHex;
  drawPreview();
}
choosePaint(paintHex);

resetCar(); cam.x=car.x; cam.y=car.y; buildMini(); renderStageGrid();
requestAnimationFrame(frame);
})();
