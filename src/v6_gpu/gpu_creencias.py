"""
gpu_creencias.py — Propagación de creencias con TODAS las casillas a la vez (arreglos), para la tarjeta de video.
Mismo método que creencias.py (mensajes por colores + balance de Sinkhorn), pero vectorizado:
corre con CuPy en la tarjeta (--dispositivo gpu) o con NumPy en el procesador (--dispositivo cpu).

    python gpu_creencias.py TABLERO.txt [-P F,C,K,G ...] [--rondas 150] [--sol SOL.txt] [--salida cr.txt] [--dispositivo gpu]
"""
import sys, time, argparse
import numpy as np

def rot(p, k):
    for _ in range(k % 4): p = (p[3], p[0], p[1], p[2])
    return p
def canon(p): return min(rot(p, k) for k in range(4))

def leer(ruta):
    v = [int(x) for l in open(ruta, encoding='utf-8', errors='replace') if not l.lstrip().startswith('#') for x in l.split()]
    nn = len(v) // 4; n = int(round(nn ** 0.5))
    return n, [tuple(v[4 * i:4 * i + 4]) for i in range(nn)]

def preparar(n, P, fijas):
    """Opciones de todas las casillas en arreglos planos: casilla, tipo y colores de cada opción."""
    tipos = {}
    for p in P: t = canon(p); tipos[t] = tipos.get(t, 0) + 1
    T = list(tipos); tid = {t: i for i, t in enumerate(T)}
    cuenta = np.array([tipos[t] for t in T], np.float64)
    rots = {t: sorted(set(rot(t, k) for k in range(4))) for t in T}
    cel, tip, col = [], [], []
    for r in range(n):
        for c in range(n):
            rc = r * n + c; borde = [r == 0, c == n - 1, r == n - 1, c == 0]
            if (r, c) in fijas:
                q = fijas[(r, c)]; cel.append(rc); tip.append(tid[canon(q)]); col.append(q); continue
            for t in T:
                for q in rots[t]:
                    if all((q[d] == 0) == borde[d] for d in range(4)): cel.append(rc); tip.append(tid[t]); col.append(q)
    return T, cuenta, np.array(cel, np.int64), np.array(tip, np.int64), np.array(col, np.int64)

def propagar(xp, n, cuenta, cel, tip, col, rondas=150, amort=0.5, sexp=0.5, f=None):
    f = f or xp.float32
    NN = n * n; C = int(col.max()) + 1; NT = len(cuenta)
    cel, tip, col = xp.asarray(cel), xp.asarray(tip), xp.asarray(col)
    cuenta = xp.asarray(cuenta, dtype=f)
    # vecino en cada dirección y el lado opuesto por el que recibe
    r = np.arange(NN) // n; c = np.arange(NN) % n
    vec = np.stack([np.where(r > 0, np.arange(NN) - n, -1), np.where(c < n - 1, np.arange(NN) + 1, -1),
                    np.where(r < n - 1, np.arange(NN) + n, -1), np.where(c > 0, np.arange(NN) - 1, -1)], 1)
    tiene = xp.asarray(vec >= 0)                                   # [NN,4]
    destino = np.where(vec >= 0, vec * 4 + (np.arange(4)[None, :] + 2) % 4, 0)   # (casilla vecina, lado por el que entra)
    destino = xp.asarray(destino.reshape(-1))
    entra = xp.full((NN * 4, C), 1.0 / C, dtype=f)                 # mensaje que LLEGA a (casilla, lado)
    entra[~tiene.reshape(-1)] = 1.0                                 # lados hacia afuera: sin mensaje
    peso = xp.ones(NT, dtype=f)
    idx_in = [cel * 4 + d for d in range(4)]                        # fila del mensaje entrante por lado
    for it in range(rondas):
        m = [entra[idx_in[d], col[:, d]] for d in range(4)]
        val = peso[tip] * m[0] * m[1] * m[2] * m[3]
        tot = xp.bincount(cel, weights=val, minlength=NN).astype(f)
        val = val / xp.maximum(tot[cel], 1e-30)
        # mensajes que salen por cada lado: suma por color, sin lo que vino de ese lado
        nuevo = xp.zeros((NN * 4, C), dtype=f)
        for d in range(4):
            w = val / xp.maximum(m[d], 1e-30)
            sal = xp.bincount(cel * C + col[:, d], weights=w, minlength=NN * C).astype(f).reshape(NN, C)
            sal = sal / xp.maximum(sal.sum(1, keepdims=True), 1e-30)
            fil = xp.arange(NN) * 4 + d
            ok = tiene[:, d]
            nuevo[destino.reshape(NN, 4)[ok, d]] = sal[ok]
        llega = tiene.reshape(-1)
        entra[llega] = amort * entra[llega] + (1 - amort) * nuevo[llega]
        uso = xp.bincount(tip, weights=val, minlength=NT).astype(f)
        peso = peso * (cuenta / xp.maximum(uso, 1e-12)) ** sexp
        peso = peso / peso.mean()
    return val

