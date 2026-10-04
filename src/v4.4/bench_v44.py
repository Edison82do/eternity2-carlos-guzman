"""Corre e2marcos sobre todos los tableros de tableros/ y verifica cada solución con el
verificador independiente de Python (nucleo.verificar).
    python bench_v44.py --exe e2marcos44.exe --hilos 12 --limite 60
"""
import argparse, csv, glob, os, re, statistics, subprocess, sys, tempfile
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'solucionador_marcos_v2'))
from nucleo import leer_tablero, verificar

ap = argparse.ArgumentParser()
ap.add_argument('--exe', default='e2marcos44.exe' if os.name == 'nt' else './e2marcos44')
ap.add_argument('--hilos', type=int, nargs='*', default=[0])
ap.add_argument('--limite', type=float, default=60)
ap.add_argument('--patron', default='*.txt')
ap.add_argument('--carpeta', default='tableros')
ap.add_argument('--extra', nargs='*', default=[], help='opciones extra para e2marcos, p. ej. --lados')
ap.add_argument('--csv', default='resultados_v44.csv')
args = ap.parse_args()
aqui = os.path.dirname(os.path.abspath(__file__))
filas = []
for ruta in sorted(glob.glob(os.path.join(aqui, args.carpeta, args.patron))):
    nombre = os.path.basename(ruta)[:-4]
    n, piezas = leer_tablero(ruta)
    linea = f'{nombre:>22}:'
    for h in args.hilos:
        sol = os.path.join(tempfile.gettempdir(), f'sol_{os.getpid()}.txt')
        if os.path.exists(sol): os.remove(sol)
        cmd = [args.exe, ruta, '-q', '-l', str(args.limite), '-o', sol] + (['-t', str(h)] if h else []) + [x for e in args.extra for x in e.split()]
        out = subprocess.run(cmd, capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=args.limite + 120).stdout
        m = re.search(r'resuelto=(\d) ms=([\d.]+) marcos=(\d+) nodos=(\d+) grupos=(\d+)/(\d+) hilos=(\d+)', out)
        ok = bool(m) and m.group(1) == '1'
        verif = ''
        if ok:
            ns, sp = leer_tablero(sol)
            v, msg = verificar(ns, sp, piezas)
            verif = 'verificada' if v else 'FALLA: ' + msg
            ok = v
        seg = float(m.group(2)) / 1000 if m else args.limite
        filas.append({'tablero': nombre, 'hilos': m.group(7) if m else h, 'resuelto': ok, 'segundos': seg,
                      'marcos': m.group(3) if m else '', 'nodos': m.group(4) if m else '',
                      'grupos': f'{m.group(5)}/{m.group(6)}' if m else '', 'verificacion': verif})
        linea += f'  [{filas[-1]["hilos"]} hilos] {"OK" if ok else "--"} {seg:8.3f}s'
    print(linea, flush=True)
with open(os.path.join(aqui, args.csv), 'w', newline='', encoding='utf-8') as f:
    w = csv.DictWriter(f, fieldnames=list(filas[0].keys())); w.writeheader(); w.writerows(filas)
print('\nResumen (no resueltos cuentan como el límite):')
for pref in sorted({re.sub(r'_s\d+$', '', f['tablero']) for f in filas}):
    for h in sorted({str(f['hilos']) for f in filas}):
        sel = [f for f in filas if f['tablero'].startswith(pref) and str(f['hilos']) == h]
        if not sel: continue
        ts = [f['segundos'] if f['resuelto'] else args.limite for f in sel]
        print(f'{pref:>10} {h:>3} hilos: {sum(f["resuelto"] for f in sel)}/{len(sel)}  mediana {statistics.median(ts):.3f}s  media {statistics.mean(ts):.3f}s')
