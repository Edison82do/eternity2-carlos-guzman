"""
Etapas 1-3 del método de Carlos: esquina fija, grupos, marcos y firmas.

El marco se lee como un collar recorrido en sentido horario desde la esquina de
arriba a la izquierda:
  - orilla normalizada  = (a, d, b): a toca a la anterior, d mira hacia adentro,
                           b toca a la siguiente.
  - esquina normalizada = (a, b).
Regla del collar: el b de cada pieza es igual al a de la siguiente.
"""
import time
from collections import Counter
from itertools import permutations

from nucleo import rotar, tipo_pieza
from control import Detenido


class DemasiadosMarcos(Exception):
    pass


def normalizar_orilla(p):
    while p[0] != 0:
        p = rotar(p)
    return (p[3], p[2], p[1])            # (a=O, d=S, b=E)


def normalizar_esquina(p):
    while not (p[0] == 0 and p[3] == 0):
        p = rotar(p)
    return (p[2], p[1])                  # (a=S, b=E)


def clasificar(piezas):
    esquinas, orillas, interiores = [], [], []
    for p in piezas:
        t = tipo_pieza(p)
        if t == "esquina":
            esquinas.append(normalizar_esquina(p))
        elif t == "orilla":
            orillas.append(normalizar_orilla(p))
        elif t == "interior":
            interiores.append(p)
        else:
            raise ValueError(f"pieza inválida {p}")
    if len(esquinas) != 4:
        raise ValueError(f"hay {len(esquinas)} esquinas, deben ser 4")
    return esquinas, orillas, interiores


def grupos_de_esquinas(esquinas):
    """Paso 1 y 2: la primera esquina queda fija arriba-izquierda; las otras 3 se
    permutan. El set() elimina los grupos repetidos cuando hay esquinas idénticas.
    Orden de cada grupo: (arriba-izq, arriba-der, abajo-der, abajo-izq)."""
    fija = esquinas[0]
    return [(fija,) + g for g in sorted(set(permutations(esquinas[1:])))]


def enumerar_marcos(esq4, orillas, n, ctrl=None, limite_marcos=None, fin_tiempo=None):
    """Genera todos los marcos de un grupo SIN repetidos: se recorre por TIPOS de
    pieza (con su cantidad), así dos piezas iguales nunca producen dos marcos.
    Devuelve (marcos_unicos, firmas) con firmas = {firma: [cantidad_marcos, marco_ejemplo]}."""
    m = n - 2
    total = 4 * (m + 1)
    cuenta = Counter(orillas)
    por_a = {}
    for t in cuenta:
        por_a.setdefault(t[0], []).append(t)
    for lista in por_a.values():
        lista.sort()
    collar = [None] * total
    firmas = {}
    estado = {"marcos": 0, "nodos": 0}

    def emitir():
        f = tuple(collar[k][1] for k in range(total) if k % (m + 1) != 0)
        e = firmas.get(f)
        if e is None:
            firmas[f] = [1, tuple(collar)]
        else:
            e[0] += 1
        estado["marcos"] += 1
        if limite_marcos is not None and estado["marcos"] > limite_marcos:
            raise DemasiadosMarcos(f"más de {limite_marcos} marcos en un grupo")

    def dfs(pos, prev_b):
        estado["nodos"] += 1
        if ctrl is not None:
            ctrl.tick(fase="marcos", collar=list(collar), n=n, nodos=estado["nodos"],
                      marcos=estado["marcos"], firmas=len(firmas))
        elif fin_tiempo is not None and (estado["nodos"] & 4095) == 0 and time.perf_counter() > fin_tiempo:
            raise Detenido("límite de tiempo")
        if pos == total:
            if prev_b == esq4[0][0]:
                emitir()
            return
        if pos % (m + 1) == 0:
            c = esq4[pos // (m + 1)]
            if c[0] != prev_b:
                return
            collar[pos] = c
            dfs(pos + 1, c[1])
            collar[pos] = None
            return
        for t in por_a.get(prev_b, ()):
            if cuenta[t] > 0:
                cuenta[t] -= 1
                collar[pos] = t
                dfs(pos + 1, t[2])
                collar[pos] = None
                cuenta[t] += 1

    collar[0] = esq4[0]
    dfs(1, esq4[0][1])
    return estado["marcos"], firmas, estado["nodos"]


def decidir_fusion(firmas_por_grupo, limite_fusion=200_000, factor_repeticion=1.3):
    """Regla pedida por Carlos: fundir los grupos solo si las firmas juntas son
    manejables o si se repiten mucho entre grupos. Devuelve (fundir, motivo)."""
    suma = sum(len(f) for f in firmas_por_grupo.values())
    union = len(set().union(*[set(f) for f in firmas_por_grupo.values()])) if firmas_por_grupo else 0
    if union == 0:
        return False, "no hay firmas"
    if len(firmas_por_grupo) == 1:
        return False, "solo queda un grupo"
    repeticion = suma / union
    if repeticion >= factor_repeticion:
        return True, f"las firmas se repiten entre grupos ({suma} → {union} únicas, x{repeticion:.2f})"
    if union <= limite_fusion:
        return True, f"{union} firmas en total: cantidad manejable"
    return False, f"{union} firmas y casi no se repiten: se analizan los grupos por separado"


def collar_a_casillas(n):
    """Posición (fila, col) y número de giros horarios de cada índice del collar."""
    m = n - 2
    res = []
    res.append(((0, 0), 0))
    for j in range(1, m + 1):
        res.append(((0, j), 0))
    res.append(((0, n - 1), 1))
    for i in range(1, m + 1):
        res.append(((i, n - 1), 1))
    res.append(((n - 1, n - 1), 2))
    for j in range(m, 0, -1):
        res.append(((n - 1, j), 2))
    res.append(((n - 1, 0), 3))
    for i in range(m, 0, -1):
        res.append(((i, 0), 3))
    return res


def pieza_desde_collar(elem):
    """Convierte un elemento normalizado del collar a pieza (N,E,S,O) con el gris arriba."""
    if len(elem) == 2:                    # esquina (a, b) → gris al N y al O
        a, b = elem
        return (0, b, a, 0)
    a, d, b = elem                        # orilla → gris al N
    return (0, b, d, a)


def colocar_marco(n, collar, grid):
    for k, ((r, c), giros) in enumerate(collar_a_casillas(n)):
        if collar[k] is not None:
            grid[r * n + c] = rotar(pieza_desde_collar(collar[k]), giros)
