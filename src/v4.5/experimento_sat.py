"""Cambio de representación: el tablero como problema SAT (lógica proposicional).
Variables: x[casilla, pieza, giro] = "esta pieza, con este giro, va en esta casilla";
           y[unión, color]          = "esta unión entre dos casillas tiene este color".
Reglas: cada casilla una pieza; cada pieza en una casilla; cada unión un color; cada pieza puesta
fija el color de sus 4 uniones. Igual que en el método de Carlos, una esquina queda fija arriba
a la izquierda (quita las 4 rotaciones del tablero). Lo resuelve CaDiCaL, que aprende de cada fracaso.
"""
import sys, time, itertools
from pysat.formula import CNF, IDPool
from pysat.card import CardEnc, EncType
from pysat.solvers import Solver

def leer(ruta):
    v = []
    for l in open(ruta):
        if l.startswith('#'): continue
        v += [int(x) for x in l.split()]
    n = int(round((len(v) // 4) ** 0.5))
    return n, [tuple(v[4 * i:4 * i + 4]) for i in range(len(v) // 4)]

def rot(p, k):  # p = (N, E, S, O); k giros horarios
    for _ in range(k % 4): p = (p[3], p[0], p[1], p[2])
    return p

def construir(n, piezas, esquina_fija=True):
    pool = IDPool(); cnf = CNF()
    casillas = [(r, c) for r in range(n) for c in range(n)]
    def afuera(r, c):  # lados que dan al borde: N E S O
        return (r == 0, c == n - 1, r == n - 1, c == 0)
    x = {}
    por_casilla = {rc: [] for rc in casillas}; por_pieza = {i: [] for i in range(len(piezas))}
    esquinas = [i for i, p in enumerate(piezas) if sum(1 for q in p if q == 0) == 2]
    fija = esquinas[0]
    for (r, c) in casillas:
        af = afuera(r, c)
        for i, p in enumerate(piezas):
            vistos = set()
            for k in range(4):
                q = rot(p, k)
                if any((q[d] == 0) != af[d] for d in range(4)): continue
                if q in vistos: continue
                vistos.add(q)
                if esquina_fija and (r, c) == (0, 0) and i != fija: continue
                if esquina_fija and i == fija and (r, c) != (0, 0): continue
                var = pool.id(('x', r, c, i, k)); x[(r, c, i, k)] = (var, q)
                por_casilla[(r, c)].append(var); por_pieza[i].append(var)
    for lits in list(por_casilla.values()) + list(por_pieza.values()):
        if not lits: raise SystemExit('Tablero imposible: una casilla o pieza sin opciones')
        enc = CardEnc.equals(lits=lits, bound=1, vpool=pool, encoding=EncType.seqcounter)
        cnf.extend(enc.clauses)
    colores = sorted({q for p in piezas for q in p if q != 0})
    # uniones: horizontales (r,c)-(r,c+1) y verticales (r,c)-(r+1,c)
    union = {}
    for r in range(n):
        for c in range(n):
            if c + 1 < n: union[('h', r, c)] = None
            if r + 1 < n: union[('v', r, c)] = None
    for u in union:
        ys = [pool.id(('y', u, col)) for col in colores]
        cnf.extend(CardEnc.equals(lits=ys, bound=1, vpool=pool, encoding=EncType.seqcounter).clauses)
    for (r, c, i, k), (var, q) in x.items():
        lados = {0: ('v', r - 1, c), 1: ('h', r, c), 2: ('v', r, c), 3: ('h', r, c - 1)}
        for d, u in lados.items():
            if u in union and q[d] != 0:
                cnf.append([-var, pool.id(('y', u, q[d]))])
    return cnf, x

def main():
    ruta = sys.argv[1]; limite = float(sys.argv[2]) if len(sys.argv) > 2 else 600
    solver_nombre = sys.argv[3] if len(sys.argv) > 3 else 'cadical153'
    n, piezas = leer(ruta)
    t0 = time.time(); cnf, x = construir(n, piezas)
    print(f'{ruta}: {n}x{n}, {cnf.nv} variables, {len(cnf.clauses)} cláusulas, armado en {time.time() - t0:.1f} s', flush=True)
    t1 = time.time()
    with Solver(name=solver_nombre, bootstrap_with=cnf.clauses) as s:
        import threading
        timer = threading.Timer(limite, lambda: s.interrupt()); timer.start()
        r = s.solve_limited(expect_interrupt=True)
        timer.cancel()
        dt = time.time() - t1
        st = s.accum_stats()
        if r:
            m = set(l for l in s.get_model() if l > 0)
            tab = {}
            for (rr, cc, i, k), (var, q) in x.items():
                if var in m: tab[(rr, cc)] = q
            ok = all(tab[(rr, cc)][1] == tab[(rr, cc + 1)][3] for rr in range(n) for cc in range(n - 1)) and \
                 all(tab[(rr, cc)][2] == tab[(rr + 1, cc)][0] for rr in range(n - 1) for cc in range(n))
            print(f'RESUELTO en {dt:.1f} s, verificación {"OK" if ok else "FALLÓ"}; {st}')
        elif r is False:
            print(f'SIN SOLUCIÓN (demostrado) en {dt:.1f} s; {st}')
        else:
            print(f'CORTADO por límite ({limite:.0f} s); {st}')

if __name__ == '__main__':
    main()
