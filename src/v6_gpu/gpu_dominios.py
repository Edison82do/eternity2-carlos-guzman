"""
gpu_dominios.py — Versión 6: búsqueda exacta CON LA PODA DE LA V4.6, repartida en miles de hilos
(proyecto de Carlos Edison Guzman Marte).
Cada casilla lleva la lista de piezas-con-giro que todavía le caben; siempre se decide la casilla con menos opciones;
al poner una pieza se quita de las demás casillas y se recortan las vecinas; si una pieza que falta ya no cabe en
ningún lado, la rama se corta. Si hay solución la encuentra; si agota todo, es una prueba de que no la hay.

    python gpu_dominios.py TABLERO.txt [-P F,C,K,G ...] [--hilos-gpu 4096] [--limite S] [--parar ARCHIVO]
                           [-o solucion.txt] [--dispositivo gpu|cpu] [--hilos 12]

Salida: líneas de estado (mismo formato que gpu_exacto.py); al final "RESULTADO ..." y, si hay solución, "TABLERO ...".
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

ORDEN = ('OC', 'OP', 'MASK', 'PMASK', 'VEC')

def preparar(n, P, fijas):
    NN = n * n; NP = len(P); C = 1 + max(max(p) for p in P)
    opc = []                                   # (pieza, giro, colores)
    for p in range(NP):
        vistos = set()
        for g in range(4):
            q = rot(P[p], g)
            if q in vistos: continue
            vistos.add(q); opc.append((p, g, q))
    NO = len(opc); NW = (NO + 63) // 64
    if NW > 16: raise SystemExit('Tablero demasiado grande para esta versión (máx. 1024 opciones)')
    OC = np.array([q[0] | (q[1] << 8) | (q[2] << 16) | (q[3] << 24) for p, g, q in opc], np.uint32)
    OP = np.array([p for p, g, q in opc], np.uint16)
    MASK = np.zeros((4, C, NW), np.uint64); PMASK = np.zeros((NP, NW), np.uint64)
    bit = lambda o: np.uint64(1 << (o & 63))
    for o, (p, g, q) in enumerate(opc):
        for d in range(4): MASK[d, q[d], o >> 6] |= bit(o)
        PMASK[p, o >> 6] |= bit(o)
    VEC = np.full((NN, 4), -1, np.int32)
    for r in range(n):
        for c in range(n):
            k = r * n + c
            if r > 0: VEC[k, 0] = k - n
            if c < n - 1: VEC[k, 1] = k + 1
            if r < n - 1: VEC[k, 2] = k + n
            if c > 0: VEC[k, 3] = k - 1
    T = NN * NW + 8 + NW
    raiz = np.zeros(T, np.uint64)
    D = raiz[:NN * NW].reshape(NN, NW); PU = raiz[NN * NW:NN * NW + 4]; US = raiz[NN * NW + 4:NN * NW + 8]
    for k in range(NN):
        borde = [VEC[k, d] < 0 for d in range(4)]
        for o, (p, g, q) in enumerate(opc):
            if all((q[d] == 0) == borde[d] for d in range(4)): D[k, o >> 6] |= bit(o)
    for k, (p, g) in fijas.items():
        q = rot(P[p], g); o = next(i for i, x in enumerate(opc) if x[0] == p and x[2] == q)
        D[:, :] &= ~PMASK[p]
        D[k, :] = 0; D[k, o >> 6] = bit(o)
        PU[k >> 6] |= np.uint64(1 << (k & 63)); US[p >> 6] |= np.uint64(1 << (p & 63))
    for k, (p, g) in fijas.items():
        q = rot(P[p], g)
        for d in range(4):
            k2 = VEC[k, d]
            if k2 >= 0 and k2 not in fijas: D[k2] &= MASK[(d + 2) % 4, q[d]]
    return dict(OC=OC, OP=OP, MASK=np.ascontiguousarray(MASK.reshape(-1)), PMASK=np.ascontiguousarray(PMASK.reshape(-1)),
                VEC=np.ascontiguousarray(VEC.reshape(-1)), n=n, modo=1, NN=NN, NW=NW, NP=NP, C=C, T=T, raiz=raiz, opc=opc)

def cargar_lib():
    so = os.path.join(AQUI, 'dominios_b.dll' if os.name == 'nt' else 'dominios_cpu.so')
    lib = ctypes.CDLL(so)
    lib.prefijos_dom.restype = ctypes.c_longlong; lib.agotar_dom.restype = ctypes.c_longlong
    return lib

def PTR(a): return a.ctypes.data_as(ctypes.c_void_p)
def args_d(D): return [PTR(D[k]) for k in ORDEN] + [ctypes.c_int(D[x]) for x in ('NN', 'NW', 'NP', 'C', 'n', 'modo')] + [ctypes.c_int(1)]

def main():
    ap = argparse.ArgumentParser(); ap.add_argument('tablero'); ap.add_argument('-P', action='append', default=[])
    ap.add_argument('--prefijos', type=int, default=0); ap.add_argument('--hilos-gpu', type=int, default=4096)
    ap.add_argument('--hilos', type=int, default=os.cpu_count())
    ap.add_argument('--modo', type=int, default=13, help='suma: 1 = mapas de filas y columnas, 2 = hasta punto fijo, 4 = interior primero, 8 = mapas perezosos (13 = como la V4.6)')
    ap.add_argument('--limite', type=float, default=0); ap.add_argument('--cada', type=float, default=2.0)
    ap.add_argument('--parar'); ap.add_argument('-o'); ap.add_argument('--dispositivo', default='gpu', choices=['gpu', 'cpu'])
    a = ap.parse_args()
    n, P = leer(a.tablero); NN = n * n
    fijas = {}
    for s in a.P:
        for t in s.replace(';', ' ').split():
            f, c, k, g = map(int, t.split(',')); fijas[(f - 1) * n + (c - 1)] = (k - 1, g & 3)
    t0 = time.time()
    D = preparar(n, P, fijas); D['modo'] = a.modo; T = D['T']; NIV = NN + 2
    lib = cargar_lib(); ad = args_d(D)
    gpu = a.dispositivo == 'gpu'
    if gpu:
        import warnings; warnings.filterwarnings('ignore')
        import cupy as cp
        nombre = cp.cuda.runtime.getDeviceProperties(0)['name'].decode(); H = a.hilos_gpu
    else:
        nombre = f'procesador ({a.hilos} hilos)'; H = a.hilos
    print(f'Tablero {n}x{n}: {NN} casillas, {len(fijas)} piezas fijas, poda tipo V4.6 (dominios), en {nombre}', flush=True)
    sol = None; nodos_tot = 0; hechos = 0; parado = False; maxd = [0]; npref = 0; d0 = 0
    # raíz: elegir la primera casilla
    raiz = D['raiz'].copy(); kk = ctypes.c_int(-1)
    e = lib.elegir_cpu(*ad, PTR(raiz), ctypes.byref(kk)) if (a.modo & 1) == 0 or lib.lineas_cpu(*ad, PTR(raiz)) else -1
    SOLB = np.zeros(NN * D['NW'], np.uint64)
    Bt = np.zeros(NIV * T, np.uint64); St = np.zeros(NIV, np.int32)
    if e == 0: sol = raiz[:NN * D['NW']].copy()
    elif e < 0: PREF = np.zeros(0, np.uint16)
    else:
        meta = a.prefijos or (6 * H if gpu else 300 * H)
        d0 = 1
        while True:
            cnt = lib.prefijos_dom(*ad, PTR(raiz), kk, ctypes.c_int(d0), ctypes.c_longlong(0), None, PTR(Bt), PTR(St), PTR(SOLB))
            if cnt == -2: sol = SOLB.copy(); break
            if cnt == 0 or cnt >= meta or d0 >= NN - len(fijas) - 1 or cnt > 3_000_000: break
            if time.time() - t0 > 8 or d0 >= (NN - len(fijas)) // 2: break      # no gastar mucho en repartir
            d0 += 1
        if sol is None:
            PREF = np.zeros(max(cnt, 1) * d0 * 2, np.uint16)
            cnt = lib.prefijos_dom(*ad, PTR(raiz), kk, ctypes.c_int(d0), ctypes.c_longlong(cnt), PTR(PREF), PTR(Bt), PTR(St), PTR(SOLB))
            npref = int(cnt)
            print(f'Reparto: {npref:,} prefijos de {d0} casillas (preparado en {time.time() - t0:.1f} s)', flush=True)
    maxd[0] = d0
    t1 = time.time(); ult = t1
    def parar():
        if a.limite and time.time() - t1 > a.limite: return True
        if a.parar and os.path.exists(a.parar):
            try: os.remove(a.parar)
            except OSError: pass
            return True
        return False
    libres = NN - len(fijas)
    def estado(ahora):
        v = nodos_tot / max(ahora - t1, 1e-9); frac = hechos / max(npref, 1)
        eta = (ahora - t1) * (1 - frac) / frac if frac > 0 else float('inf')
        print(f'  {ahora - t1:7.1f} s | nodos {nodos_tot:,} ({v / 1e6:.2f} M/s) | prefijos agotados {hechos:,}/{npref:,} '
              f'({100 * frac:.3f}%) | faltan ~{eta / 3600:.2f} h (estimado lineal) | máximo colocado {maxd[0] + len(fijas)} de {NN}', flush=True)
    if sol is None and npref and gpu:
        src = open(os.path.join(AQUI, 'dominios.cu'), encoding='utf-8').read()
        ker = cp.RawKernel(src, 'buscar_dom', options=('-std=c++11',))
        g = {k: cp.asarray(D[k]) for k in ORDEN}
        gR = cp.asarray(raiz); gP = cp.asarray(PREF)
        mem = H * NIV * T * 8
        print(f'Memoria de trabajo en la tarjeta: {mem / 2**30:.2f} GB ({H} hilos)', flush=True)
        BLOQ = cp.zeros(H * NIV * T, cp.uint64); SELS = cp.zeros(H * NIV, cp.int32); ED = cp.full(H, -1, cp.int32)
        SIG = cp.zeros(1, cp.uint64); NOD = cp.zeros(1, cp.uint64); HEC = cp.zeros(1, cp.uint64)
        SOL = cp.zeros(1, cp.int32); gSOLB = cp.zeros(NN * D['NW'], cp.uint64); MAXD = cp.zeros(1, cp.int32)
        hilos = 64; bloques = (H + hilos - 1) // hilos; pres = 200
        while True:
            ta = time.time()
            ker((bloques,), (hilos,), tuple(g[k] for k in ORDEN) + (np.int32(NN), np.int32(D['NW']), np.int32(D['NP']), np.int32(D['C']),
                np.int32(n), np.int32(a.modo), np.int32(H), gR, np.int32(kk.value), gP, np.int32(d0), np.int64(npref), SIG, BLOQ, SELS, ED, np.int32(H), np.int32(NIV),
                np.int64(pres), NOD, HEC, SOL, gSOLB, MAXD))
            cp.cuda.Device().synchronize(); dt = time.time() - ta
            nodos_tot = int(NOD.get()[0]); hechos = int(HEC.get()[0]); maxd[0] = int(MAXD.get()[0])
            if int(SOL.get()[0]): sol = gSOLB.get(); break
            if bool((ED == -2).all()): break
            pres = int(min(max(pres * min(4.0, 0.4 / max(dt, 1e-4)), 20), 5e6))
            ahora = time.time()
            if ahora - ult >= a.cada: estado(ahora); ult = ahora
            if parar(): parado = True; break
    elif sol is None and npref:
        SOL = ctypes.c_int(0); sig = [0]; cand = threading.Lock()
        nod = [ctypes.c_longlong(0) for _ in range(a.hilos)]; mx = [ctypes.c_int(0) for _ in range(a.hilos)]
        hech = [0] * a.hilos; alto = [False]; trozo = max(1, min(16, npref // (a.hilos * 50) or 1))
        def obrero(w):
            B = np.zeros(NIV * T, np.uint64); S = np.zeros(NIV, np.int32); SB = np.zeros(NN * D['NW'], np.uint64)
            while not SOL.value and not alto[0]:
                with cand:
                    i0 = sig[0]; sig[0] = min(npref, i0 + trozo)
                if i0 >= npref: return
                hech[w] += lib.agotar_dom(*ad, PTR(raiz), kk, PTR(PREF), ctypes.c_int(d0), ctypes.c_longlong(i0), ctypes.c_longlong(min(npref, i0 + trozo)),
                                          PTR(B), PTR(S), ctypes.byref(SOL), PTR(SB), ctypes.byref(nod[w]), ctypes.byref(mx[w]))
                if SOL.value == 1 and not alto[0] and SB.any(): SOLB[:] = SB
        hs = [threading.Thread(target=obrero, args=(w,), daemon=True) for w in range(a.hilos)]
        for h in hs: h.start()
        while any(h.is_alive() for h in hs):
            time.sleep(0.05)
            ahora = time.time(); nodos_tot = sum(x.value for x in nod); hechos = sum(hech); maxd[0] = max([d0] + [x.value for x in mx])
            if ahora - ult >= a.cada: estado(ahora); ult = ahora
            if parar(): parado = True; alto[0] = True; break
        if not parado:
            for h in hs: h.join()
        nodos_tot = sum(x.value for x in nod); hechos = sum(hech)
        if SOL.value == 1: sol = SOLB.copy()
    dt = time.time() - t0
    estado(time.time())
    if sol is not None:
        Dm = sol.reshape(NN, D['NW']); opc = D['opc']; tab = []
        for k in range(NN):
            w = next(i for i in range(D['NW']) if Dm[k, i]); o = w * 64 + (int(Dm[k, w]) & -int(Dm[k, w])).bit_length() - 1
            tab.append(opc[o][2])
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
