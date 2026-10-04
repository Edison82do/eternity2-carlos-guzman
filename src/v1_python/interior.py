"""
Etapa del interior: se llena el hueco de (n-2)x(n-2) SIN orden lineal.

 - Siempre se elige la casilla vacía con MENOS piezas posibles (si alguna tiene una
   sola, es una "combinación obligatoria" y se pone primero).
 - Después de cada pieza se comprueba que ninguna casilla vacía se quede sin opciones
   y que ninguna pieza restante se quede sin lugar; si pasa, se retrocede enseguida.
 - Las casillas pegadas al marco solo aceptan colores que aparezcan en alguna firma
   todavía viva. Las firmas vivas se guardan como bits de un entero (bitset), así que
   aplicar TODAS las restricciones de todos los marcos cuesta una operación AND.
 - Las piezas iguales se tratan como un solo tipo, y las rotaciones repetidas de
   piezas simétricas se eliminan: nunca se exploran dos ramas idénticas.
"""
import time
from collections import Counter

from nucleo import canonica, rotaciones_distintas
from control import Detenido


class BuscadorInterior:
    def __init__(self, n, interiores, firmas_lista, ctrl=None, fin_tiempo=None):
        self.n = n
        self.m = m = n - 2
        self.ctrl = ctrl
        self.fin_tiempo = fin_tiempo
        self.firmas_lista = firmas_lista          # [(firma, [cantidad, marco_ejemplo])]
        self.nodos = 0

        # Tipos de pieza y orientaciones sin repetir
        cuenta = Counter(canonica(p) for p in interiores)
        self.tipos = sorted(cuenta)
        self.cuenta = [cuenta[t] for t in self.tipos]
        self.orient = []                          # (tipo, (N,E,S,O))
        for ti, t in enumerate(self.tipos):
            for r in rotaciones_distintas(t):
                self.orient.append((ti, r))
        colores = {c for _, r in self.orient for c in r}
        for f, _ in firmas_lista:
            colores.update(f)
        self.C = C = (max(colores) if colores else 0) + 1

        # lado_color[s][c] = bits de las orientaciones con color c en el lado s
        self.lado_color = [[0] * C for _ in range(4)]
        self.mascara_tipo = [0] * len(self.tipos)
        for o, (ti, r) in enumerate(self.orient):
            b = 1 << o
            for s in range(4):
                self.lado_color[s][r[s]] |= b
            self.mascara_tipo[ti] |= b

        # bits[p][c] = firmas que tienen el color c en la posición p del collar
        P = 4 * m
        self.bits = [[0] * C for _ in range(P)]
        for i, (f, _) in enumerate(firmas_lista):
            for p, c in enumerate(f):
                self.bits[p][c] |= 1 << i

        # Para cada casilla interior: [(lado, posición_collar)] de los lados que tocan el marco
        self.toca_marco = []
        for i in range(m):
            for j in range(m):
                t = []
                if i == 0:
                    t.append((0, j))
                if j == m - 1:
                    t.append((1, m + i))
                if i == m - 1:
                    t.append((2, 2 * m + (m - 1 - j)))
                if j == 0:
                    t.append((3, 3 * m + (m - 1 - i)))
                self.toca_marco.append(t)
        self.celdas = [None] * (m * m)

    # --------------------------------------------------------------
    def _candidatos(self, k, disponibles, permitido_pos):
        m = self.m
        i, j = divmod(k, m)
        cand = disponibles
        celdas = self.celdas
        lc = self.lado_color
        if i > 0 and celdas[k - m] is not None:
            cand &= lc[0][self.orient[celdas[k - m]][1][2]]
        if i < m - 1 and celdas[k + m] is not None:
            cand &= lc[2][self.orient[celdas[k + m]][1][0]]
        if j > 0 and celdas[k - 1] is not None:
            cand &= lc[3][self.orient[celdas[k - 1]][1][1]]
        if j < m - 1 and celdas[k + 1] is not None:
            cand &= lc[1][self.orient[celdas[k + 1]][1][3]]
        for s, p in self.toca_marco[k]:
            cand &= permitido_pos[s][p]
        return cand

    def _permitidos(self, vivas):
        """Para cada lado y posición del collar: bits de orientaciones cuyo color en
        ese lado aparece en alguna firma viva."""
        C = self.C
        res = [dict() for _ in range(4)]
        for k in range(self.m * self.m):
            if self.celdas[k] is not None:
                continue
            for s, p in self.toca_marco[k]:
                if p in res[s]:
                    continue
                bp = self.bits[p]
                lc = self.lado_color[s]
                acc = 0
                for c in range(C):
                    if bp[c] & vivas:
                        acc |= lc[c]
                res[s][p] = acc
        return res

    def _mostrar(self, vivas):
        if self.ctrl is None:
            return
        idx = (vivas & -vivas).bit_length() - 1 if vivas else -1
        ejemplo = self.firmas_lista[idx][1][1] if idx >= 0 else None
        self.ctrl.tick(fase="interior", n=self.n, nodos=self.nodos,
                       interior=[None if o is None else self.orient[o][1] for o in self.celdas],
                       collar=list(ejemplo) if ejemplo else None,
                       firmas_vivas=bin(vivas).count("1"))

    # --------------------------------------------------------------
    def buscar(self):
        """Devuelve (interior, firma_indice) o None si no hay solución."""
        m = self.m
        todas = (1 << len(self.firmas_lista)) - 1
        if m == 0:
            return ([], 0) if todas else None
        # vecinos[k] = [(k2, lado_de_k2_que_mira_a_k, lado_de_k_que_mira_a_k2)]
        self.vecinos = []
        for k in range(m * m):
            i, j = divmod(k, m)
            v = []
            if i > 0:
                v.append((k - m, 2, 0))
            if i < m - 1:
                v.append((k + m, 0, 2))
            if j > 0:
                v.append((k - 1, 1, 3))
            if j < m - 1:
                v.append((k + 1, 3, 1))
            self.vecinos.append(v)
        disponibles = 0
        for ti, c in enumerate(self.cuenta):
            if c > 0:
                disponibles |= self.mascara_tipo[ti]
        permitido = self._permitidos(todas)
        cand = [self._candidatos(k, disponibles, permitido) for k in range(m * m)]
        return self._rec(todas, disponibles, m * m, cand)

    def _rec(self, vivas, disponibles, vacias, cand):
        """cand[k] = orientaciones posibles para la casilla vacía k (se va
        estrechando de padre a hijo: solo se aplican las restricciones nuevas)."""
        self.nodos += 1
        if self.ctrl is not None:
            self._mostrar(vivas)
        elif self.fin_tiempo is not None and (self.nodos & 1023) == 0 and time.perf_counter() > self.fin_tiempo:
            raise Detenido("límite de tiempo")
        if vacias == 0:
            idx = (vivas & -vivas).bit_length() - 1
            return ([None if o is None else self.orient[o][1] for o in self.celdas], idx)

        celdas = self.celdas
        mejor_k, mejor_n = -1, 1 << 30
        union = 0
        for k in range(len(celdas)):
            if celdas[k] is not None:
                continue
            c = cand[k]
            if not c:
                return None                         # casilla sin opciones: retroceder
            union |= c
            # menos opciones primero; empate: la que tiene más vecinas ya puestas
            vec = 0
            for k2, _, _ in self.vecinos[k]:
                if celdas[k2] is not None:
                    vec += 1
            nb = c.bit_count() * 8 - vec
            if nb < mejor_n:
                mejor_k, mejor_n = k, nb
        # Cada pieza que queda debe caber en alguna casilla
        for ti, cnt in enumerate(self.cuenta):
            if cnt > 0 and not (union & self.mascara_tipo[ti]):
                return None

        k = mejor_k
        c = cand[k]
        lc = self.lado_color
        while c:
            b = c & -c
            c ^= b
            o = b.bit_length() - 1
            ti, r = self.orient[o]
            nv = vivas
            for s, p in self.toca_marco[k]:
                nv &= self.bits[p][r[s]]
            if not nv:
                continue
            celdas[k] = o
            self.cuenta[ti] -= 1
            nd = disponibles if self.cuenta[ti] > 0 else disponibles & ~self.mascara_tipo[ti]
            hijo = cand[:] if nd == disponibles else [x & nd for x in cand]
            for k2, s2, s1 in self.vecinos[k]:
                if celdas[k2] is None:
                    hijo[k2] &= lc[s2][r[s1]]
            if nv != vivas:                         # cambiaron las firmas vivas
                permitido = self._permitidos(nv)
                for k2 in range(len(celdas)):
                    if celdas[k2] is None:
                        for s, p in self.toca_marco[k2]:
                            hijo[k2] &= permitido[s][p]
            res = self._rec(nv, nd, vacias - 1, hijo)
            if res is not None:
                return res
            self.cuenta[ti] += 1
            celdas[k] = None
        return None
