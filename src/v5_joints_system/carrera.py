"""
carrera.py — Mezcla de los dos sistemas: lanza a la vez el método por marcos (V4.6, exacto) y el
sistema de uniones (recocido + cierre exacto), repartiendo los hilos. El primero que resuelve gana y
el otro se detiene. La solución se verifica aquí antes de darla por buena.

    python carrera.py TABLERO.txt [--hilos 12] [--limite 600] [-P F,C,K,G ...]

Busca los motores en:
    ..\\solucionador_marcos_v4_6\\e2marcos46.exe   (o _compatible)
    .\\e2uniones.exe                              (o _compatible)
"""
import os, sys, subprocess, time, argparse, threading

AQUI = os.path.dirname(os.path.abspath(__file__))
WIN = os.name == 'nt'

def buscar(*nombres):
    for n in nombres:
        if os.path.exists(n): return n
    return None

def leer_tablero(ruta):
    v = [int(x) for l in open(ruta, encoding='utf-8', errors='replace') if not l.lstrip().startswith('#') for x in l.split()]
    nn = len(v) // 4; n = int(round(nn ** 0.5))
    return n, [tuple(v[4 * i:4 * i + 4]) for i in range(nn)]

def rot(p, k):
    for _ in range(k % 4): p = (p[3], p[0], p[1], p[2])
    return p

def verificar(n, piezas, ruta_sol):
    """La solución debe usar exactamente las piezas del juego, encajar en todas las uniones y tener gris afuera."""
    try: v = [int(x) for l in open(ruta_sol) if not l.lstrip().startswith('#') for x in l.split()]
    except OSError: return False
    if len(v) != 4 * n * n: return False
    S = [tuple(v[4 * i:4 * i + 4]) for i in range(n * n)]
    canon = lambda p: min(rot(p, k) for k in range(4))
    if sorted(canon(p) for p in S) != sorted(canon(p) for p in piezas): return False
    for r in range(n):
        for c in range(n):
            p = S[r * n + c]
            if (r == 0) != (p[0] == 0) or (r == n - 1) != (p[2] == 0) or (c == 0) != (p[3] == 0) or (c == n - 1) != (p[1] == 0): return False
            if c < n - 1 and p[1] != S[r * n + c + 1][3]: return False
            if r < n - 1 and p[2] != S[(r + 1) * n + c][0]: return False
    return True

def main():
    ap = argparse.ArgumentParser(); ap.add_argument('tablero')
    ap.add_argument('--hilos', type=int, default=os.cpu_count() or 4); ap.add_argument('--limite', type=int, default=600)
    ap.add_argument('-P', action='append', default=[])
    a = ap.parse_args()
    ext = '.exe' if WIN else ''
    m46 = os.path.join(AQUI, '..', 'solucionador_marcos_v4_6')
    exe_marcos = buscar(os.path.join(m46, 'e2marcos46' + ext), os.path.join(m46, 'e2marcos46_compatible' + ext))
    exe_uniones = buscar(os.path.join(AQUI, 'e2uniones' + ext), os.path.join(AQUI, 'e2uniones_compatible' + ext))
    if not exe_marcos or not exe_uniones: print('No encontré los dos motores:', exe_marcos, exe_uniones); return 1
    n, piezas = leer_tablero(a.tablero)
    base = os.path.splitext(a.tablero)[0]
    sol_m, sol_u = base + '_solucion_marcos.txt', base + '_solucion_uniones.txt'
    for f in (sol_m, sol_u):
        try: os.remove(f)
        except OSError: pass
    h1 = max(1, a.hilos // 2); h2 = max(1, a.hilos - h1)
    pistas = sum([['-P', p] for p in a.P], [])
    cmd_m = [exe_marcos, a.tablero, '-t', str(h1), '-l', str(a.limite), '-o', sol_m] + pistas
    cmd_u = [exe_uniones, a.tablero, '-t', str(h2), '--limite', str(a.limite), '-o', sol_u] + pistas
    print(f'Carrera en {os.path.basename(a.tablero)} ({n}x{n}): marcos con {h1} hilos, uniones con {h2} hilos, límite {a.limite} s', flush=True)
    t0 = time.time(); flags = subprocess.CREATE_NO_WINDOW if WIN else 0
    procs = {'marcos': subprocess.Popen(cmd_m, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors='replace', creationflags=flags),
             'uniones': subprocess.Popen(cmd_u, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors='replace', creationflags=flags)}
    salidas = {k: [] for k in procs}
    def leer(k):
        for l in procs[k].stdout: salidas[k].append(l.rstrip())
    for k in procs: threading.Thread(target=leer, args=(k,), daemon=True).start()
    ganador = None; ult = t0
    while True:
        time.sleep(0.05)
        for k, p in procs.items():
            if p.poll() is not None and ganador is None:
                time.sleep(0.2)
                ok = any(l.startswith('RESULTADO') and 'resuelto=1' in l for l in salidas[k])
                if ok and verificar(n, piezas, sol_m if k == 'marcos' else sol_u): ganador = k
        vivos = [k for k, p in procs.items() if p.poll() is None]
        if ganador or not vivos: break
        if time.time() - ult >= 10:
            ult = time.time(); print(f'  {ult - t0:6.0f} s  sigue la carrera…', flush=True)
    for p in procs.values():
        if p.poll() is None: p.kill()
    dt = time.time() - t0
    if ganador:
        print(f'GANADOR {ganador} en {dt:.2f} s — solución verificada en {os.path.basename(sol_m if ganador == "marcos" else sol_u)}')
        print(f'RESULTADO resuelto=1 s={dt:.2f} ganador={ganador}')
        return 0
    print(f'RESULTADO resuelto=0 s={dt:.2f} ganador=ninguno'); return 3

if __name__ == '__main__':
    sys.exit(main())
