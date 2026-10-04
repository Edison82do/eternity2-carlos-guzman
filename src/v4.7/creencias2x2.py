"""
creencias2x2.py — Propagación de creencias por BLOQUES de 2x2 (tablero de lado par).
Cada bloque solo admite combinaciones de 4 piezas distintas que encajan entre sí (y con el gris de afuera);
los bloques vecinos se "hablan" por el par de colores de las 2 uniones que comparten.
    python creencias2x2.py TABLERO.txt --sol SOL.txt -P F,C,K,G ... [--rondas 60]
"""
import sys, argparse, time
import numpy as np
sys.path.insert(0, '/home/claude/uniones')
from creencias import leer, rot, canon

def opciones_celda(n, P, r, c, fijas):
    borde = [r == 0, c == n - 1, r == n - 1, c == 0]
    if (r, c) in fijas: return [fijas[(r, c)]]
    out = []
    for k, p in enumerate(P):
        for g in range(4):
            q = rot(p, g)
            if all((q[d] == 0) == borde[d] for d in range(4)): out.append((k, q))
    return sorted(set(out))

def dominio_bloque(n, P, R, C, fijas, fijas_k):
    """Combinaciones (TL, TR, BL, BR) que encajan. Devuelve arrays: piezas [D,4], colores [D,4,4]."""
    cel = [(R, C), (R, C + 1), (R + 1, C), (R + 1, C + 1)]
    op = [opciones_celda(n, P, r, c, fijas) for r, c in cel]
    # quitar piezas fijas usadas en otras casillas
    op = [[x for x in o if x[0] not in fijas_k or (cel[i] in fijas and fijas[cel[i]] == x)] for i, o in enumerate(op)]
    from collections import defaultdict
    tr_por_O = defaultdict(list)
    for x in op[1]: tr_por_O[x[1][3]].append(x)
    bl_por_N = defaultdict(list)
    for x in op[2]: bl_por_N[x[1][0]].append(x)
    br_por_NO = defaultdict(list)
    for x in op[3]: br_por_NO[(x[1][0], x[1][3])].append(x)
    pk, cols = [], []
    for a in op[0]:
        for b in tr_por_O[a[1][1]]:
            if b[0] == a[0]: continue
            for c in bl_por_N[a[1][2]]:
                if c[0] in (a[0], b[0]): continue
                for d in br_por_NO[(b[1][2], c[1][1])]:
                    if d[0] in (a[0], b[0], c[0]): continue
                    pk.append((a[0], b[0], c[0], d[0])); cols.append((a[1], b[1], c[1], d[1]))
    return cel, np.array(pk, dtype=np.int32).reshape(-1, 4), np.array(cols, dtype=np.int16).reshape(-1, 4, 4)

