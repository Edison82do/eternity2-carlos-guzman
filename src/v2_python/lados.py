"""
Versión 2: FIRMAS POR LADO.

En vez de generar marcos completos (que pueden ser millones), cada lado del marco
se trata por separado: una cadena de (n-2) piezas de orilla que va de una esquina a
la siguiente. Hay muchas menos cadenas por lado que marcos completos.

Lo único que une a los 4 lados es que no pueden repetir piezas. Eso se comprueba:
  1) al preparar: se eliminan las firmas de lado que no forman parte de NINGÚN marco
     completo (poda entre lados);
  2) durante la búsqueda del interior: cada vez que la firma de un lado queda
     decidida, se comprueba que los lados decididos se puedan armar sin repetir piezas.

Estructuras:
  cadenas[(inicio, fin)] = {firma_lado: {multiconjunto: secuencia_ejemplo}}
  multiconjunto = tupla ordenada de (tipo, cantidad) de las piezas usadas.
"""
import time
from collections import Counter

from control import Detenido


def extremos_de_lados(esq4):
    """Colores de inicio y fin de cada lado para un grupo (TL, TR, BR, BL)."""
    return [(esq4[s][1], esq4[(s + 1) % 4][0]) for s in range(4)]


def enumerar_cadenas(inicio, fin, cuenta, m, fin_tiempo=None, contador=None):
    """Todas las cadenas de m piezas de orilla (por tipo, sin repetidos) que empiezan
    tocando el color `inicio` y terminan con el color `fin`.
    Devuelve {firma: {multiconjunto: secuencia}}."""
    por_a = {}
    for t in cuenta:
        por_a.setdefault(t[0], []).append(t)
    usadas = Counter()
    seq = []
    res = {}
    nodos = [0]

    def dfs(prev_b):
        nodos[0] += 1
        if fin_tiempo is not None and (nodos[0] & 4095) == 0 and time.perf_counter() > fin_tiempo:
            raise Detenido("límite de tiempo")
        if len(seq) == m:
            if prev_b == fin:
                firma = tuple(t[1] for t in seq)
                multi = tuple(sorted(usadas.items()))
                res.setdefault(firma, {}).setdefault(multi, tuple(seq))
            return
        for t in por_a.get(prev_b, ()):
            if usadas[t] < cuenta[t]:
                usadas[t] += 1
                seq.append(t)
                dfs(t[2])
                seq.pop()
                usadas[t] -= 1
                if usadas[t] == 0:
                    del usadas[t]

    if m == 0:
        return {(): {(): ()}} if inicio == fin else {}
    dfs(inicio)
    if contador is not None:
        contador["nodos"] = contador.get("nodos", 0) + nodos[0]
    return res


class Factibilidad:
    """Responde: ¿se pueden elegir cadenas para estos lados sin repetir piezas?"""

    def __init__(self, cuenta_orillas):
        self.total = Counter(cuenta_orillas)
        self.cache = {}
        self.consultas = 0

    def posible(self, opciones_por_lado):
        """opciones_por_lado: lista de listas de multiconjuntos (uno por lado a decidir).
        Devuelve la elección (lista de multiconjuntos) o None."""
        self.consultas += 1
        orden = sorted(range(len(opciones_por_lado)), key=lambda i: len(opciones_por_lado[i]))
        restante = Counter(self.total)
        eleccion = [None] * len(opciones_por_lado)

        def dfs(k):
            if k == len(orden):
                return True
            i = orden[k]
            for multi in opciones_por_lado[i]:
                if all(restante[t] >= c for t, c in multi):
                    for t, c in multi:
                        restante[t] -= c
                    eleccion[i] = multi
                    if dfs(k + 1):
                        return True
                    for t, c in multi:
                        restante[t] += c
            return False

        return eleccion if dfs(0) else None


def preparar_grupo(esq4, orillas, n, cache_cadenas, fin_tiempo=None, contador=None):
    """Para un grupo: cadenas de cada lado + poda de firmas de lado que no pueden
    completar ningún marco. Devuelve (lados, info) con
    lados[s] = {firma: {multi: secuencia}} ya podado, o None si el grupo no tiene marcos."""
    m = n - 2
    cuenta = Counter(orillas)
    lados = []
    for (ini, fin) in extremos_de_lados(esq4):
        clave = (ini, fin)
        if clave not in cache_cadenas:
            cache_cadenas[clave] = enumerar_cadenas(ini, fin, cuenta, m, fin_tiempo, contador)
        lados.append(cache_cadenas[clave])
    info = {"cadenas": [sum(len(v) for v in l.values()) for l in lados],
            "firmas_antes": [len(l) for l in lados]}
    if any(not l for l in lados):
        info["firmas_despues"] = [0, 0, 0, 0]
        return None, info

    # Poda entre lados (por parejas): una cadena de un lado sobrevive solo si, para
    # cada uno de los otros 3 lados, existe alguna cadena compatible (sin pasarse de
    # las piezas que hay). Se repite hasta que no cambie nada. La comprobación
    # completa de los 4 lados juntos se hace después, durante la búsqueda del interior.
    podados = [dict(l) for l in lados]
    total = cuenta

    def compatibles(mu, mu2):
        d = dict(mu)
        for t, c in mu2:
            if d.get(t, 0) + c > total[t]:
                return False
        return True

    cambio = True
    while cambio:
        cambio = False
        for s in range(4):
            otros = [[mu for f in podados[s2].values() for mu in f] for s2 in range(4) if s2 != s]
            nuevos = {}
            for firma, multis in podados[s].items():
                if fin_tiempo is not None and time.perf_counter() > fin_tiempo:
                    raise Detenido("límite de tiempo")
                vivos = {mu: seq for mu, seq in multis.items()
                         if all(any(compatibles(mu, mu2) for mu2 in lista) for lista in otros)}
                if vivos:
                    nuevos[firma] = vivos
            if sum(len(v) for v in nuevos.values()) != sum(len(v) for v in podados[s].values()):
                cambio = True
            podados[s] = nuevos
            if not nuevos:
                info["firmas_despues"] = [len(x) for x in podados]
                return None, info
    # Comprobación de que existe al menos un marco completo con piezas sin repetir
    fac = Factibilidad(cuenta)
    if fac.posible([[mu for f in podados[s].values() for mu in f] for s in range(4)]) is None:
        info["firmas_despues"] = [0, 0, 0, 0]
        return None, info
    info["firmas_despues"] = [len(x) for x in podados]
    return podados, info
