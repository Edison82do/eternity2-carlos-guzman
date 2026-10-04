"""
creencias.py — Propagación de creencias (física estadística) para tableros tipo Eternity.
Cada casilla "conversa" con sus vecinas a través de los colores de sus uniones y se estima, para cada
casilla, qué tan probable es cada pieza (con su giro). Las piezas son únicas: después de cada ronda se
equilibra para que cada pieza sume, entre todas las casillas, sus copias (balance de Sinkhorn).

    python creencias.py TABLERO.txt [--sol SOLUCION.txt] [--rondas 200] [-P F,C,K,G ...]

Con --sol mide cuántas casillas aciertan (la pieza más probable es la de la solución).
"""
import sys, argparse
import numpy as np

def rot(p, k):
    for _ in range(k % 4): p = (p[3], p[0], p[1], p[2])
    return p
def canon(p): return min(rot(p, k) for k in range(4))

def leer(ruta):
    v = [int(x) for l in open(ruta) if not l.lstrip().startswith('#') for x in l.split()]
    nn = len(v) // 4; n = int(round(nn ** 0.5))
    return n, [tuple(v[4 * i:4 * i + 4]) for i in range(nn)]

def preparar(n, P, fijas):
    tipos = {};
    for p in P: t = canon(p); tipos[t] = tipos.get(t, 0) + 1
    T = list(tipos); tid = {t: i for i, t in enumerate(T)}; cuenta = np.array([tipos[t] for t in T], float)
    dom = []      # por casilla: lista de (tipo, colores N E S O)
    for r in range(n):
        for c in range(n):
            borde = [r == 0, c == n - 1, r == n - 1, c == 0]
            if (r, c) in fijas: dom.append([(tid[canon(fijas[(r, c)])], fijas[(r, c)])]); continue
            opc = set()
            for t in T:
                for k in range(4):
                    q = rot(t, k)
                    if all((q[d] == 0) == borde[d] for d in range(4)): opc.add((tid[t], q))
            dom.append(sorted(opc))
    return T, cuenta, dom

def propagar(n, T, cuenta, dom, rondas=200, amort=0.5, sink=True):
    C = 1 + max(max(q) for d in dom for _, q in d)
    NN = n * n
    tip = [np.array([t for t, _ in d]) for d in dom]
    col = [np.array([q for _, q in d]) for d in dom]          # D x 4
    vec = lambda rc, d: {0: rc - n if rc >= n else -1, 1: rc + 1 if rc % n < n - 1 else -1,
                         2: rc + n if rc < NN - n else -1, 3: rc - 1 if rc % n > 0 else -1}[d]
    msg = {}      # (de, a) -> vector sobre colores
    for rc in range(NN):
        for d in range(4):
            o = vec(rc, d)
            if o >= 0: msg[(rc, o)] = np.ones(C) / C
    peso = np.ones(len(T))          # factor de cada tipo (Sinkhorn)
    creencia = [None] * NN
    for it in range(rondas):
        nuevo = {}
        for rc in range(NN):
            entra = []
            val = peso[tip[rc]].copy()
            for d in range(4):
                o = vec(rc, d)
                if o < 0: entra.append(None); continue
                m = msg[(o, rc)]; entra.append(m); val = val * m[col[rc][:, d]]
            s = val.sum()
            if s <= 0: val = np.ones(len(val)); s = val.sum()
            creencia[rc] = val / s
            for d in range(4):
                o = vec(rc, d)
                if o < 0: continue
                m_in = entra[d][col[rc][:, d]]
                sal = np.bincount(col[rc][:, d], weights=val / np.maximum(m_in, 1e-300), minlength=C)
                sal = sal / max(sal.sum(), 1e-300)
                nuevo[(rc, o)] = amort * msg[(rc, o)] + (1 - amort) * sal
        msg = nuevo
        if sink:   # cada tipo debe sumar sus copias entre todas las casillas
            uso = np.zeros(len(T))
            for rc in range(NN): np.add.at(uso, tip[rc], creencia[rc])
            peso = peso * (cuenta / np.maximum(uso, 1e-12)) ** 0.5
    return tip, col, creencia

def main():
    ap = argparse.ArgumentParser(); ap.add_argument('tablero'); ap.add_argument('--sol')
    ap.add_argument('--rondas', type=int, default=200); ap.add_argument('-P', action='append', default=[])
    ap.add_argument('--sin-sinkhorn', action='store_true'); ap.add_argument('--salida')
    a = ap.parse_args()
    n, P = leer(a.tablero); NN = n * n
    fijas = {}
    for s in a.P:
        f, c, k, g = map(int, s.split(',')); fijas[(f - 1, c - 1)] = rot(P[k - 1], g)
    T, cuenta, dom = preparar(n, P, fijas)
    tip, col, cre = propagar(n, T, cuenta, dom, a.rondas, sink=not a.sin_sinkhorn)
    if a.salida:
        with open(a.salida, 'w') as f:
            for rc in range(NN):
                for x in range(len(cre[rc])): q = col[rc][x]; f.write(f'{rc} {q[0]} {q[1]} {q[2]} {q[3]} {cre[rc][x]:.6g}\n')
    if a.sol:
        _, S = leer(a.sol)
        aciertos = 0; seguros = []; rangos = []
        libres = [rc for rc in range(NN) if (rc // n, rc % n) not in fijas]
        for rc in libres:
            b = cre[rc]; i = int(np.argmax(b)); q = tuple(col[rc][i])
            ok = q == S[rc]; aciertos += ok
            seguros.append((b[i], ok))
            # en qué puesto queda la pieza verdadera
            j = [x for x in range(len(b)) if tuple(col[rc][x]) == S[rc]]
            rangos.append(int((b > b[j[0]]).sum()) + 1 if j else -1)
        seguros.sort(reverse=True)
        print(f'casillas libres: {len(libres)}. Acierta la pieza más probable en {aciertos}/{len(libres)}')
        for k in (5, 10, 20):
            print(f'  las {k} casillas más seguras: {sum(ok for _, ok in seguros[:k])}/{k} correctas (prob. mín. {seguros[k-1][0]:.2f})')
        dsz = [len(dom[rc]) for rc in libres]
        print(f'  puesto medio de la pieza verdadera: {np.mean(rangos):.1f} (opciones por casilla: media {np.mean(dsz):.0f})')
    return 0

if __name__ == '__main__':
    sys.exit(main())
