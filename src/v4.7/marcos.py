"""
marcos.py — ¿Cuántos marcos válidos hay, cuántas "firmas" (colores hacia adentro) distintas, y cuántas pasan
la prueba del anillo siguiente? Estimados de Knuth por muestreo.
    python marcos.py TABLERO.txt [muestras] [semilla]
"""
import sys, random, math
from collections import defaultdict

def rot(p, k):
    for _ in range(k % 4): p = (p[3], p[0], p[1], p[2])
    return p

def leer(ruta):
    v = [int(x) for l in open(ruta) if not l.lstrip().startswith('#') for x in l.split()]
    nn = len(v) // 4; n = int(round(nn ** 0.5))
    return n, [tuple(v[4 * i:4 * i + 4]) for i in range(nn)]

def preparar(n, P):
    esq, ori, inte = [], [], []
    for k, p in enumerate(P):
        z = sum(c == 0 for c in p)
        if z == 2:
            for g in range(4):
                q = rot(p, g)
                if q[0] == 0 and q[3] == 0: esq.append((k, q[2], q[1]))      # (pieza, entra, sale)
        elif z == 1:
            for g in range(4):
                q = rot(p, g)
                if q[0] == 0: ori.append((k, q[3], q[2], q[1]))           # (pieza, entra, hacia adentro, sale)
        else: inte.append((k, p))
    return esq, ori, inte

class Marco:
    def __init__(self, n, P):
        self.n = n; self.L = 4 * (n - 1); self.esq, self.ori, self.inte = preparar(n, P)
        self.es_esq = [i % (n - 1) == 0 for i in range(self.L)]
        self.ori_por_entra = defaultdict(list)
        for o in self.ori: self.ori_por_entra[o[1]].append(o)
        self.esq_por_entra = defaultdict(list)
        for e in self.esq: self.esq_por_entra[e[1]].append(e)

    def muestra(self, rnd, esq0):
        """Un camino al azar (Knuth). Devuelve (peso, firma) o (0, None) si se trabó."""
        usados = {esq0[0]}; sale = esq0[2]; peso = 1.0; firma = []
        for i in range(1, self.L):
            if self.es_esq[i]: cand = [e for e in self.esq_por_entra[sale] if e[0] not in usados]
            else: cand = [o for o in self.ori_por_entra[sale] if o[0] not in usados]
            if i == self.L - 1:   # la última debe cerrar contra la esquina fija
                cand = [o for o in cand if o[3] == esq0[1]]
            if not cand: return 0.0, None
            peso *= len(cand); x = rnd.choice(cand); usados.add(x[0])
            if self.es_esq[i]: sale = x[2]
            else: sale = x[3]; firma.append(x[2])
        return peso, tuple(firma)

    def multiplicidad(self, esq0, firma, tope=10 ** 7):
        """Cuántos marcos (con esa esquina fija) tienen exactamente esa firma."""
        pos_ori = [i for i in range(1, self.L) if not self.es_esq[i]]
        idx = {p: j for j, p in enumerate(pos_ori)}
        cuenta = [0]; usados = {esq0[0]}
        def rec(i, sale):
            if cuenta[0] >= tope: return
            if i == self.L:
                if sale == esq0[1]: cuenta[0] += 1
                return
            if self.es_esq[i]:
                for e in self.esq_por_entra[sale]:
                    if e[0] in usados: continue
                    usados.add(e[0]); rec(i + 1, e[2]); usados.discard(e[0])
            else:
                c = firma[idx[i]]
                for o in self.ori_por_entra[sale]:
                    if o[0] in usados or o[2] != c: continue
                    usados.add(o[0]); rec(i + 1, o[3]); usados.discard(o[0])
        rec(1, esq0[2])
        return cuenta[0]

    def anillo_posible(self, firma, tope=200000):
        """¿Se puede armar el anillo de casillas pegadas al marco, con piezas interiores?"""
        n = self.n; m = n - 2
        if m < 3: return True
        # colores hacia adentro por casilla del borde interior: arriba (de izq a der), derecha, abajo (de der a izq), izquierda
        f = list(firma); k = n - 2
        arriba, der, abajo, izq = f[0:k], f[k:2 * k], f[2 * k:3 * k], f[3 * k:4 * k]
        req = {}
        for c in range(m): req.setdefault((0, c), [None] * 4)[0] = arriba[c]
        for r in range(m): req.setdefault((r, m - 1), [None] * 4)[1] = der[r]
        for c in range(m): req.setdefault((m - 1, m - 1 - c), [None] * 4)[2] = abajo[c]
        for r in range(m): req.setdefault((m - 1 - r, 0), [None] * 4)[3] = izq[r]
        celdas = [(0, c) for c in range(m)] + [(r, m - 1) for r in range(1, m)] + [(m - 1, c) for c in range(m - 2, -1, -1)] + [(r, 0) for r in range(m - 2, 0, -1)]
        opc = []
        for k2, p in self.inte:
            for g in range(4): opc.append((k2, rot(p, g)))
        puesto = {}; usados = set(); nodos = [0]
        def rec(i):
            nodos[0] += 1
            if nodos[0] > tope: return None
            if i == len(celdas): return True
            r, c = celdas[i]; rq = list(req[(r, c)])
            for d, (dr, dc) in enumerate(((-1, 0), (0, 1), (1, 0), (0, -1))):
                o = (r + dr, c + dc)
                if o in puesto: rq[d] = puesto[o][(d + 2) % 4]
            for k2, q in opc:
                if k2 in usados: continue
                if all(rq[d] is None or rq[d] == q[d] for d in range(4)):
                    puesto[(r, c)] = q; usados.add(k2)
                    x = rec(i + 1)
                    del puesto[(r, c)]; usados.discard(k2)
                    if x is None or x: return x
            return False
        return rec(0)

