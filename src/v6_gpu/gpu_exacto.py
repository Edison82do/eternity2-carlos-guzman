"""
gpu_exacto.py — Versión 6: búsqueda EXACTA con poda, repartida en miles de hilos (proyecto de Carlos Edison Guzman Marte).
No adivina: recorre el árbol completo de posibilidades (con poda por colores), así que si hay solución la encuentra,
y si termina sin hallarla eso PRUEBA que no la hay (con esas piezas fijas).
El procesador corta el árbol en "prefijos" (tableros a medio armar) y cada hilo agota los suyos.

    python gpu_exacto.py TABLERO.txt [-P F,C,K,G ...] [--orden filas|diagonal|auto] [--prefijos N]
                         [--hilos-gpu 16384] [--limite S] [--parar ARCHIVO] [-o solucion.txt] [--dispositivo gpu|cpu]

Salida: líneas de estado; al final "RESULTADO ..." y, si hay solución, "TABLERO ...".
"""
import sys, os, time, argparse, ctypes, threading
import numpy as np

AQUI = os.path.dirname(os.path.abspath(__file__))
if os.name == 'nt':
    _k32 = ctypes.WinDLL('kernel32', use_last_error=True)
    _TerminarProceso = _k32.TerminateProcess; _ProcesoActual = _k32.GetCurrentProcess
    _ProcesoActual.restype = ctypes.c_void_p; _TerminarProceso.argtypes = [ctypes.c_void_p, ctypes.c_uint]

def rot(p, k):
    for _ in range(k % 4): p = (p[3], p[0], p[1], p[2])
    return p
def canon(p): return min(rot(p, k) for k in range(4))

def leer(ruta):
    v = []
    for l in open(ruta, encoding='utf-8', errors='replace'):
        if l.lstrip().startswith('#'): continue
        v += [int(x) for x in l.split()]
    nn = len(v) // 4; n = int(round(nn ** 0.5))
    if n * n * 4 != len(v): raise SystemExit('El archivo no tiene un tablero cuadrado')
    return n, [tuple(v[4 * i:4 * i + 4]) for i in range(nn)]

