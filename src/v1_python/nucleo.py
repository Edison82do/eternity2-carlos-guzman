"""
Núcleo: piezas, rotaciones, lectura/escritura de tableros, generador y verificador.

Una pieza es una tupla (N, E, S, O) de colores. El color 0 es el gris de la orilla.
El formato de archivo es el mismo del Eternity II Editor: una fila del tablero por
línea, 4 números por pieza (N E S O).
"""
import random
from collections import Counter


def rotar(p, k=1):
    """Gira la pieza k veces en sentido horario."""
    for _ in range(k % 4):
        p = (p[3], p[0], p[1], p[2])
    return p


def canonica(p):
    """Forma canónica: la menor de sus 4 rotaciones. Dos piezas son iguales
    (intercambiables) si y solo si tienen la misma forma canónica."""
    return min(rotar(p, k) for k in range(4))


def rotaciones_distintas(p):
    """Las orientaciones distintas de una pieza (una pieza simétrica tiene menos de 4)."""
    vistas = []
    for k in range(4):
        r = rotar(p, k)
        if r not in vistas:
            vistas.append(r)
    return vistas


def tipo_pieza(p):
    g = sum(1 for c in p if c == 0)
    return {2: "esquina", 1: "orilla", 0: "interior"}.get(g, "invalida")


# ---------------------------------------------------------------- archivos

def leer_tablero(ruta):
    """Lee un archivo del Editor y devuelve (n, lista de piezas en orden de lectura)."""
    filas = []
    with open(ruta, encoding="utf-8") as f:
        for linea in f:
            nums = [int(x) for x in linea.split()]
            if nums:
                filas.append(nums)
    piezas = []
    for fila in filas:
        if len(fila) % 4:
            raise ValueError(f"Fila con {len(fila)} números, no es múltiplo de 4")
        for i in range(0, len(fila), 4):
            piezas.append(tuple(fila[i:i + 4]))
    n = int(round(len(piezas) ** 0.5))
    if n * n != len(piezas):
        raise ValueError(f"{len(piezas)} piezas: el tablero no es cuadrado")
    return n, piezas


def escribir_tablero(ruta, n, grid):
    """grid: lista de n*n piezas (fila por fila)."""
    with open(ruta, "w", encoding="utf-8") as f:
        for r in range(n):
            f.write("   ".join(" ".join(str(c) for c in grid[r * n + c]) for c in range(n)) + "\n")


# ---------------------------------------------------------------- generador

def generar(n, colores, colores_borde=None, semilla=None):
    """Genera un tablero resuelto de n x n y devuelve (grid_resuelto, piezas_mezcladas).
    colores: número de colores interiores. colores_borde: colores de las uniones de
    la orilla (si es None se usa la misma paleta, como en los modelos del Editor)."""
    rnd = random.Random(semilla)
    if colores_borde is None:
        paleta_borde = list(range(1, colores + 1))
        paleta_int = paleta_borde
    else:
        paleta_borde = list(range(1, colores_borde + 1))
        paleta_int = list(range(colores_borde + 1, colores_borde + colores + 1))
    # h[r][c]: color del lado entre (r,c) y (r,c+1); v[r][c]: entre (r,c) y (r+1,c)
    h = [[0] * (n - 1) for _ in range(n)]
    v = [[0] * n for _ in range(n - 1)]
    for r in range(n):
        for c in range(n - 1):
            en_borde = r == 0 or r == n - 1
            h[r][c] = rnd.choice(paleta_borde if en_borde else paleta_int)
    for r in range(n - 1):
        for c in range(n):
            en_borde = c == 0 or c == n - 1
            v[r][c] = rnd.choice(paleta_borde if en_borde else paleta_int)
    grid = []
    for r in range(n):
        for c in range(n):
            N = 0 if r == 0 else v[r - 1][c]
            S = 0 if r == n - 1 else v[r][c]
            O = 0 if c == 0 else h[r][c - 1]
            E = 0 if c == n - 1 else h[r][c]
            grid.append((N, E, S, O))
    mezcla = [rotar(p, rnd.randrange(4)) for p in grid]
    rnd.shuffle(mezcla)
    return grid, mezcla


def mezclar(piezas, semilla=None):
    rnd = random.Random(semilla)
    m = [rotar(p, rnd.randrange(4)) for p in piezas]
    rnd.shuffle(m)
    return m


# ---------------------------------------------------------------- verificador

def verificar(n, grid, piezas_originales):
    """Verificador independiente. Devuelve (ok, mensaje).
    Comprueba: todas las casillas llenas, gris exactamente en la orilla exterior,
    todos los lados vecinos iguales y que se usaron exactamente las piezas dadas."""
    if len(grid) != n * n or any(p is None for p in grid):
        return False, "tablero incompleto"
    for r in range(n):
        for c in range(n):
            N, E, S, O = grid[r * n + c]
            if (N == 0) != (r == 0) or (S == 0) != (r == n - 1):
                return False, f"gris mal puesto en ({r},{c})"
            if (O == 0) != (c == 0) or (E == 0) != (c == n - 1):
                return False, f"gris mal puesto en ({r},{c})"
            if c < n - 1 and E != grid[r * n + c + 1][3]:
                return False, f"no encaja ({r},{c})-({r},{c+1})"
            if r < n - 1 and S != grid[(r + 1) * n + c][0]:
                return False, f"no encaja ({r},{c})-({r+1},{c})"
    if Counter(canonica(p) for p in grid) != Counter(canonica(p) for p in piezas_originales):
        return False, "las piezas usadas no son las del juego"
    return True, "OK: solución válida"


def puntuacion(n, grid):
    """Número de uniones interiores que coinciden (como la puntuación del Editor)."""
    total = 0
    for r in range(n):
        for c in range(n):
            p = grid[r * n + c]
            if p is None:
                continue
            if c < n - 1 and grid[r * n + c + 1] is not None and p[1] == grid[r * n + c + 1][3]:
                total += 1
            if r < n - 1 and grid[(r + 1) * n + c] is not None and p[2] == grid[(r + 1) * n + c][0]:
                total += 1
    return total
