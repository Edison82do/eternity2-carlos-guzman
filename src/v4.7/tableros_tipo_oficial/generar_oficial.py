"""generar_oficial.py — Tableros con solución conocida "tipo oficial", con las reglas que se ven en el Eternity II:
  1. colores perfectamente equilibrados: cada color de orilla y cada color interior aparece casi el mismo
     número de veces (como mucho 1 de diferencia);
  2. ninguna pieza repetida (contando giros);
  3. ninguna pieza simétrica (que quede igual al girarla 90 o 180 grados).
Guarda el tablero mezclado y girado al azar, y la solución aparte.
    python generar_oficial.py N I B semilla salida_tablero.txt salida_solucion.txt"""
import sys, random

def rot(p, k):
    for _ in range(k % 4): p = (p[3], p[0], p[1], p[2])
    return p
def canon(p): return min(rot(p, k) for k in range(4))

def generar(N, I, B, sem, max_iter=2_000_000):
    rnd = random.Random(sem)
    # uniones: horizontales H[r][c] entre (r,c)-(r,c+1); verticales V[r][c] entre (r,c)-(r+1,c)
    uni = [('h', r, c) for r in range(N) for c in range(N - 1)] + [('v', r, c) for r in range(N - 1) for c in range(N)]
    es_marco = lambda u: (u[0] == 'h' and u[1] in (0, N - 1)) or (u[0] == 'v' and u[2] in (0, N - 1))
    marco = [u for u in uni if es_marco(u)]; inter = [u for u in uni if not es_marco(u)]
    col = {}
    for lista, base, k in ((marco, 1, B), (inter, B + 1, I)):
        bolsa = [base + (i % k) for i in range(len(lista))]; rnd.shuffle(bolsa)   # equilibrado exacto
        for u, c in zip(lista, bolsa): col[u] = c
    def pieza(r, c):
        g = lambda u: col.get(u, 0)
        return (g(('v', r - 1, c)), g(('h', r, c)), g(('v', r, c)), g(('h', r, c - 1)))
    celdas_de = {}
    for u in uni:
        a = (u[1], u[2]); b = (u[1], u[2] + 1) if u[0] == 'h' else (u[1] + 1, u[2])
        celdas_de[u] = (a, b)
    def malas():
        cnt = {}; m = set()
        for r in range(N):
            for c in range(N):
                p = pieza(r, c); t = canon(p); cnt.setdefault(t, []).append((r, c))
                if rot(p, 1) == p or rot(p, 2) == p: m.add((r, c))
        for t, cs in cnt.items():
            if len(cs) > 1: m.update(cs)
        return m
    m = malas(); it = 0
    while m and it < max_iter:
        it += 1
        r, c = rnd.choice(sorted(m))
        # una unión de esa casilla, intercambiada con otra unión de la misma clase (se conserva el equilibrio)
        mias = [u for u in uni if (r, c) in celdas_de[u]]
        u1 = rnd.choice(mias); grupo = marco if es_marco(u1) else inter
        u2 = rnd.choice(grupo)
        if col[u1] == col[u2]: continue
        col[u1], col[u2] = col[u2], col[u1]
        m2 = malas()
        if len(m2) <= len(m) or rnd.random() < 0.05: m = m2
        else: col[u1], col[u2] = col[u2], col[u1]
    if m: raise SystemExit(f'no se logró quitar repetidas/simétricas ({len(m)} casillas)')
    G = [pieza(r, c) for r in range(N) for c in range(N)]
    pz = [rot(p, rnd.randint(0, 3)) for p in G]; rnd.shuffle(pz)
    return G, pz

if __name__ == '__main__':
    N, I, B, sem = map(int, sys.argv[1:5]); f_tab, f_sol = sys.argv[5], sys.argv[6]
    G, pz = generar(N, I, B, sem)
    with open(f_tab, 'w') as f:
        for r in range(N): f.write('   '.join(' '.join(map(str, p)) for p in pz[r * N:(r + 1) * N]) + '\n')
    with open(f_sol, 'w') as f:
        for r in range(N): f.write('   '.join(' '.join(map(str, p)) for p in G[r * N:(r + 1) * N]) + '\n')
    print(f_tab, 'listo')
