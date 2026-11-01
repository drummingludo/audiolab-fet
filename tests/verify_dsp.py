"""Banc de test du moteur FET : compare le plugin (C++) au vrai code d'Audio Lab (JS, worklet al-fet),
puis mesure les courbes statiques (ratios 4/8/12/20/All) et les temps d'attaque / relâchement."""
import hashlib, json, os, subprocess, sys, numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
TMP = os.environ.get("ALF_TMP", "/tmp/alf")
os.makedirs(TMP, exist_ok=True)
rng = np.random.default_rng(1234)
OK = True
REF_SHA = "e26781f73e8b2401e70ea370c7fd824a9ecb3d89d0bf690fb9da82e40eca536b"   # Audio_Lab.html du 17/10/2026, lignes 1542–1620

def check(cond, msg):
    global OK
    print(("  OK   " if cond else "  ÉCHEC ") + msg)
    OK &= bool(cond)

def P(**k):
    base = dict(inDb=0.0, outDb=0.0, atk=4.0, rel=4.0, ratio=4); base.update(k); return base

def write_params(prm):
    pj = os.path.join(TMP, "p.json"); json.dump(dict(prm, bypass=0), open(pj, "w"))
    pt = os.path.join(TMP, "p.txt"); open(pt, "w").write("%r %r %r %r %d" % (prm["inDb"], prm["outDb"], prm["atk"], prm["rel"], prm["ratio"]))
    return pj, pt

def run_both(fs, x, prm, block=128):
    inp = os.path.join(TMP, "in.f32"); x.astype(np.float32).tofile(inp)
    pj, pt = write_params(prm)
    oj, oc = os.path.join(TMP, "oj.f32"), os.path.join(TMP, "oc.f32")
    subprocess.run(["node", os.path.join(HERE, "ref_run.js"), str(fs), pj, inp, oj], check=True)
    subprocess.run([os.path.join(HERE, "core_run"), str(fs), pt, inp, oc, str(block)], check=True)
    return np.fromfile(oj, np.float32).reshape(-1, 2), np.fromfile(oc, np.float32).reshape(-1, 2)

def trace(fs, x, prm, block=16):
    inp = os.path.join(TMP, "in.f32"); x.astype(np.float32).tofile(inp)
    _, pt = write_params(prm)
    r = subprocess.run([os.path.join(HERE, "core_run"), str(fs), pt, inp, os.path.join(TMP, "oc.f32"), str(block), "t"],
                       check=True, capture_output=True, text=True)
    return np.array([float(v) for v in r.stdout.split()])

def music(fs, sec=4.0):
    n = int(fs * sec); t = np.arange(n) / fs; L = np.zeros(n); R = np.zeros(n)
    for k in range(int(sec / 0.5)):
        s = int(k * 0.5 * fs); m = min(n - s, int(0.25 * fs)); tt = np.arange(m) / fs
        kick = np.sin(2 * np.pi * (50 + 120 * np.exp(-tt * 30)) * tt) * np.exp(-tt * 9) * 0.9
        L[s:s + m] += kick; R[s:s + m] += kick
        if k % 2:
            sn = rng.standard_normal(m) * np.exp(-tt * 18) * 0.5
            L[s:s + m] += sn; R[s:s + m] += 0.8 * sn
    bass = 0.25 * np.sin(2 * np.pi * 55 * t) * (0.6 + 0.4 * np.sin(2 * np.pi * 0.5 * t))
    voice = 0.2 * np.sin(2 * np.pi * 220 * t + 3 * np.sin(2 * np.pi * 5 * t)) * (np.sin(2 * np.pi * 0.7 * t) > -0.3)
    L += bass + voice; R += bass + 0.6 * voice + 0.02 * rng.standard_normal(n)
    L[int(3.2 * fs):int(3.5 * fs)] = 0; R[int(3.2 * fs):int(3.5 * fs)] = 0
    return np.stack([L, R], 1).astype(np.float32)

print("\n0) Copie du code web utilisée comme référence")
check(hashlib.sha256(open(os.path.join(HERE, "fet_ref.js"), "rb").read()).hexdigest() == REF_SHA,
      "fet_ref.js = classe FetDSP d'Audio_Lab.html (empreinte SHA-256)")

print("\n1) Comparaison plugin (C++) ↔ Audio Lab (code web d'origine)")
cases = [("Réglages par défaut (ratio 4, attaque/relâchement 4)", P()),
         ("Voix (input +12, ratio 4, atk 3, rel 5)", P(inDb=12, outDb=-6, atk=3, rel=5)),
         ("Batterie (input +18, ratio 8, atk 1, rel 7)", P(inDb=18, outDb=-8, atk=1, rel=7)),
         ("Limiteur (ratio 20, atk 7, rel 6)", P(inDb=20, outDb=-10, atk=7, rel=6, ratio=20)),
         ("Ratio 12", P(inDb=15, ratio=12)),
         ("All buttons (input +24, atk 7, rel 7)", P(inDb=24, outDb=-12, atk=7, rel=7, ratio=0)),
         ("Extrêmes (input +30, output +20, atk 1, rel 1)", P(inDb=30, outDb=20, atk=1, rel=1, ratio=0)),
         ("Valeurs entre deux crans (atk 2,5, rel 5,7)", P(inDb=10, atk=2.5, rel=5.7, ratio=8))]