def recorrido(n, orden, fijas=()):
    NN = n * n
    if orden == 'voraz':
        out = sorted(fijas); hecho = set(out)
        def vec4(c): return [c - n if c >= n else -1, c + 1 if c % n < n - 1 else -1, c + n if c < NN - n else -1, c - 1 if c % n > 0 else -1]
        while len(out) < NN:
            mejor = None
            for c in range(NN):
                if c in hecho: continue
                v = vec4(c); pun = sum(1 for o in v if o < 0 or o in hecho)
                if pun > 1 or (pun == 1 and not any(o >= 0 and o in hecho for o in v)) or len(out) == len(fijas):
                    if mejor is None or pun > mejor[0]: mejor = (pun, c)
            if mejor is None: mejor = (0, min(c for c in range(NN) if c not in hecho))
            out.append(mejor[1]); hecho.add(mejor[1])
        return out
    if orden == 'diagonal': return sorted(range(NN), key=lambda i: (i // n + i % n, i // n))
    if orden in ('marco', 'espiral'):
        out = []; vistos = set()
        anillos = range((n + 1) // 2) if orden == 'espiral' else [0]
        for k in anillos:
            a, b = k, n - 1 - k
            ring = [(a, c) for c in range(a, b + 1)] + [(r, b) for r in range(a + 1, b + 1)] + \
                   [(b, c) for c in range(b - 1, a - 1, -1)] + [(r, a) for r in range(b - 1, a, -1)]
            for r, c in ring:
                if r * n + c not in vistos: vistos.add(r * n + c); out.append(r * n + c)
        out += [i for i in range(NN) if i not in vistos]
        return out
    return list(range(NN))

def preparar(n, P, fijas, orden):
    """Arma las listas de candidatos. fijas: {casilla: (pieza, giro)}."""
    NN = n * n; C = 1 + max(max(p) for p in P)
    celdas = recorrido(n, orden, fijas)
    pos = {c: i for i, c in enumerate(celdas)}
    col4 = np.zeros(NN * 4, np.uint32)
    for p in range(NN):
        for g in range(4):
            q = rot(P[p], g); col4[p * 4 + g] = q[0] | (q[1] << 8) | (q[2] << 16) | (q[3] << 24)
    usadas = {k for k, g in fijas.values()}
    fx = lambda x: 0 if x == 0 else (2 if x == n - 1 else 1)
    vec = lambda c, d: [c - n if c >= n else -1, c + 1 if c % n < n - 1 else -1, c + n if c < NN - n else -1, c - 1 if c % n > 0 else -1][d]
    clases = {}; listas = []; cls = np.zeros(NN, np.int32); key = np.full(NN * 4, -1, np.int32); chk = np.full(NN * 4, -1, np.int32)
    for i, c in enumerate(celdas):
        r, s_ = c // n, c % n
        conocidos = [d for d in range(4) if vec(c, d) >= 0 and pos[vec(c, d)] < i]
        bordes = [d for d in range(4) if vec(c, d) < 0]
        lados = (conocidos + bordes + [4, 4])[:2]          # 4 = comodín
        dA, dB = lados
        for t, d in enumerate(lados):
            if d < 4 and vec(c, d) >= 0: key[i * 4 + 2 * t] = pos[vec(c, d)]; key[i * 4 + 2 * t + 1] = (d + 2) % 4
            else: key[i * 4 + 2 * t] = -1; key[i * 4 + 2 * t + 1] = 0
        for d in range(4):
            if d in lados: continue
            o = vec(c, d)
            if o < 0: continue
            if pos[o] < i: chk[i * 4 + d] = pos[o]
            elif o in fijas: k, g = fijas[o]; chk[i * 4 + d] = -2 - rot(P[k], g)[(d + 2) % 4]
        cl_key = ('fija', c) if c in fijas else (fx(r), fx(s_), dA, dB)
        if cl_key not in clases:
            clases[cl_key] = len(listas); L = [[] for _ in range(C * C)]; listas.append(L)
            if c in fijas:
                k, g = fijas[c]; q = rot(P[k], g)
                L[(q[dA] if dA < 4 else 0) * C + (q[dB] if dB < 4 else 0)].append(k * 4 + g)
            else:
                borde = [r == 0, s_ == n - 1, r == n - 1, s_ == 0]
                for p in range(NN):
                    if p in usadas: continue
                    vistos = set()
                    for g in range(4):
                        q = rot(P[p], g)
                        if q in vistos: continue
                        vistos.add(q)
                        if all((q[d] == 0) == borde[d] for d in range(4)):
                            L[(q[dA] if dA < 4 else 0) * C + (q[dB] if dB < 4 else 0)].append(p * 4 + g)
        cls[i] = clases[cl_key]
    NCL = len(listas)
    cand = []; tini = np.zeros(NCL * C * C, np.int32); tnum = np.zeros(NCL * C * C, np.int32)
    for cl in range(NCL):
        for kk in range(C * C):
            tini[cl * C * C + kk] = len(cand); tnum[cl * C * C + kk] = len(listas[cl][kk]); cand += listas[cl][kk]
    return dict(CAND=np.array(cand or [0], np.uint16), TINI=tini, TNUM=tnum, CLS=cls, KEY=key, CHK=chk,
                COL4=col4, NN=NN, C=C, celdas=celdas)

ORDEN_DATOS = ('CAND', 'TINI', 'TNUM', 'CLS', 'KEY', 'CHK', 'COL4')

def cargar_lib():
    so = os.path.join(AQUI, 'exacto_c.dll' if os.name == 'nt' else 'exacto_cpu.so')
    lib = ctypes.CDLL(so)
    lib.prefijos.restype = ctypes.c_longlong; lib.agotar_cpu.restype = ctypes.c_longlong
    return lib

def args_datos(D):
    P = lambda a: a.ctypes.data_as(ctypes.c_void_p)
    return [P(D[k]) for k in ORDEN_DATOS] + [ctypes.c_int(D['NN']), ctypes.c_int(D['C'])]

def generar_prefijos(lib, D, meta, maxp):
    """Busca la profundidad más chica con al menos `meta` prefijos (sin pasar de maxp)."""
    nodos = ctypes.c_longlong(0); ad = args_datos(D)
    d = 1; ant = None
    while d < D['NN']:
        n = lib.prefijos(*ad, ctypes.c_int(d), ctypes.c_longlong(0), None, ctypes.byref(nodos))
        if n == 0: return d, np.zeros((0, d), np.uint16)      # ni siquiera hay prefijos: no hay solución
        if n >= meta or n > maxp // 4: break
        d += 1
    if n > maxp:
        d -= 1
    out = np.zeros(max(1, lib.prefijos(*ad, ctypes.c_int(d), ctypes.c_longlong(0), None, ctypes.byref(nodos))) * d, np.uint16)
    n = lib.prefijos(*ad, ctypes.c_int(d), ctypes.c_longlong(len(out) // d), out.ctypes.data_as(ctypes.c_void_p), ctypes.byref(nodos))
    return d, out[:n * d].reshape(n, d)

def main():
    ap = argparse.ArgumentParser(); ap.add_argument('tablero'); ap.add_argument('-P', action='append', default=[])
    ap.add_argument('--orden', default='filas', choices=['filas', 'marco', 'espiral', 'diagonal', 'voraz'])
    ap.add_argument('--prefijos', type=int, default=0, help='cuántos prefijos generar como mínimo (0 = automático)')
    ap.add_argument('--hilos-gpu', type=int, default=16384); ap.add_argument('--nucleo', type=int, default=3, choices=[1, 2, 3]); ap.add_argument('--hilos', type=int, default=os.cpu_count())
    ap.add_argument('--limite', type=float, default=0, help='segundos (0 = sin límite)'); ap.add_argument('--cada', type=float, default=2.0)
    ap.add_argument('--parar'); ap.add_argument('-o'); ap.add_argument('--dispositivo', default='gpu', choices=['gpu', 'cpu'])
    a = ap.parse_args()
    n, P = leer(a.tablero); NN = n * n
    fijas = {}
    for s in a.P:
        for t in s.replace(';', ' ').split():
            f, c, k, g = map(int, t.split(',')); fijas[(f - 1) * n + (c - 1)] = (k - 1, g & 3)
    t0 = time.time()
    D = preparar(n, P, fijas, a.orden)
    lib = cargar_lib()
    gpu = a.dispositivo == 'gpu'
    if gpu:
        import warnings; warnings.filterwarnings('ignore')
        import cupy as cp
        nombre = cp.cuda.runtime.getDeviceProperties(0)['name'].decode()
        H = a.hilos_gpu
    else:
        nombre = f'procesador ({a.hilos} hilos)'; H = a.hilos
    meta = a.prefijos or (8 * H if gpu else 200 * H)
    d0, PREF = generar_prefijos(lib, D, meta, 20_000_000)
    npref = len(PREF)
    print(f'Tablero {n}x{n}: {NN} casillas, {len(fijas)} piezas fijas, orden por {a.orden}, en {nombre}', flush=True)
    print(f'Reparto: {npref:,} prefijos de {d0} casillas (preparado en {time.time() - t0:.1f} s)', flush=True)
    sol = None; nodos_tot = 0; hechos = 0; parado = False; maxd = [d0]
    t1 = time.time(); ult = t1
    def parar():
        if a.limite and time.time() - t1 > a.limite: return True
        if a.parar and os.path.exists(a.parar):
            try: os.remove(a.parar)
            except OSError: pass
            return True
        return False
    def estado(ahora):
        v = nodos_tot / max(ahora - t1, 1e-9)
        frac = hechos / max(npref, 1)
        eta = (ahora - t1) * (1 - frac) / frac if frac > 0 else float('inf')
        print(f'  {ahora - t1:7.1f} s | nodos {nodos_tot:,} ({v / 1e6:.1f} M/s) | prefijos agotados {hechos:,}/{npref:,} '
              f'({100 * frac:.3f}%) | faltan ~{eta / 3600:.2f} h (estimado lineal) | máximo colocado {maxd[0]} de {NN}', flush=True)
    if npref and gpu:
        src = open(os.path.join(AQUI, 'exacto.cu'), encoding='utf-8').read()
        ker = cp.RawKernel(src, ['buscar_gpu', 'buscar_gpu2', 'buscar_gpu3'][a.nucleo - 1], options=('-std=c++11',))
        hilos = 128
        shm = [0, NN * 13 * 4, NN * 13 * 4 + 8 * 4 * hilos][a.nucleo - 1]
        g = {k: cp.asarray(D[k]) for k in ORDEN_DATOS}
        gP = cp.asarray(PREF.reshape(-1))
        SIG = cp.zeros(1, cp.uint64); NOD = cp.zeros(1, cp.uint64); HEC = cp.zeros(1, cp.uint64)
        SOL = cp.zeros(1, cp.int32); SOLTAB = cp.zeros(NN, cp.uint16); MAXD = cp.zeros(1, cp.int32)
        EC = cp.zeros(NN * H, cp.uint16); EK = cp.zeros(NN * H, cp.uint16); EU = cp.zeros(8 * H, cp.uint32)
        ED = cp.full(H, -1, cp.int32)
        bloques = (H + hilos - 1) // hilos
        pres = 2000
        while True:
            ta = time.time()
            ker((bloques,), (hilos,), tuple(g[k] for k in ORDEN_DATOS) + (np.int32(NN), np.int32(D['C']), gP, np.int32(d0), np.int64(npref), SIG,
                EC, EK, EU, ED, np.int32(H), np.int64(pres), NOD, HEC, SOL, SOLTAB, MAXD), shared_mem=shm)
            cp.cuda.Device().synchronize()
            dt = time.time() - ta
            nodos_tot = int(NOD.get()[0]); hechos = int(HEC.get()[0]); maxd[0] = int(MAXD.get()[0])
            if int(SOL.get()[0]): sol = SOLTAB.get().astype(np.int64); break
            if bool((ED == -2).all()): break
            pres = int(min(max(pres * min(4.0, 0.4 / max(dt, 1e-4)), 100), 5e7))   # lanzamientos de ~0.4 s
            ahora = time.time()
            if ahora - ult >= a.cada: estado(ahora); ult = ahora
            if parar(): parado = True; break
    elif npref:
        ad = args_datos(D); gP = np.ascontiguousarray(PREF.reshape(-1))
        SOL = ctypes.c_int(0); SOLTAB = np.zeros(NN, np.uint16)
        sig = [0]; cand = threading.Lock(); nod = [ctypes.c_longlong(0) for _ in range(a.hilos)]
        mx = [ctypes.c_int(0) for _ in range(a.hilos)]; trozo = max(1, min(64, npref // (a.hilos * 50) or 1))
        hech = [0] * a.hilos; alto = [False]
        def obrero(w):
            while not SOL.value and not alto[0]:
                with cand:
                    i0 = sig[0]; sig[0] = min(npref, i0 + trozo)
                if i0 >= npref: return
                hech[w] += lib.agotar_cpu(*ad, gP.ctypes.data_as(ctypes.c_void_p), ctypes.c_int(d0), ctypes.c_longlong(i0),
                                          ctypes.c_longlong(min(npref, i0 + trozo)), ctypes.byref(SOL),
                                          SOLTAB.ctypes.data_as(ctypes.c_void_p), ctypes.byref(nod[w]), ctypes.byref(mx[w]))
        hs = [threading.Thread(target=obrero, args=(w,), daemon=True) for w in range(a.hilos)]
        for h in hs: h.start()
        while any(h.is_alive() for h in hs):
            time.sleep(0.05)
            ahora = time.time(); nodos_tot = sum(x.value for x in nod); hechos = sum(hech); maxd[0] = max(x.value for x in mx)
            if ahora - ult >= a.cada: estado(ahora); ult = ahora
            if parar(): parado = True; alto[0] = True; SOL.value = 2 if not SOL.value else SOL.value; break
        if not parado:
            for h in hs: h.join()
        nodos_tot = sum(x.value for x in nod); hechos = sum(hech)
        if SOL.value == 1: sol = SOLTAB.astype(np.int64)
    dt = time.time() - t0
    estado(time.time())
    if sol is not None:
        celdas = D['celdas']; tab = [None] * NN
        for i, c in enumerate(celdas): tab[c] = rot(P[sol[i] // 4], sol[i] % 4)
        from collections import Counter
        ok_piezas = Counter(canon(p) for p in tab) == Counter(canon(p) for p in P)
        ok_gris = all((tab[r * n + c][0] == 0) == (r == 0) and (tab[r * n + c][2] == 0) == (r == n - 1) and
                      (tab[r * n + c][3] == 0) == (c == 0) and (tab[r * n + c][1] == 0) == (c == n - 1) for r in range(n) for c in range(n))
        ok_fijas = all(tab[c] == rot(P[k], g) for c, (k, g) in fijas.items())
        enc = sum(tab[r * n + c][1] == tab[r * n + c + 1][3] for r in range(n) for c in range(n - 1)) + \
              sum(tab[r * n + c][2] == tab[(r + 1) * n + c][0] for r in range(n - 1) for c in range(n))
        ok = ok_piezas and ok_gris and ok_fijas and enc == 2 * n * (n - 1)
        print(f'RESULTADO resuelto={int(ok)} s={dt:.2f} nodos={nodos_tot} prefijos={npref} agotados={hechos} '
              f'verificado_piezas={int(ok_piezas)} gris_afuera={int(ok_gris)} fijas={int(ok_fijas)} uniones={enc}/{2 * n * (n - 1)}', flush=True)
        if a.o:
            with open(a.o, 'w') as f:
                for r in range(n): f.write('   '.join(' '.join(map(str, tab[r * n + c])) for c in range(n)) + '\n')
            print(f'Solución guardada en {a.o}', flush=True)
        print('TABLERO ' + ' '.join(' '.join(map(str, p)) for p in tab), flush=True)
        return 0 if ok else 4
    if parado:
        print(f'RESULTADO resuelto=0 s={dt:.2f} nodos={nodos_tot} prefijos={npref} agotados={hechos} detenido=1 '
              f'(revisado {100 * hechos / max(npref, 1):.3f}% del árbol)', flush=True)
        return 3
    print(f'RESULTADO resuelto=0 s={dt:.2f} nodos={nodos_tot} prefijos={npref} agotados={hechos} '
          f'SIN_SOLUCION=1 (se agotó todo el árbol: es una prueba de que no hay solución con estas piezas fijas)', flush=True)
    return 2

if __name__ == '__main__':
    codigo = main()
    sys.stdout.flush(); sys.stderr.flush()
    if os.name == 'nt': _TerminarProceso(_ProcesoActual(), int(codigo))
    os._exit(codigo)
