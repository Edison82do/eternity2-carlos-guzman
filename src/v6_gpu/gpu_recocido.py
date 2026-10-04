"""
gpu_recocido.py — Versión 6: recocido masivo en la tarjeta de video (proyecto de Carlos Edison Guzman Marte).
Siempre están las piezas REALES; se intercambian y giran para que encajen más uniones (el puntaje del récord).
Miles de tableros a la vez, cada uno a una temperatura de una escalera; los vecinos intercambian temperaturas.

    python gpu_recocido.py TABLERO.txt [-P F,C,K,G ...] [--tableros 4096] [--limite S] [--semilla K]
                           [--tfria 0.3] [--tcaliente 2.0] [--escalones 32] [--pasos 2000] [--cada 1]
                           [--desde-archivo] [--mostrar] [--parar ARCHIVO] [-o solucion.txt] [--dispositivo gpu|cpu]

Salida: una línea de estado cada --cada segundos; con --mostrar, además "TABLERO ..." con el mejor tablero;
al final "RESULTADO ..." y "TABLERO ...". Con -o, guarda el mejor tablero (si está completo, es la solución).
"""
import sys, os, time, argparse, ctypes
import numpy as np

AQUI = os.path.dirname(os.path.abspath(__file__))
if os.name == 'nt':   # preparada desde el arranque: al final se termina el proceso sin descargar librerías (evita que Windows se trabe)
    _k32 = ctypes.WinDLL('kernel32', use_last_error=True)
    _TerminarProceso = _k32.TerminateProcess; _ProcesoActual = _k32.GetCurrentProcess
    _ProcesoActual.restype = ctypes.c_void_p; _TerminarProceso.argtypes = [ctypes.c_void_p, ctypes.c_uint]

def rot(p, k):
    for _ in range(k % 4): p = (p[3], p[0], p[1], p[2])
    return p
def canon(p): return min(rot(p, k) for k in range(4))

def leer(ruta):
    cab = {}; v = []
    for l in open(ruta, encoding='utf-8', errors='replace'):
        if l.lstrip().startswith('#'):
            if ':' in l: k, x = l.lstrip('# ').split(':', 1); cab[k.strip().lower()] = x.strip()
            continue
        v += [int(x) for x in l.split()]
    nn = len(v) // 4; n = int(round(nn ** 0.5))
    if n * n * 4 != len(v): raise SystemExit('El archivo no tiene un tablero cuadrado')
    return n, [tuple(v[4 * i:4 * i + 4]) for i in range(nn)], cab

