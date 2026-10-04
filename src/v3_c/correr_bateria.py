"""
Corre la batería de investigación con el motor rápido y verifica cada solución.

    python correr_bateria.py                       (todos los hilos, 300 s por tablero)
    python correr_bateria.py --hilos 1 6 12
    python correr_bateria.py --patron "harris_8x8_7-2*" --limite 120
"""
import argparse
import os
import subprocess
import sys

AQUI = os.path.dirname(os.path.abspath(__file__))
ap = argparse.ArgumentParser()
ap.add_argument('--hilos', type=int, nargs='*', default=[os.cpu_count() or 1])
ap.add_argument('--limite', type=float, default=300)
ap.add_argument('--patron', default='*.txt')
ap.add_argument('--csv', default='resultados_bateria.csv')
args = ap.parse_args()
cmd = [sys.executable, os.path.join(AQUI, 'bench_v3.py'), '--carpeta', 'bateria_investigacion',
       '--patron', args.patron, '--limite', str(args.limite), '--csv', args.csv,
       '--hilos'] + [str(h) for h in args.hilos]
print('Ejecutando:', ' '.join(cmd[1:]))
sys.exit(subprocess.call(cmd, cwd=AQUI))
