"""Escribe en tableros/ los mismos tableros mezclados que usaron las pruebas de la v1 y la v2."""
import os, sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'solucionador_marcos_v2'))
from nucleo import generar, mezclar, escribir_tablero, leer_tablero
aqui = os.path.dirname(os.path.abspath(__file__))
out = os.path.join(aqui, 'tableros')
os.makedirs(out, exist_ok=True)
for n, c, ns in ((5, 6, 10), (6, 6, 10), (7, 6, 10), (8, 8, 5)):
    for s in range(1, ns + 1):
        grid, _ = generar(n, c, semilla=1000 * n + s)
        escribir_tablero(os.path.join(out, f'gen_{n}x{n}_c{c}_s{s}.txt'), n, mezclar(grid, s))
modelo = os.path.join(aqui, '..', 'solucionador_marcos_v2', 'modelos_editor', 'model_yannick_0706.txt')
n, g = leer_tablero(modelo)
for s in range(1, 6):
    escribir_tablero(os.path.join(out, f'model0706_s{s}.txt'), n, mezclar(g, s))
print(len(os.listdir(out)), 'tableros')