def main():
    ruta = sys.argv[1]; S = int(sys.argv[2]) if len(sys.argv) > 2 else 2000; sem = int(sys.argv[3]) if len(sys.argv) > 3 else 1
    n, P = leer(ruta); M = Marco(n, P); rnd = random.Random(sem)
    esq0 = M.esq[0]
    pesos = []; firmas = []
    for _ in range(S):
        w, f = M.muestra(rnd, esq0); pesos.append(w); firmas.append(f)
    tot = sum(pesos) / S
    print(f'{n}x{n}: {len(M.ori)//1} orillas (con giro), marco de {M.L} casillas. Esquina fija: pieza {esq0[0]+1}')
    print(f'  marcos válidos (con esa esquina arriba a la izquierda): ~{tot:.3g}   (caminos que llegaron: {sum(1 for w in pesos if w)}/{S})')
    # firmas distintas: suma de w / multiplicidad
    sub = [(w, f) for w, f in zip(pesos, firmas) if w][:int(sys.argv[4]) if len(sys.argv) > 4 else 300]
    acc = 0; acc_ok = 0; mults = []; okc = 0; nok = 0
    for w, f in sub:
        mlt = M.multiplicidad(esq0, f); mults.append(mlt)
        acc += w / mlt
        a = M.anillo_posible(f)
        if a is None: nok += 1; a = True
        if a: acc_ok += w / mlt; okc += 1
    fac = len(sub) / S
    exito = sum(1 for w in pesos if w) / S
    print(f'  firmas distintas: ~{acc / len(sub) * exito:.3g}')
    print(f'  marcos por firma (mediana de la muestra): {sorted(mults)[len(mults)//2]}, máx {max(mults)}')
    print(f'  firmas con el anillo siguiente posible: {okc}/{len(sub)} de la muestra (sin decidir en el tope: {nok})  ->  ~{acc_ok / len(sub) * exito:.3g}')

if __name__ == '__main__':
    main()
