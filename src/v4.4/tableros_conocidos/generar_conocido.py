"""Genera tableros tipo Harris (I colores interiores, B de orilla) con SOLUCIÓN CONOCIDA.
Guarda el tablero mezclado y girado (lo que ve el motor) y la solución aparte.
    python generar_conocido.py N I B semilla salida_tablero.txt salida_solucion.txt"""
import sys, random
N, I, B, sem = map(int, sys.argv[1:5]); f_tab, f_sol = sys.argv[5], sys.argv[6]
rnd = random.Random(sem)
borde = lambda: rnd.randint(1, B); inter = lambda: rnd.randint(B + 1, B + I)
H = [[0] * (N - 1) for _ in range(N)]   # H[r][c]: unión entre (r,c) y (r,c+1)
V = [[0] * N for _ in range(N - 1)]     # V[r][c]: unión entre (r,c) y (r+1,c)
for r in range(N):
    for c in range(N - 1): H[r][c] = borde() if r in (0, N - 1) else inter()
for r in range(N - 1):
    for c in range(N): V[r][c] = borde() if c in (0, N - 1) else inter()
G = [[(V[r-1][c] if r > 0 else 0, H[r][c] if c < N-1 else 0, V[r][c] if r < N-1 else 0, H[r][c-1] if c > 0 else 0) for c in range(N)] for r in range(N)]
def rot(p, k):
    for _ in range(k % 4): p = (p[3], p[0], p[1], p[2])
    return p
pz = [rot(p, rnd.randint(0, 3)) for row in G for p in row]; rnd.shuffle(pz)
with open(f_tab, 'w') as f:
    for r in range(N): f.write('   '.join(' '.join(map(str, p)) for p in pz[r*N:(r+1)*N]) + '\n')
with open(f_sol, 'w') as f:
    for r in range(N): f.write('   '.join(' '.join(map(str, p)) for p in G[r]) + '\n')
print(f_tab, 'listo')