def main():
    ap = argparse.ArgumentParser(); ap.add_argument('tablero'); ap.add_argument('--sol'); ap.add_argument('--rondas', type=int, default=60)
    ap.add_argument('-P', action='append', default=[]); a = ap.parse_args()
    n, P = leer(a.tablero); NN = n * n; assert n % 2 == 0, 'lado par'
    fijas = {}; fijas_k = set()
    for s in a.P:
        f, c, k, g = map(int, s.split(',')); fijas[(f - 1, c - 1)] = (k - 1, rot(P[k - 1], g)); fijas_k.add(k - 1)
    t0 = time.time(); B = n // 2
    bloques = {}
    for R in range(B):
        for C in range(B):
            bloques[(R, C)] = dominio_bloque(n, P, 2 * R, 2 * C, fijas, fijas_k)
    tam = [len(v[1]) for v in bloques.values()]
    print(f'dominios de bloques: total {sum(tam):,} combinaciones (máx {max(tam):,}, mín {min(tam):,}) en {time.time()-t0:.1f} s', flush=True)
    MC = 1 + max(max(p) for p in P); NP = MC * MC
    # colores hacia cada vecino: N = (TL.N, TR.N), E = (TR.E, BR.E), S = (BL.S, BR.S), O = (TL.O, BL.O)  (en orden de recorrido común)
    def par(cols, d):
        if d == 0: return cols[:, 0, 0] * MC + cols[:, 1, 0]
        if d == 1: return cols[:, 1, 1] * MC + cols[:, 3, 1]
        if d == 2: return cols[:, 2, 2] * MC + cols[:, 3, 2]
        return cols[:, 0, 3] * MC + cols[:, 2, 3]
    pares = {k: [par(v[2], d).astype(np.int64) for d in range(4)] for k, v in bloques.items()}
    veci = lambda R, C, d: {0: (R - 1, C), 1: (R, C + 1), 2: (R + 1, C), 3: (R, C - 1)}[d]
    msg = {}
    for (R, C) in bloques:
        for d in range(4):
            o = veci(R, C, d)
            if o in bloques: msg[((R, C), o)] = np.ones(NP) / NP
    peso = np.ones(NN)
    cre = {}
    for it in range(a.rondas):
        nuevo = {}
        for (R, C), (cel, pk, cols) in bloques.items():
            val = peso[pk].prod(axis=1)
            ent = []
            for d in range(4):
                o = veci(R, C, d)
                if o in bloques: m = msg[(o, (R, C))]; ent.append(m); val = val * m[pares[(R, C)][d]]
                else: ent.append(None)
            s = val.sum(); val = val / s if s > 0 else np.ones(len(val)) / max(len(val), 1)
            cre[(R, C)] = val
            for d in range(4):
                o = veci(R, C, d)
                if o not in bloques: continue
                pr = pares[(R, C)][d]
                sal = np.bincount(pr, weights=val / np.maximum(ent[d][pr], 1e-300), minlength=NP)
                sal = sal / max(sal.sum(), 1e-300)
                nuevo[((R, C), o)] = 0.5 * msg[((R, C), o)] + 0.5 * sal
        msg = nuevo
        uso = np.zeros(NN)
        for (R, C), (cel, pk, cols) in bloques.items():
            for j in range(4): np.add.at(uso, pk[:, j], cre[(R, C)])
        peso = peso * (1.0 / np.maximum(uso, 1e-12)) ** 0.5
    print(f'propagación: {time.time()-t0:.1f} s', flush=True)
    if a.sol:
        _, S0 = leer(a.sol)
        def girar(S, k):
            G = [[S[r * n + c] for c in range(n)] for r in range(n)]
            for _ in range(k): G = [[rot(G[n - 1 - c][r], 1) for c in range(n)] for r in range(n)]
            return [G[r][c] for r in range(n) for c in range(n)]
        S = [Sk for Sk in (girar(S0, k) for k in range(4)) if all(Sk[r * n + c] == q for (r, c), (k2, q) in fijas.items())][0]
        rangos = []; aciertos = 0; libres = 0
        for (R, C), (cel, pk, cols) in bloques.items():
            b = cre[(R, C)]
            for j, (r, c) in enumerate(cel):
                if (r, c) in fijas: continue
                libres += 1
                # marginal por pieza con giro en esta casilla
                claves = cols[:, j, 0].astype(np.int64) * MC**3 + cols[:, j, 1] * MC**2 + cols[:, j, 2] * MC + cols[:, j, 3]
                u, inv = np.unique(claves, return_inverse=True)
                marg = np.bincount(inv, weights=b, minlength=len(u))
                q = S[r * n + c]; kq = ((q[0] * MC + q[1]) * MC + q[2]) * MC + q[3]
                pos = np.searchsorted(u, kq)
                if pos < len(u) and u[pos] == kq:
                    rk = int((marg > marg[pos]).sum()) + 1
                else: rk = len(u)
                rangos.append(rk); aciertos += rk == 1
        print(f'casillas libres: {libres}. Acierta la pieza más probable en {aciertos}/{libres}')
        print(f'  puesto medio de la pieza verdadera: {np.mean(rangos):.1f}   (mediana {int(np.median(rangos))})')

if __name__ == '__main__':
    main()