def main():
    ap = argparse.ArgumentParser(); ap.add_argument('tablero'); ap.add_argument('--sol'); ap.add_argument('--salida')
    ap.add_argument('--rondas', type=int, default=150); ap.add_argument('-P', action='append', default=[])
    ap.add_argument('--dispositivo', default='gpu', choices=['gpu', 'cpu'])
    a = ap.parse_args()
    if a.dispositivo == 'gpu':
        import cupy as xp
        print('Tarjeta:', xp.cuda.runtime.getDeviceProperties(0)['name'].decode(), flush=True)
    else: xp = np
    n, P = leer(a.tablero); NN = n * n
    fijas = {}
    for s in a.P:
        for trozo in s.replace(';', ' ').split():
            fi, co, k, g = map(int, trozo.split(',')); fijas[(fi - 1, co - 1)] = rot(P[k - 1], g)
    t0 = time.time()
    T, cuenta, cel, tip, col = preparar(n, P, fijas)
    t1 = time.time()
    val = propagar(xp, n, cuenta, cel, tip, col, a.rondas)
    val = xp.asnumpy(val) if xp is not np else val
    t2 = time.time()
    print(f'{n}x{n}: {len(cel):,} opciones; preparar {t1-t0:.2f} s; {a.rondas} rondas en {t2-t1:.3f} s ({(t2-t1)/a.rondas*1000:.2f} ms por ronda)', flush=True)
    if a.salida:
        with open(a.salida, 'w') as fo:
            for i in range(len(cel)):
                q = col[i]; fo.write(f'{cel[i]} {q[0]} {q[1]} {q[2]} {q[3]} {val[i]:.6g}\n')
    if a.sol:
        _, S0 = leer(a.sol)
        def girar(S, k):
            G = [[S[r * n + c] for c in range(n)] for r in range(n)]
            for _ in range(k): G = [[rot(G[n - 1 - c][r], 1) for c in range(n)] for r in range(n)]
            return [G[r][c] for r in range(n) for c in range(n)]
        S = [Sk for Sk in (girar(S0, k) for k in range(4)) if all(Sk[r * n + c] == q for (r, c), q in fijas.items())][0]
        rangos = []; ac = 0
        for rc in range(NN):
            if (rc // n, rc % n) in fijas: continue
            ii = np.nonzero(cel == rc)[0]; b = val[ii]
            j = [x for x in ii if tuple(col[x]) == S[rc]]
            rk = int((b > val[j[0]]).sum()) + 1 if j else len(ii)
            rangos.append(rk); ac += rk == 1
        print(f'casillas libres: {len(rangos)}. Acierta la pieza más probable en {ac}/{len(rangos)}')
        print(f'  puesto medio de la pieza verdadera: {np.mean(rangos):.1f} (mediana {int(np.median(rangos))})')

if __name__ == '__main__':
    main()
