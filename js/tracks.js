// ---------- stages: drift tracks ----------
const STAGES=[
 {name:'Practice Bowl', desc:'Wide and forgiving. Learn to link the corners.', w:230, r:120, laps:2, v:270, theme:'grass',
  pts:[[500,500],[2300,500],[2750,900],[2700,1500],[2250,1800],[1500,1700],[1150,2050],[1250,2550],[750,2750],[380,2250],[350,1200]]},
 {name:'Container Port', desc:'Two long hairpins between the stacks. Keep the rear out.', w:200, r:90, laps:2, v:290, theme:'port',
  pts:[[400,400],[2600,400],[2850,720],[2550,1000],[1150,1020],[880,1270],[1150,1520],[2550,1540],[2850,1850],[2550,2170],[1150,2200],[700,2500],[380,2150]]},
 {name:'Mountain Touge', desc:'Narrow road, short run-off, trees everywhere. One mistake costs the zone.', w:175, r:60, laps:2, v:300, theme:'forest',
  pts:[[500,300],[1500,380],[2500,300],[2850,650],[2450,1020],[1650,920],[1200,1250],[1700,1600],[2600,1520],[2950,1950],[2550,2450],[1650,2330],[950,2650],[420,2250],[720,1650],[300,1100]]},
 {name:'Crossover', desc:'A figure eight under the lights. Watch the crossing.', w:185, r:75, laps:2, v:310, theme:'stadium', eight:true},
 {name:'Snake Canyon', desc:'Endless direction changes. Transitions decide everything.', w:155, r:50, laps:2, v:320, theme:'desert',
  pts:[[450,520],[950,300],[1450,620],[1950,300],[2450,620],[2950,420],[3150,950],[2800,1420],[3150,1920],[2850,2450],[2300,2750],[1800,2420],[1300,2750],[800,2420],[380,2620],[230,2000],[560,1500],[230,1020]]},
 {name:"Devil's Knot", desc:'Night run. Tight, twisted and unforgiving. Only the best finish.', w:138, r:38, laps:2, v:330, theme:'night',
  pts:[[500,420],[1700,420],[2050,720],[1750,1040],[1250,960],[950,1250],[1250,1550],[2200,1430],[2600,930],[3050,1200],[2950,2000],[2450,2250],[2150,1930],[1700,2250],[2000,2750],[1300,2850],[700,2550],[900,2020],[420,1720],[300,1000]]},
];
function eightPts(){ const p=[]; for(let i=0;i<16;i++){ const t=i/16*Math.PI*2; p.push([1750+1400*Math.cos(t), 1450+1150*Math.sin(t)*Math.cos(t)]); } return p; }

