class FetDSP {
  constructor(sr){
    this.sr = sr;
    this.p = {inDb:0, outDb:0, atk:4, rel:4, ratio:4, bypass:0};
    this.grDb = 0; this.gIn = 1; this.gOut = 1; this.gInT = 1; this.gOutT = 1;
    this.hpL = {x1:0, y1:0}; this.hpR = {x1:0, y1:0};
    this.meterGR = 0; this.meterPk = 0;
    const rc = 1/(2*Math.PI*18); this.hpA = rc/(rc + 1/sr);
    this.sm = Math.exp(-1/(0.02*sr));
    this.setParams({});
  }
  setParams(np){
    Object.assign(this.p, np);
    const p = this.p, sr = this.sr;
    // Attack 1..7 : 800 us -> 20 us ; Release 1..7 : 1100 ms -> 50 ms (7 = le plus rapide, comme sur l'original)
    const ta = 800e-6 * Math.pow(20/800, (p.atk-1)/6);
    const tr = 1.1 * Math.pow(0.05/1.1, (p.rel-1)/6);
    this.abm = p.ratio === 0;
    this.aA = Math.exp(-1/((this.abm ? ta*0.5 : ta)*sr));
    this.tRel = tr;
    // Ratio / seuil fixe / genou : seuil plus haut pour les ratios élevés (comportement de l'original)
    const T = {4:[4,-14,6], 8:[8,-11,4], 12:[12,-9,3], 20:[20,-7,2], 0:[20,-10,10]}[p.ratio] || [4,-14,6];
    this.R = T[0]; this.thr = T[1]; this.knee = T[2];
    this.gInT = Math.pow(10, p.inDb/20); this.gOutT = Math.pow(10, p.outDb/20);
  }
  resetMeters(){ this.meterGR = 0; this.meterPk = 0; }
  static(l){
    const xg = l - this.thr, h = this.knee/2, s = 1/this.R - 1;
    let over;
    if (xg <= -h) over = 0; else if (xg >= h) over = xg; else { const t = xg + h; over = t*t/(2*this.knee); }
    let gr = s*over;
    if (this.abm && xg > 8) gr -= 0.25*(xg - 8);   // « all buttons » : la sortie redescend quand on pousse
    return gr;
  }
  process(L, R, oL, oR, n){
    const p = this.p, sm = this.sm;
    // relâchement dépendant du programme : plus lent quand la réduction est profonde
    const aR = Math.exp(-1/(this.tRel*(1 + 0.6*Math.min(1, -this.grDb/20))*this.sr));
    let grMin = 0, pk = this.meterPk;
    for (let i = 0; i < n; i++){
      this.gIn += (this.gInT - this.gIn)*(1 - sm);
      this.gOut += (this.gOutT - this.gOut)*(1 - sm);
      const l = L[i]*this.gIn, r = R[i]*this.gIn;
      const a = Math.max(Math.abs(l), Math.abs(r));
      const lev = 20*Math.log10(a > 1e-6 ? a : 1e-6);
      const target = this.static(lev);
      this.grDb = target < this.grDb ? target + (this.grDb - target)*this.aA : target + (this.grDb - target)*aR;
      if (this.grDb < grMin) grMin = this.grDb;
      const g = Math.pow(10, this.grDb/20);
      // distorsion du FET : harmoniques paires qui augmentent avec la réduction (beaucoup plus en « all buttons »)
      const depth = Math.min(1, -this.grDb/20);
      const e = this.abm ? 0.06 + 0.35*depth : 0.08*depth;
      let yl = l*g, yr = r*g;
      yl = yl + e*yl*yl; yr = yr + e*yr*yr;
      // ampli de sortie classe A : saturation douce
      yl = Math.tanh(yl*1.2)/1.2; yr = Math.tanh(yr*1.2)/1.2;
      // couplage transfo (coupe-bas 18 Hz, retire aussi le continu créé par la distorsion paire)
      const hl = this.hpA*(this.hpL.y1 + yl - this.hpL.x1); this.hpL.x1 = yl; this.hpL.y1 = hl;
      const hr = this.hpA*(this.hpR.y1 + yr - this.hpR.x1); this.hpR.x1 = yr; this.hpR.y1 = hr;
      const ol = hl*this.gOut, or = hr*this.gOut;
      oL[i] = ol; oR[i] = or;
      const m = Math.max(Math.abs(ol), Math.abs(or)); if (m > pk) pk = m;
    }
    if (grMin < this.meterGR) this.meterGR = grMin;
    this.meterPk = pk;
  }
}
class FetProc extends AudioWorkletProcessor{
  constructor(){ super(); this.dsp=new FetDSP(sampleRate); this.cnt=0; this.port.onmessage=e=>this.dsp.setParams(e.data); }
  process(inputs,outputs){
    const inp=inputs[0], out=outputs[0];
    if(!inp || !inp[0] || !out || !out[0]) return true;
    const L=inp[0], R=inp[1]||inp[0], oL=out[0], oR=out[1]||out[0];
    this.dsp.process(L,R,oL,oR,L.length);
    if(++this.cnt>=6){ this.port.postMessage({gr:this.dsp.meterGR, pk:this.dsp.meterPk}); this.dsp.resetMeters(); this.cnt=0; }
    return true;
  }
}
registerProcessor('al-fet',FetProc);
