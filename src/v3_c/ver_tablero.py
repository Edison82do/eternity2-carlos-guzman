"""
Muestra un tablero (por ejemplo la solución que guarda e2marcos con -o) en una ventana.

    python ver_tablero.py solucion.txt
"""
import os
import sys
import tkinter as tk

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'solucionador_marcos_v2'))
from nucleo import leer_tablero  # noqa: E402
from grafico import color        # noqa: E402


def mostrar(ruta):
    n, grid = leer_tablero(ruta)
    raiz = tk.Tk()
    raiz.title(f"{os.path.basename(ruta)} — {n}x{n}")
    lado = max(20, min(80, 760 // n))
    cv = tk.Canvas(raiz, width=lado * n + 10, height=lado * n + 10, bg="#202020", highlightthickness=0)
    cv.pack()
    for r in range(n):
        for c in range(n):
            p = grid[r * n + c]
            x, y = 5 + c * lado, 5 + r * lado
            cx, cy = x + lado / 2, y + lado / 2
            esq = [(x, y), (x + lado, y), (x + lado, y + lado), (x, y + lado)]
            for s in range(4):
                a, b = esq[s], esq[(s + 1) % 4]
                cv.create_polygon(a[0], a[1], b[0], b[1], cx, cy, fill=color(p[s]), outline="#202020")
            if lado >= 44:
                off = lado * 0.3
                for s, (dx, dy) in enumerate(((0, -off), (off, 0), (0, off), (-off, 0))):
                    cv.create_text(cx + dx, cy + dy, text=str(p[s]), font=("Consolas", max(7, lado // 7)))
    raiz.mainloop()


if __name__ == "__main__":
    if len(sys.argv) < 2:
        sys.exit("Uso: python ver_tablero.py ARCHIVO.txt")
    mostrar(sys.argv[1])
