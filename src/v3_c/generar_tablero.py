"""Genera un tablero aleatorio resuelto de N x N (formato del Editor). e2marcos lo mezcla con -s.
    python generar_tablero.py 8 8 1 tablero_8x8.txt      (N, colores, semilla, archivo)
"""
import os, sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'solucionador_marcos_v2'))
from nucleo import generar, escribir_tablero
if len(sys.argv) != 5:
    sys.exit(__doc__)
n, c, s, ruta = int(sys.argv[1]), int(sys.argv[2]), int(sys.argv[3]), sys.argv[4]
grid, _ = generar(n, c, semilla=s)
escribir_tablero(ruta, n, grid)
print(f"Tablero {n}x{n} con {c} colores guardado en {ruta}")
