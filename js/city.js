// ---------- endless city for survival: an organic street network generated cell by cell ----------
// Avenues join jittered lattice nodes with curved (quadratic) roads, some nodes become roundabouts,
// and each cell between four nodes gets side streets (through streets, crescents, cul-de-sacs),
// a park, or buildings that face and follow the nearest street. Everything is derived from a hash
// of the cell coordinates, so a cell always comes out the same and neighbours agree on shared edges.
function createEndlessCity(o){   // o: surface ids, palettes and makePalm from game.js
  const CH=960, RING_R=130, RING_W=110, SIDE_W=96, STEP=24, COARSE=40;
  const cells=new Map();

  const hash=(i,j,k)=>{
    let h=(Math.imul(i,374761393)+Math.imul(j,668265263)+Math.imul(k+1,1274126177))^0x5bf03635;
    h=Math.imul(h^(h>>>15),2246822519); h=Math.imul(h^(h>>>13),3266489917); h^=h>>>16;
    return (h>>>0)/4294967296;
  };
  const rng=(i,j)=>{ let n=0; return ()=>hash(i,j,1000+(n++)); };

  // the avenue network, shared by neighbouring cells
  const isOrigin=(i,j)=>i===0&&j===0;
  const node=(i,j)=>isOrigin(i,j)?[0,0]:[i*CH+(hash(i,j,1)-0.5)*CH*0.34, j*CH+(hash(i,j,2)-0.5)*CH*0.34];
  const ring=(i,j)=>!isOrigin(i,j)&&hash(i,j,3)<0.16;
  const edgeOn=(k,i,j)=>{   // a few avenues are missing: bigger blocks and T-junctions (never next to the start)
    if(k==='h'?(j===0&&(i===0||i===-1)):(i===0&&(j===0||j===-1))) return true;
    return hash(i,j,k==='h'?4:5)>0.1;
  };
  const edgeW=(k,i,j)=>((k==='h'?j:i)%3===0)?180:146;   // every third line is a wide boulevard
  function edge(k,i,j){
    const a=node(i,j), b=k==='h'?node(i+1,j):node(i,j+1), dx=b[0]-a[0], dy=b[1]-a[1], L=Math.hypot(dx,dy);
    const strong=hash(i,j,k==='h'?8:9)<0.4, bend=(hash(i,j,k==='h'?6:7)-0.5)*CH*(strong?0.5:0.08);
    return {a,b,c:[(a[0]+b[0])/2-dy/L*bend,(a[1]+b[1])/2+dx/L*bend]};
  }

  // geometry helpers
  const bez=(e,t)=>{ const u=1-t; return [u*u*e.a[0]+2*u*t*e.c[0]+t*t*e.b[0], u*u*e.a[1]+2*u*t*e.c[1]+t*t*e.b[1]]; };
  function bezPts(e,step){
    const n=Math.max(3,Math.ceil(Math.hypot(e.b[0]-e.a[0],e.b[1]-e.a[1])/step)), p=[];
    for(let q=0;q<=n;q++) p.push(bez(e,q/n));
    return p;
  }
  function circlePts(x,y,r,n){ const p=[]; for(let q=0;q<n;q++){ const t=q/n*Math.PI*2; p.push([x+Math.cos(t)*r,y+Math.sin(t)*r]); } return p; }
  function lineInfo(px,py,pts,closed){   // nearest point on a polyline, with the segment direction
    let best=1e9,bx=pts[0][0],by=pts[0][1],tx=1,ty=0;
    const n=pts.length, m=closed?n:n-1;
    for(let q=0;q<Math.max(1,m);q++){
      const a=pts[q], b=pts[(q+1)%n], dx=b[0]-a[0], dy=b[1]-a[1], L2=dx*dx+dy*dy;
      let u=L2?((px-a[0])*dx+(py-a[1])*dy)/L2:0; u=u<0?0:u>1?1:u;
      const qx=a[0]+dx*u, qy=a[1]+dy*u, d=Math.hypot(px-qx,py-qy);
      if(d<best){ best=d; bx=qx; by=qy; if(L2){ tx=dx; ty=dy; } }
    }
    const L=Math.hypot(tx,ty)||1;
    return {d:best,x:bx,y:by,tx:tx/L,ty:ty/L};
  }
  function inPoly(x,y,P){
    let c=false;
    for(let q=0,k=P.length-1;q<P.length;k=q++){ const a=P[q], b=P[k];
      if((a[1]>y)!==(b[1]>y) && x<(b[0]-a[0])*(y-a[1])/(b[1]-a[1])+a[0]) c=!c; }
    return c;
  }
  function bbox(pts,m){
    let x0=1e9,y0=1e9,x1=-1e9,y1=-1e9;
    for(const p of pts){ if(p[0]<x0)x0=p[0]; if(p[0]>x1)x1=p[0]; if(p[1]<y0)y0=p[1]; if(p[1]>y1)y1=p[1]; }
    return [x0-m,y0-m,x1+m,y1+m];
  }
  function obb(cx,cy,hw,hh,ang){   // corners clockwise on screen, like the Miami buildings
    const c=Math.cos(ang), s=Math.sin(ang);
    return [[-hw,-hh],[hw,-hh],[hw,hh],[-hw,hh]].map(([x,y])=>[cx+x*c-y*s, cy+x*s+y*c]);
  }
  function quadsOverlap(A,B){   // separating axis test for two convex quads
    for(const P of [A,B]) for(let q=0;q<4;q++){
      const a=P[q], b=P[(q+1)%4], nx=b[1]-a[1], ny=a[0]-b[0];
      let amin=1e18,amax=-1e18,bmin=1e18,bmax=-1e18;
      for(const p of A){ const v=p[0]*nx+p[1]*ny; if(v<amin)amin=v; if(v>amax)amax=v; }
      for(const p of B){ const v=p[0]*nx+p[1]*ny; if(v<bmin)bmin=v; if(v>bmax)bmax=v; }
      if(amax<bmin||bmax<amin) return false;
    }
    return true;
  }
  const toPath=(pts,closed)=>{ const p=new Path2D(); pts.forEach((q,k)=>k?p.lineTo(q[0],q[1]):p.moveTo(q[0],q[1])); if(closed) p.closePath(); return p; };

  function genCell(i,j){
    const R=rng(i,j), pick=a=>a[Math.floor(R()*a.length)];
    const C={i,j,roads:[],parks:[],squares:[],islands:[],lots:[],solids:[],scenery:[]};
    const obs=[];   // everything buildings must keep clear of: {pts, w, closed}
    const addRoad=(pts,w,main,closed)=>{ C.roads.push({pts,w,main:!!main,closed:!!closed,bb:bbox(pts,w/2+4),path:toPath(pts,closed)}); };

    const E={top:edge('h',i,j), right:edge('v',i+1,j), bottom:edge('h',i,j+1), left:edge('v',i,j)};
    const on={top:edgeOn('h',i,j), right:edgeOn('v',i+1,j), bottom:edgeOn('h',i,j+1), left:edgeOn('v',i,j)};
    const W={top:edgeW('h',i,j), right:edgeW('v',i+1,j), bottom:edgeW('h',i,j+1), left:edgeW('v',i,j)};
    // this cell draws its top and left avenues and its own corner roundabout; neighbours draw the rest
    if(on.top) addRoad(bezPts(E.top,STEP),W.top,true);
    if(on.left) addRoad(bezPts(E.left,STEP),W.left,true);
    if(ring(i,j)){
      const n=node(i,j);
      addRoad(circlePts(n[0],n[1],RING_R,40),RING_W,false,true);
      C.islands.push({x:n[0],y:n[1],r:RING_R-RING_W/2});
      C.scenery.push(o.makePalm(n[0],n[1],R));
    }
    for(const k of ['top','right','bottom','left']) obs.push({pts:bezPts(E[k],COARSE),w:on[k]?W[k]:0});   // a missing avenue still splits cells
    for(const [a,b] of [[i,j],[i+1,j],[i+1,j+1],[i,j+1]]) if(ring(a,b)){ const n=node(a,b); obs.push({pts:[n,n],w:(RING_R+RING_W/2)*2}); }

    const boundary=[...bezPts(E.top,COARSE),...bezPts(E.right,COARSE),...bezPts(E.bottom,COARSE).reverse(),...bezPts(E.left,COARSE).reverse()];
    const n00=node(i,j), n10=node(i+1,j), n11=node(i+1,j+1), n01=node(i,j+1);
    const ctr=[(n00[0]+n10[0]+n11[0]+n01[0])/4,(n00[1]+n10[1]+n11[1]+n01[1])/4];
    const kr=R(), kind=isOrigin(i,j)?'square':kr<0.1?'park':kr<0.45?'square':'city';   // lots of open asphalt to fight on
    const street=(pts,w)=>{ addRoad(pts,w); obs.push({pts,w}); };
    const curve=(a,c,b)=>bezPts({a,c,b},STEP);

    if(kind==='city'){   // side streets
      const v=R(), mid=k=>bez(E[k],0.5);
      const through=(k1,k2)=>{ const a=mid(k1), b=mid(k2); street(curve(a,[ctr[0]+(R()-0.5)*CH*0.3,ctr[1]+(R()-0.5)*CH*0.3],b),SIDE_W); };
      if(v<0.22 && on.top&&on.bottom) through('top','bottom');
      else if(v<0.44 && on.left&&on.right) through('left','right');
      else if(v<0.56 && on.top&&on.bottom&&on.left&&on.right){ through('top','bottom'); through('left','right'); }
      else if(v<0.74){   // crescent: a round street cutting off one corner
        const [k1,t1,k2,t2,cn]=pick([['top',.38,'left',.38,n00],['top',.62,'right',.38,n10],['right',.62,'bottom',.62,n11],['bottom',.38,'left',.62,n01]]);
        if(on[k1]&&on[k2]) street(curve(bez(E[k1],t1),[cn[0]+(ctr[0]-cn[0])*0.6,cn[1]+(ctr[1]-cn[1])*0.6],bez(E[k2],t2)),SIDE_W);
      }
      else if(v<0.88){   // cul-de-sac with a turning circle
        const ks=['top','right','bottom','left'].filter(k=>on[k]);
        if(ks.length){
          const a=mid(pick(ks)), end=[a[0]+(ctr[0]-a[0])*0.6,a[1]+(ctr[1]-a[1])*0.6];
          street(curve(a,[(a[0]+end[0])/2+(R()-0.5)*90,(a[1]+end[1])/2+(R()-0.5)*90],end),SIDE_W);
          addRoad(circlePts(end[0],end[1],28,16),56,false,true); obs.push({pts:[end,end],w:112});
        }
      }
    }

    const clear=(x,y)=>{   // distance from (x,y) to the nearest road edge, and which road
      let best=null;
      for(const ob of obs){ const li=lineInfo(x,y,ob.pts,ob.closed), c=li.d-ob.w/2; if(!best||c<best.c){ best=li; best.c=c; best.half=ob.w/2; } }
      return best;
    };
    const placed=[];
    if(kind==='park') C.parks.push({pts:boundary,path:toPath(boundary,true)});
    if(kind==='square') C.squares.push({pts:boundary,path:toPath(boundary,true),x:ctr[0],y:ctr[1],r:140+R()*80});
    if(kind==='city'){   // buildings and parking lots, fronting the nearest street
      const bb=bbox(boundary,0);
      for(let gy=bb[1]+60;gy<bb[3];gy+=160) for(let gx=bb[0]+60;gx<bb[2];gx+=160){
        const px=gx+(R()-0.5)*60, py=gy+(R()-0.5)*60;
        if(R()<0.3||!inPoly(px,py,boundary)) continue;
        const nr=clear(px,py); if(nr.c<30) continue;
        const lot=R()<0.4, tall=!lot&&R()<0.2;
        const fw=lot?200+R()*120:tall?100+R()*50:60+R()*70, dp=lot?130+R()*70:tall?100+R()*50:55+R()*55;   // frontage, depth
        let nx=px-nr.x, ny=py-nr.y; const nl=Math.hypot(nx,ny)||1; nx/=nl; ny/=nl;
        const off=nr.half+16+dp/2, cx=nr.x+nx*off, cy=nr.y+ny*off, ang=Math.atan2(nr.ty,nr.tx);
        const pts=obb(cx,cy,fw/2,dp/2,ang);
        if(!pts.every(p=>inPoly(p[0],p[1],boundary)&&clear(p[0],p[1]).c>=12)) continue;
        if(clear(cx,cy).c<Math.min(fw,dp)/2) continue;
        const grown=obb(cx,cy,fw/2+10,dp/2+10,ang);
        if(placed.some(q=>quadsOverlap(grown,q))) continue;
        placed.push(pts);
        if(lot){ C.lots.push({pts,ang,cx,cy,fw,dp,path:toPath(pts,true)}); continue; }
        const pool=(tall||fw*dp>9000)&&R()<0.5, rad=Math.hypot(fw,dp)/2;
        const b={pts,ang,cx,cy,hw:fw/2,hh:dp/2,rad,bb:[cx-rad,cy-rad,cx+rad,cy+rad],
          ht:tall?0.38+R()*0.26:0.1+R()*0.14,col:pick(tall?o.TOWERS:o.DECO),acc:tall?null:pick(o.ACCENT),vents:pool?0:Math.floor(R()*3),pool,s:R()};
        C.solids.push(b); C.scenery.push(b);
      }
    }
    // palms along every street on this side, then scattered through parks and the start plaza
    const palmOk=(x,y)=>{ if(!inPoly(x,y,boundary)) return false; const c=clear(x,y); if(c.c<8) return false;
      const sq=obb(x,y,10,10,0); return !placed.some(q=>quadsOverlap(sq,q)); };
    for(const ob of obs){ if(!ob.w||ob.pts.length<3) continue;
      let acc=R()*120;
      for(let q=1;q<ob.pts.length;q++){
        const a=ob.pts[q-1], b=ob.pts[q], L=Math.hypot(b[0]-a[0],b[1]-a[1]); acc+=L;
        if(acc<140+R()*60) continue; acc=0;
        const tx=(b[0]-a[0])/L, ty=(b[1]-a[1])/L, off=ob.w/2+22;
        for(const sgn of [1,-1]){ const x=b[0]-ty*off*sgn, y=b[1]+tx*off*sgn; if(palmOk(x,y)) C.scenery.push(o.makePalm(x,y,R)); }
      }
    }
    if(kind==='park'){ const bb=bbox(boundary,0);
      for(let k=0;k<10;k++){ const x=bb[0]+R()*(bb[2]-bb[0]), y=bb[1]+R()*(bb[3]-bb[1]); if(palmOk(x,y)&&clear(x,y).c>30) C.scenery.push(o.makePalm(x,y,R)); } }
    return C;
  }

  function getCell(i,j){ const k=i+','+j; let c=cells.get(k); if(!c){ c=genCell(i,j); cells.set(k,c); } return c; }
  function cellsIn(x0,y0,x1,y1){   // cells whose contents can reach into the rectangle
    const out=[];
    for(let j=Math.floor(y0/CH)-1;j<=Math.floor(y1/CH)+1;j++) for(let i=Math.floor(x0/CH)-1;i<=Math.floor(x1/CH)+1;i++) out.push(getCell(i,j));
    return out;
  }
  function surfaceAt(x,y){
    const near=cellsIn(x,y,x,y);
    for(const C of near) for(const s of C.islands) if((x-s.x)**2+(y-s.y)**2<s.r*s.r) return o.GRASS;
    for(const C of near) for(const r of C.roads){ const b=r.bb; if(x<b[0]||x>b[2]||y<b[1]||y>b[3]) continue; if(lineInfo(x,y,r.pts,r.closed).d<r.w/2) return o.ROAD; }
    for(const C of near) for(const p of C.parks) if(inPoly(x,y,p.pts)) return o.GRASS;
    for(const C of near) for(const q of C.squares) if(inPoly(x,y,q.pts)) return o.ROAD;
    for(const C of near) for(const l of C.lots) if(inPoly(x,y,l.pts)) return o.LOT;
    return o.WALK;
  }
  function forSolidsNear(x,y,r,fn){   // fn(building) for walls whose bounds touch the circle; return true to stop
    for(const C of cellsIn(x-r,y-r,x+r,y+r)) for(const b of C.solids){
      const bb=b.bb; if(x+r<bb[0]||x-r>bb[2]||y+r<bb[1]||y-r>bb[3]) continue;
      if(fn(b)) return;
    }
  }
  function nearestRoad(x,y){   // a spot on the closest street, facing along it
    let best=null;
    for(const C of cellsIn(x,y,x,y)) for(const r of C.roads){ if(r.w<SIDE_W) continue; const li=lineInfo(x,y,r.pts,r.closed); if(!best||li.d<best.d) best=li; }
    return {x:best.x,y:best.y,a:Math.atan2(best.ty,best.tx)};
  }
  function start(){ const e=edge('h',0,0), p=bez(e,0.1), q=bez(e,0.12); return {x:p[0],y:p[1],a:Math.atan2(q[1]-p[1],q[0]-p[0])}; }
  function evict(x,y){   // keep memory flat however far the car goes
    if(cells.size<140) return;
    const ci=Math.floor(x/CH), cj=Math.floor(y/CH);
    for(const [k,c] of cells) if(Math.abs(c.i-ci)>4||Math.abs(c.j-cj)>4) cells.delete(k);
  }
  return {CH,cellsIn,surfaceAt,forSolidsNear,nearestRoad,start,evict,clear:()=>cells.clear(),size:()=>cells.size};
}