class Backend:
    def __init__(self, dispositivo):
        self.gpu = dispositivo == 'gpu'
        src = open(os.path.join(AQUI, 'nucleo2.cu'), encoding='utf-8').read()
        if self.gpu:
            import warnings; warnings.filterwarnings('ignore')
            import cupy as cp
            self.cp = cp; self.xp = cp
            self.src = src; self.kernel = cp.RawKernel(src, 'pasos', options=('-std=c++11',))
            self.nombre = cp.cuda.runtime.getDeviceProperties(0)['name'].decode()
        else:
            self.xp = np
            so = os.path.join(AQUI, 'nucleo2_cierre.dll' if os.name == 'nt' else 'nucleo2_cpu.so')
            self.lib = ctypes.CDLL(so); self.nombre = f'procesador ({os.cpu_count()} hilos)'
    def arr(self, a, dtype): return self.xp.asarray(np.array(a, dtype=dtype, copy=True))
    def host(self, a): return self.cp.asnumpy(a) if self.gpu else a
    def cerrar(self, B, NN, NE, n, kmax, radio, limite, d):
        orden = ('col4', 'vec', 'rotfija', 'fija', 'tab', 'punt', 'mej', 'mtab', 'nodos', 'intentos')
        if self.gpu:
            k = self.cp.RawKernel(self.src, 'cerrar', options=('-std=c++11',)) if not hasattr(self, 'kc') else self.kc
            self.kc = k; hilos = 64
            k(((B + hilos - 1) // hilos,), (hilos,), (np.int32(B), np.int32(NN), np.int32(NE), np.int32(n), np.int32(kmax), np.int32(radio),
              np.int64(limite)) + tuple(d[x] for x in orden))
            self.cp.cuda.Stream.null.synchronize()
        else:
            P = lambda a: a.ctypes.data_as(ctypes.c_void_p)
            self.lib.cerrar_cpu(ctypes.c_int(B), ctypes.c_int(NN), ctypes.c_int(NE), ctypes.c_int(n), ctypes.c_int(kmax), ctypes.c_int(radio),
                                ctypes.c_longlong(limite), *[P(d[x]) for x in orden])
    def correr(self, B, NN, NE, K, d):
        orden = ('col4', 'vec', 'rotfija', 'libres', 'nlib', 'tab', 'punt', 'mej', 'mtab', 'temp', 'rng', 'acept')
        if self.gpu:
            fijo = NN * 8 * 4
            hilos = max(32, min(128, ((49152 - fijo) // (NN * 2)) // 32 * 32)); bloques = (B + hilos - 1) // hilos
            self.kernel((bloques,), (hilos,), (np.int32(B), np.int32(NN), np.int32(NE), np.int32(K)) + tuple(d[k] for k in orden),
                        shared_mem=fijo + hilos * NN * 2)
            self.cp.cuda.Stream.null.synchronize()
        else:
            P = lambda a: a.ctypes.data_as(ctypes.c_void_p)
            self.lib.pasos_cpu(ctypes.c_int(B), ctypes.c_int(NN), ctypes.c_int(NE), ctypes.c_int(K), *[P(d[k]) for k in orden])

def main():
    ap = argparse.ArgumentParser(); ap.add_argument('tablero'); ap.add_argument('-P', action='append', default=[])
    ap.add_argument('--tableros', type=int, default=4096); ap.add_argument('--limite', type=float, default=60)
    ap.add_argument('--semilla', type=int, default=1); ap.add_argument('--tfria', type=float, default=0.15); ap.add_argument('--tcaliente', type=float, default=1.0)
    ap.add_argument('--escalones', type=int, default=32); ap.add_argument('--pasos', type=int, default=2000); ap.add_argument('--cada', type=float, default=1.0)
    ap.add_argument('--desde-archivo', action='store_true'); ap.add_argument('--mostrar', action='store_true'); ap.add_argument('--parar')
    ap.add_argument('-o'); ap.add_argument('--dispositivo', default='gpu', choices=['gpu', 'cpu'])
    ap.add_argument('--cierre', type=int, default=-1, help='intentar el cierre exacto cuando falten <= K uniones (-1 = automático, 0 = nunca)')
    ap.add_argument('--cierre-nodos', type=int, default=1000000)
    ap.add_argument('--cierre-en', default='auto', choices=['auto', 'gpu', 'cpu'], help='dónde se hace el cierre exacto (auto = procesador si se puede)')
    ap.add_argument('--cierre-tableros', type=int, default=48, help='cuántos de los mejores tableros intentan el cierre en cada tanda')
    a = ap.parse_args()
    n, P, cab = leer(a.tablero); NN = n * n; NE = 2 * n * (n - 1); NP = NN
    if cab.get('inicio', '') == 'desde-archivo': a.desde_archivo = True
    rnd = np.random.default_rng(a.semilla)
    # geometría
    vec = np.full((NN, 4), -1, np.int32)
    for r in range(n):
        for c in range(n):
            i = r * n + c
            if r > 0: vec[i, 0] = i - n
            if c < n - 1: vec[i, 1] = i + 1
            if r < n - 1: vec[i, 2] = i + n
            if c > 0: vec[i, 3] = i - 1
    clase_c = np.array([(vec[i] < 0).sum() for i in range(NN)])        # 0 interior, 1 orilla, 2 esquina
    clase_p = np.array([sum(x == 0 for x in p) for p in P])
    pc = np.array(P, np.int32).reshape(-1)
    rotfija = np.full((NP, NN), -1, np.int32)
    for p in range(NP):
        if clase_p[p] == 0:
            rotfija[p, clase_c != 0] = -2; continue
        for c in range(NN):
            if clase_c[c] != clase_p[p]: rotfija[p, c] = -2; continue
            for g in range(4):
                q = rot(P[p], g)
                if all((q[d] == 0) == (vec[c, d] < 0) for d in range(4)): rotfija[p, c] = g; break
    # piezas fijas
    fijas = {}
    for s in a.P:
        for t in s.replace(';', ' ').split():
            f, c, k, g = map(int, t.split(',')); fijas[(f - 1) * n + (c - 1)] = (k - 1, g & 3)
    usadas = {k for k, g in fijas.values()}
    libres = np.zeros((3, NN), np.int32); nlib = np.zeros(3, np.int32)
    for c in range(NN):
        if c in fijas: continue
        k = clase_c[c]; libres[k, nlib[k]] = c; nlib[k] += 1
    B = a.tableros
    perm = np.zeros((B, NN), np.int32); giro = np.zeros((B, NN), np.int32)
    for c, (k, g) in fijas.items(): perm[:, c] = k; giro[:, c] = g
    piezas_k = [[p for p in range(NP) if clase_p[p] == k and p not in usadas] for k in range(3)]
    if a.desde_archivo:
        for c in range(NN):
            if c in fijas: continue
            perm[:, c] = c; giro[:, c] = 0
    else:
        for b in range(B):
            for k in range(3):
                cs = libres[k, :nlib[k]]; ps = rnd.permutation(piezas_k[k]); perm[b, cs] = ps
        giro[:] = rnd.integers(0, 4, size=(B, NN))
        for c in range(NN):
            if c in fijas or clase_c[c] == 0: continue
            giro[:, c] = rotfija[perm[:, c], c]
    def puntaje_np(pe, gi):
        col = lambda p, r, d: pc[p * 4 + ((d - r) & 3)]
        s = np.zeros(pe.shape[0], np.int64)
        for r in range(n):
            for c in range(n):
                i = r * n + c
                if c < n - 1: s += col(pe[:, i], gi[:, i], 1) == col(pe[:, i + 1], gi[:, i + 1], 3)
                if r < n - 1: s += col(pe[:, i], gi[:, i], 2) == col(pe[:, i + n], gi[:, i + n], 0)
        return s
    punt = puntaje_np(perm, giro).astype(np.int32)
    # escalera de temperaturas: cada grupo de 'escalones' tableros recorre de fría a caliente
    G = max(2, min(a.escalones, B))
    lv = np.arange(B) % G
    escala = a.tfria * (a.tcaliente / a.tfria) ** (np.arange(G) / (G - 1))
    temp = escala[lv].astype(np.float32)
    be = Backend(a.dispositivo)
    col4 = np.zeros(NP * 4, np.uint32)
    for p in range(NP):
        for g in range(4):
            q = rot(P[p], g); col4[p * 4 + g] = q[0] | (q[1] << 8) | (q[2] << 16) | (q[3] << 24)
    if pc.max() > 255: raise SystemExit('Más de 255 colores: no cabe en la tabla')
    tab = (perm * 4 + giro).astype(np.uint16)
    d = {'col4': be.arr(col4, np.uint32), 'vec': be.arr(vec.reshape(-1), np.int32), 'rotfija': be.arr(rotfija.reshape(-1), np.int8),
         'libres': be.arr(libres.reshape(-1), np.int32), 'nlib': be.arr(nlib, np.int32),
         'tab': be.arr(tab.reshape(-1), np.uint16), 'punt': be.arr(punt, np.int32), 'mej': be.arr(punt, np.int32),
         'mtab': be.arr(tab.reshape(-1), np.uint16), 'temp': be.arr(temp, np.float32),
         'rng': be.arr(rnd.integers(1, 2**63, size=B, dtype=np.uint64), np.uint64), 'acept': be.arr(np.zeros(B, np.int64), np.int64),
         'fija': be.arr(np.array([1 if c in fijas else 0 for c in range(NN)]), np.uint8),
         'nodos': be.arr(np.zeros(B, np.int64), np.int64), 'intentos': be.arr(np.zeros(B, np.int32), np.int32)}
    kmax = a.cierre if a.cierre >= 0 else max(4, NE // 7)
    import threading
    cpu_cierre = None; hilo_cierre = [None, None]; tabla_sol = [None]; intentos_cpu = [0]; nodos_cpu = [0]
    if kmax > 0 and be.gpu and a.cierre_en in ('auto', 'cpu'):
        try:
            bc = Backend('cpu')
            cpu_cierre = {'be': bc, 'col4': col4, 'vec': vec.reshape(-1).astype(np.int32), 'rotfija': rotfija.reshape(-1).astype(np.int8),
                          'fija': np.array([1 if c in fijas else 0 for c in range(NN)], np.uint8)}
        except OSError as e:
            if a.cierre_en == 'cpu': raise
            print('Aviso: no se pudo cargar el cierre en el procesador; se hace en la tarjeta.', e, flush=True)
    # quién está en cada escalón de cada grupo
    ngr = (B + G - 1) // G
    lugar = np.full((ngr, G), -1, np.int64)
    for b in range(B): lugar[b // G, b % G] = b
    print(f'Tablero {n}x{n}: {NN} casillas, {NE} uniones, {len(fijas)} piezas fijas, {B} tableros en {be.nombre}, '
          f'temperaturas {a.tfria}–{a.tcaliente} en {G} escalones{", desde el archivo" if a.desde_archivo else ""}.', flush=True)
    t0 = time.time(); ult = t0; pasos_tot = 0; inter = 0; resuelto = False; mejor_global = int(punt.max())
    while True:
        be.correr(B, NN, NE, a.pasos, d); pasos_tot += B * a.pasos
        if kmax > 0:
            if cpu_cierre is None:
                be.cerrar(B, NN, NE, n, kmax, 1 + (inter % 2), a.cierre_nodos, d)
            else:   # los mejores tableros se copian al procesador, que intenta el cierre mientras la tarjeta sigue
                if hilo_cierre[0] is None or not hilo_cierre[0].is_alive():
                    if hilo_cierre[1] is not None:      # recoger el resultado anterior
                        hd = hilo_cierre[1]
                        if (hd['punt'] == NE).any():
                            k0 = int(np.nonzero(hd['punt'] == NE)[0][0]); mejor_global = NE
                            tabla_sol[0] = hd['tab'][k0 * NN:(k0 + 1) * NN].copy()
                        intentos_cpu[0] += int(hd['intentos'].sum()); nodos_cpu[0] += int(hd['nodos'].sum())
                        hilo_cierre[1] = None
                        if tabla_sol[0] is not None: resuelto = True; break
                    pu0 = be.host(d['punt']); cand = np.argsort(-pu0)[:a.cierre_tableros]; cand = cand[pu0[cand] >= NE - kmax]
                    if len(cand):
                        tb_all = be.host(d['tab']).reshape(B, NN)
                        hd = {k: np.array(cpu_cierre[k], copy=True) for k in ('col4', 'vec', 'rotfija', 'fija')}
                        hd['tab'] = np.ascontiguousarray(tb_all[cand].reshape(-1)).astype(np.uint16); m = len(cand)
                        hd['punt'] = pu0[cand].astype(np.int32).copy(); hd['mej'] = hd['punt'].copy(); hd['mtab'] = hd['tab'].copy()
                        hd['nodos'] = np.zeros(m, np.int64); hd['intentos'] = np.zeros(m, np.int32)
                        radio = 1 + (inter % 2)
                        def trabajo_cierre(hd=hd, m=m, radio=radio):
                            # reparte los tableros entre varios hilos de Python (la librería es de un solo hilo y libera a Python)
                            nh = max(1, min(os.cpu_count() or 4, m)); partes = np.array_split(np.arange(m), nh); hs = []
                            for idx in partes:
                                if len(idx) == 0: continue
                                sub = {k: hd[k] for k in ('col4', 'vec', 'rotfija', 'fija')}
                                sub['tab'] = np.ascontiguousarray(hd['tab'].reshape(m, NN)[idx].reshape(-1)); sub['mtab'] = sub['tab'].copy()
                                sub['punt'] = hd['punt'][idx].copy(); sub['mej'] = hd['mej'][idx].copy()
                                sub['nodos'] = np.zeros(len(idx), np.int64); sub['intentos'] = np.zeros(len(idx), np.int32)
                                th = threading.Thread(target=cpu_cierre['be'].cerrar, args=(len(idx), NN, NE, n, kmax, radio, a.cierre_nodos, sub))
                                th.start(); hs.append((th, idx, sub))
                            for th, idx, sub in hs:
                                th.join()
                                hd['tab'].reshape(m, NN)[idx] = sub['tab'].reshape(len(idx), NN); hd['punt'][idx] = sub['punt']
                                hd['nodos'][idx] = sub['nodos']; hd['intentos'][idx] = sub['intentos']
                        hilo_cierre[0] = threading.Thread(target=trabajo_cierre, daemon=True)
                        hilo_cierre[1] = hd; hilo_cierre[0].start()
        pu = be.host(d['punt']); mj = be.host(d['mej'])
        mejor_global = max(mejor_global, int(mj.max()))
        if os.environ.get('E2DBG'):
            tb=be.host(d['tab']).reshape(B,NN).astype(np.int64); real=puntaje_np(tb//4,tb%4)
            print('DBG desvío máx', int(np.abs(real-pu).max()), 'mej máx', int(mj.max()), 'real máx', int(real.max()), flush=True)
        if mejor_global == NE: resuelto = True; break
        # intercambio de temperaturas entre escalones vecinos (en cada grupo)
        T = be.host(d['temp']).copy()
        par = (inter % 2)
        for l in range(par, G - 1, 2):
            A = lugar[:, l]; Bb = lugar[:, l + 1]; ok = (A >= 0) & (Bb >= 0)
            A2, B2 = A[ok], Bb[ok]
            x = (1.0 / T[A2] - 1.0 / T[B2]) * (pu[B2] - pu[A2])
            acepta = (x >= 0) | (rnd.random(len(A2)) < np.exp(np.minimum(x, 0)))
            ta = T[A2[acepta]].copy(); T[A2[acepta]] = T[B2[acepta]]; T[B2[acepta]] = ta
            sw = np.nonzero(ok)[0][acepta]
            tmp = lugar[sw, l].copy(); lugar[sw, l] = lugar[sw, l + 1]; lugar[sw, l + 1] = tmp
        inter += 1
        d['temp'] = be.arr(T, np.float32)
        t = time.time()
        if t - ult >= a.cada:
            ult = t
            print(f'  {t - t0:7.1f} s | pasos {pasos_tot:,} ({pasos_tot / (t - t0) / 1e6:.1f} M/s) | mejor ahora: {int(pu.max())}/{NE} | '
                  f'mejor de todos: {mejor_global}/{NE} | tablero frío: {int(pu[lugar[:, 0][lugar[:, 0] >= 0]].max())}/{NE} | cierres {int(be.host(d["intentos"]).sum()) + intentos_cpu[0]:,}', flush=True)
            if a.mostrar:
                b = int(np.argmax(mj)); mt = be.host(d['mtab'])[b * NN:(b + 1) * NN].astype(np.int64); mp = mt // 4; mg = mt % 4
                print('TABLERO ' + ' '.join(' '.join(map(str, rot(P[mp[c]], mg[c]))) for c in range(NN)), flush=True)
        if t - t0 > a.limite: break
        if a.parar and os.path.exists(a.parar):
            try: os.remove(a.parar)
            except OSError: pass
            break
    dt = time.time() - t0
    mj = be.host(d['mej']); b = int(np.argmax(mj))
    mt = be.host(d['mtab'])[b * NN:(b + 1) * NN].astype(np.int64)
    if tabla_sol[0] is not None: mt = tabla_sol[0].astype(np.int64)
    mp = mt // 4; mg = mt % 4
    tab = [rot(P[mp[c]], mg[c]) for c in range(NN)]
    # verificación independiente
    from collections import Counter
    ok_piezas = Counter(canon(p) for p in tab) == Counter(canon(p) for p in P)
    ok_gris = all((tab[r * n + c][d] == 0) == (vec[r * n + c, d] < 0) for r in range(n) for c in range(n) for d in range(4))
    ok_fijas = all(tab[c] == rot(P[k], g) for c, (k, g) in fijas.items())
    encajan = sum(tab[r * n + c][1] == tab[r * n + c + 1][3] for r in range(n) for c in range(n - 1)) + \
              sum(tab[r * n + c][2] == tab[(r + 1) * n + c][0] for r in range(n - 1) for c in range(n))
    resuelto = encajan == NE and ok_piezas and ok_gris and ok_fijas
    print(f'CIERRE intentos={int(be.host(d["intentos"]).sum()) + intentos_cpu[0]} nodos={int(be.host(d["nodos"]).sum()) + nodos_cpu[0]} (cuando faltaban <= {kmax} uniones; en {"el procesador" if (cpu_cierre or not be.gpu) else "la tarjeta"})', flush=True)
    print(f'RESULTADO resuelto={int(resuelto)} s={dt:.2f} pasos={pasos_tot} mejor={encajan}/{NE} tableros={B} '
          f'pasos_por_s={pasos_tot / max(dt, 1e-9):.0f} verificado_piezas={int(ok_piezas)} gris_afuera={int(ok_gris)} fijas={int(ok_fijas)}', flush=True)
    if a.o:
        with open(a.o, 'w') as f:
            for r in range(n): f.write('   '.join(' '.join(map(str, tab[r * n + c])) for c in range(n)) + '\n')
        print(f'{"Solución" if resuelto else "Mejor tablero"} guardado en {a.o}', flush=True)
    print('TABLERO ' + ' '.join(' '.join(map(str, p)) for p in tab), flush=True)
    return 0 if resuelto else 3

if __name__ == '__main__':
    codigo = main()
    sys.stdout.flush(); sys.stderr.flush()
    if os.name == 'nt': _TerminarProceso(_ProcesoActual(), int(codigo))
    os._exit(codigo)     # salir ya, aunque quede un cierre del procesador a medias