function buildTrack(def){
  const P=def.eight?eightPts():def.pts, n=P.length, out=[];
  for(let i=0;i<n;i++){                       // closed Catmull-Rom spline
    const p0=P[(i-1+n)%n], p1=P[i], p2=P[(i+1)%n], p3=P[(i+2)%n];
    const seg=Math.hypot(p2[0]-p1[0],p2[1]-p1[1]), steps=Math.max(4,Math.ceil(seg/10));
    for(let s=0;s<steps;s++){ const t=s/steps, t2=t*t, t3=t2*t;
      const f=(a,b,c,d)=>0.5*((2*b)+(-a+c)*t+(2*a-5*b+4*c-d)*t2+(-a+3*b-3*c+d)*t3);
      out.push([f(p0[0],p1[0],p2[0],p3[0]), f(p0[1],p1[1],p2[1],p3[1])]); }
  }
  // resample to even 18px spacing
  const cum=[0]; for(let i=1;i<=out.length;i++){ const a=out[i-1], b=out[i%out.length]; cum.push(cum[i-1]+Math.hypot(b[0]-a[0],b[1]-a[1])); }
  const L=cum[out.length], N=Math.round(L/18), pts=[]; let j=0;
  for(let k=0;k<N;k++){ const s=k*L/N; while(cum[j+1]<s) j++;
    const a=out[j], b=out[(j+1)%out.length], u=(s-cum[j])/(cum[j+1]-cum[j]||1);
    pts.push([a[0]+(b[0]-a[0])*u, a[1]+(b[1]-a[1])*u]); }
  // tangents and curvature
  const tan=[], k=[];
  for(let i=0;i<N;i++){ const a=pts[(i-1+N)%N], b=pts[(i+1)%N]; const d=Math.hypot(b[0]-a[0],b[1]-a[1]); tan.push([(b[0]-a[0])/d,(b[1]-a[1])/d]); }
  for(let i=0;i<N;i++){ const t0=tan[(i-1+N)%N], t1=tan[(i+1)%N]; k.push((t0[0]*t1[1]-t0[1]*t1[0])/36); }
  const ks=k.map((_,i)=>{ let s=0; for(let d=-4;d<=4;d++) s+=k[(i+d+N)%N]; return s/9; });
  // start line in the middle of the longest straight
  let bestLen=0, bestMid=0, run=0;
  for(let i=0;i<2*N;i++){ if(Math.abs(ks[i%N])<0.0012){ run++; if(run>bestLen){ bestLen=run; bestMid=i-Math.floor(run/2); } } else run=0; }
  let off=((bestMid%N)+N)%N;
  if(def.eight){ let c=0,cd=1e9; pts.forEach((p,i)=>{ const d=Math.hypot(p[0]-1750,p[1]-1450); if(d<cd){cd=d;c=i;} }); off=(c+Math.round(N*0.09))%N; }
  const rot=a=>a.slice(off).concat(a.slice(0,off));
  const T={def, N, L, pts:rot(pts), tan:rot(tan), k:rot(ks), w:def.w, half:def.w/2, edge:def.w/2+def.r};
  // drift zones: corner runs, padded and merged
  const thr=1/(def.w*3.3), cor=T.k.map(v=>Math.abs(v)>thr);
  let zones=[], i=0;
  while(i<N){ if(cor[i]){ let j=i; while(j<N&&cor[j]) j++; zones.push([i,j-1]); i=j; } else i++; }
  zones=zones.filter(z=>(z[1]-z[0])*18>def.w*1.1).map(z=>[Math.max(8,z[0]-7),Math.min(N-9,z[1]+5)]);
  const merged=[]; for(const z of zones){ const m=merged[merged.length-1]; if(m && z[0]-m[1]<10) m[1]=z[1]; else merged.push(z.slice()); }
  T.zones=merged.map(([a,b])=>{
    // clipping points: the sharpest spots of the zone, on the inside edge
    const peaks=[];
    for(let q=a+1;q<b;q++){ const v=Math.abs(T.k[q]);
      if(v>thr*1.5 && v>=Math.abs(T.k[q-1]) && v>=Math.abs(T.k[q+1]) && !peaks.some(pk=>Math.abs(pk-q)<16)) peaks.push(q); }
    if(!peaks.length){ let apex=a,mk=0; for(let q=a;q<=b;q++) if(Math.abs(T.k[q])>mk){ mk=Math.abs(T.k[q]); apex=q; } peaks.push(apex); }
    const clips=peaks.map(q=>{ const side=Math.sign(T.k[q]), p=T.pts[q], t=T.tan[q];
      const nx=-t[1]*side, ny=t[0]*side, o=T.half-16; return {i:q,x:p[0]+nx*o,y:p[1]+ny*o}; });
    return {a,b,len:(b-a)*18,clips};
  });
  // bounds
  let x0=1e9,y0=1e9,x1=-1e9,y1=-1e9; for(const p of T.pts){ x0=Math.min(x0,p[0]);y0=Math.min(y0,p[1]);x1=Math.max(x1,p[0]);y1=Math.max(y1,p[1]); }
  T.bounds=[x0-T.edge-300,y0-T.edge-300,x1+T.edge+300,y1+T.edge+300];
  return T;
}