for fs in (44100, 48000, 96000):
    x = music(fs)
    for name, prm in cases:
        oj, oc = run_both(fs, x, prm)
        d = np.max(np.abs(oj - oc)); same = np.mean(oj == oc) * 100
        check(d <= 1e-6 * max(1, np.max(np.abs(oj))), f"{fs} Hz · {name} : écart max {d:.1e}, {same:.2f} % identiques au bit près")
x = music(48000)
for b in (1, 64, 100, 512, 1024, 4096):
    oj, oc = run_both(48000, x, cases[1][1], block=b)
    check(np.max(np.abs(oj - oc)) <= 1e-6, f"blocs de {b} éch. (logiciel hôte) : même résultat que le web")

fs = 48000
TABLE = {4: (4, -14, 6), 8: (8, -11, 4), 12: (12, -9, 3), 20: (20, -7, 2), 0: (20, -10, 10)}
def theory(l, ratio):
    R, thr, knee = TABLE[ratio]; xg = l - thr; h = knee / 2
    over = 0 if xg <= -h else (xg if xg >= h else (xg + h) ** 2 / (2 * knee))
    gr = (1 / R - 1) * over
    if ratio == 0 and xg > 8: gr -= 0.25 * (xg - 8)
    return gr

print("\n2) Courbes statiques (seuil fixe, ratio, knee) — niveau constant")
for ratio in (4, 8, 12, 20, 0):
    errs = []
    for lvl in range(-40, 7, 2):
        a = 10 ** (lvl / 20); x = np.full((int(fs * 3), 2), a, np.float32)
        g = trace(fs, x, P(ratio=ratio, atk=4, rel=7), 512)[-1]
        errs.append(abs(g - theory(lvl, ratio)))
    R, thr, kn = TABLE[ratio]
    lab = "All buttons" if ratio == 0 else f"{ratio}:1"
    check(max(errs) < 0.01, f"{lab:11s} : seuil {thr} dB, knee {kn} dB — écart max {max(errs):.4f} dB")

print("\n3) Temps d'attaque (positions 1 à 7) — échelon de niveau, constante de temps à 63 %")
def time_const(seg_g, t, g0, g1, frac=0.632):
    k = np.argmax((seg_g - g0) / (g1 - g0) >= frac)
    return t[k]
lo, hi = 10 ** (-30 / 20), 10 ** (0 / 20)        # −30 dBFS (pas de compression) → 0 dBFS
for atk in (1, 4, 7):
    ta = 800e-6 * (20 / 800) ** ((atk - 1) / 6)
    n0 = int(0.05 * fs)
    x = np.concatenate([np.full(n0, lo), np.full(int(0.05 * fs), hi)]).astype(np.float32)
    g = trace(fs, np.stack([x, x], 1), P(atk=atk, rel=1), 1)
    seg = g[n0:]; t = (np.arange(len(seg)) + 1) / fs
    tm = time_const(seg, t, 0.0, theory(0, 4))
    check(abs(tm - ta) <= max(0.1 * ta, 1.5 / fs), f"attaque {atk} : {tm * 1e6:6.0f} µs (réglage {ta * 1e6:.0f} µs)")

print("\n4) Temps de relâchement (positions 1 à 7) — dépend du programme : plus lent quand la réduction est forte")
for rel in (1, 4, 7):
    tr = 1.1 * (0.05 / 1.1) ** ((rel - 1) / 6)
    n0 = int(0.3 * fs)
    x = np.concatenate([np.full(n0, 10 ** (-6 / 20)), np.full(int(8 * tr * fs), lo)]).astype(np.float32)
    g = trace(fs, np.stack([x, x], 1), P(atk=4, rel=rel), 1)
    g0 = g[n0 - 1]; seg = g[n0:]; t = (np.arange(len(seg)) + 1) / fs
    tm = time_const(seg, t, g0, 0.0)
    fac0 = 1 + 0.6 * min(1, -g0 / 20)                   # facteur au départ (réduction de {g0:.1f} dB)
    check(tr * 0.95 <= tm <= tr * fac0 * 1.05, f"relâchement {rel} : {tm * 1000:6.0f} ms à 63 % (réglage {tr * 1000:.0f} ms, ×{fac0:.2f} au plus fort de la réduction)")

print("\n5) Couleur du FET : harmoniques paires qui augmentent avec la réduction")
def h2_ratio(prm):
    n = int(fs * 1.0); t = np.arange(n) / fs; s = (0.5 * np.sin(2 * np.pi * 1000 * t)).astype(np.float32)
    _, y = run_both(fs, np.stack([s, s], 1), prm, 512)
    seg = y[int(0.5 * fs):, 0] * np.hanning(n - int(0.5 * fs))
    sp = np.abs(np.fft.rfft(seg)); f = np.fft.rfftfreq(len(seg), 1 / fs)
    a1 = sp[np.argmin(np.abs(f - 1000))]; a2 = sp[np.argmin(np.abs(f - 2000))]
    return 20 * np.log10(a2 / a1)
h_light, h_deep, h_all = h2_ratio(P(inDb=-10)), h2_ratio(P(inDb=20)), h2_ratio(P(inDb=20, ratio=0))
check(h_light < h_deep < h_all, f"2e harmonique : {h_light:.0f} dB (peu de réduction) < {h_deep:.0f} dB (forte réduction) < {h_all:.0f} dB (All buttons)")

print("\nRÉSULTAT :", "tout est conforme" if OK else "ÉCHECS ci-dessus")
sys.exit(0 if OK else 1)
