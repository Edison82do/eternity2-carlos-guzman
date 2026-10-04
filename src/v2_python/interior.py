"""
Versión 2 — búsqueda del interior con FIRMAS POR LADO.

Igual que la versión 1 (casilla más restringida primero, comprobación hacia
adelante, piezas iguales tratadas como un solo tipo), pero:
 - Las firmas vivas se guardan POR LADO: 4 conjuntos de bits pequeños en lugar de
   uno gigante con todos los marcos completos.
 - Cuando la firma de un lado queda decidida (solo queda una), se comprueba que los
   lados decididos se puedan armar con piezas de orilla sin repetir. Si no, se
   retrocede enseguida.
 - Opcional: `semilla_orden` baraja el orden en que se prueban las piezas (para
   lanzar varias búsquedas distintas en paralelo).
"""
import random
import time
from collections import Counter

from nucleo import canonica, rotaciones_distintas
from control import Detenido


class BuscadorInterior:
    def __init__(self, n, interiores, lote, factibilidad, ctrl=None, fin_tiempo=None,
                 semilla_orden=None, reparto=None, prof_reparto=3):
        """lote: lista de (grupo_id, esq4, lados) con lados[s] = {firma: {multi: secuencia}}."""
        self.n = n
        self.m = m = n - 2
        self.ctrl = ctrl
        self.fin_tiempo = fin_tiempo
        self.lote = lote
        self.fac = factibilidad
        self.nodos = 0
        self.comprobaciones = 0
        self.rnd = random.Random(semilla_orden) if semilla_orden is not None else None
        # reparto=(i, P): este proceso solo explora las ramas i, i+P, i+2P... que salen
        # del nivel `prof_reparto`. Todos recorren igual los niveles de arriba, así que
        # entre los P procesos se cubre la búsqueda completa sin repetir trabajo.
        self.reparto = reparto
        self.prof_reparto = prof_reparto
        self.contador_ramas = 0

        # Firmas por lado (unión de todos los grupos del lote)
        self.firmas = []            # firmas[s] = lista de firmas del lado s
        self.grupos_de = []         # grupos_de[s][i] = {g: dict multi->secuencia}
        for s in range(4):
            union = {}
            for g, (_, _, lados) in enumerate(lote):
                for f, multis in lados[s].items():
                    union.setdefault(f, {})[g] = multis
            lista = sorted(union)
            self.firmas.append(lista)
            self.grupos_de.append([union[f] for f in lista])

        cuenta = Counter(canonica(p) for p in interiores)
        self.tipos = sorted(cuenta)
        self.cuenta = [cuenta[t] for t in self.tipos]
        self.orient = []
        for ti, t in enumerate(self.tipos):
            for r in rotaciones_distintas(t):
                self.orient.append((ti, r))
        colores = {c for _, r in self.orient for c in r}
        for s in range(4):
            for f in self.firmas[s]:
                colores.update(f)
        self.C = C = (max(colores) if colores else 0) + 1

        self.lado_color = [[0] * C for _ in range(4)]
        self.mascara_tipo = [0] * len(self.tipos)
        for o, (ti, r) in enumerate(self.orient):
            b = 1 << o
            for s in range(4):
                self.lado_color[s][r[s]] |= b
            self.mascara_tipo[ti] |= b

        # bits[s][q][c] = firmas del lado s con color c en la posición q del lado
        self.bits = [[[0] * C for _ in range(m)] for _ in range(4)]
        for s in range(4):
            for i, f in enumerate(self.firmas[s]):
                for q, c in enumerate(f):
                    self.bits[s][q][c] |= 1 << i

        # casilla -> [(lado de la casilla que mira al marco, lado del marco, posición q)]
        self.toca_marco = []
        for i in range(m):
            for j in range(m):
                t = []
                if i == 0:
                    t.append((0, 0, j))
                if j == m - 1:
                    t.append((1, 1, i))
                if i == m - 1:
                    t.append((2, 2, m - 1 - j))
                if j == 0:
                    t.append((3, 3, m - 1 - i))
                self.toca_marco.append(t)
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
        self.celdas = [None] * (m * m)
        self._cache_fac = {}
        self.eleccion_final = None

    # ------------------------------------------------------------------ utilidades
    def _permitido(self, s, q, vivas_s):
        bq = self.bits[s][q]
        lc = self.lado_color[s]
        acc = 0
        for c in range(self.C):
            if bq[c] & vivas_s:
                acc |= lc[c]
        return acc

    def _factible(self, vivas):
        """Comprueba los lados cuya firma ya quedó decidida (un solo bit vivo)."""
        decididos = []
        for s in range(4):
            v = vivas[s]
            if v and not (v & (v - 1)):
                decididos.append((s, v.bit_length() - 1))
        if not decididos:
            return True
        clave = tuple(decididos)
        r = self._cache_fac.get(clave)
        if r is not None:
            if r is not False and len(decididos) == 4:
                self.eleccion_final = r
            return r is not False
        self.comprobaciones += 1
        ok = False
        guardar = False
        grupos = None
        for s, i in decididos:
            gs = set(self.grupos_de[s][i])
            grupos = gs if grupos is None else grupos & gs
        for g in sorted(grupos or ()):
            opciones = [list(self.grupos_de[s][i][g]) for s, i in decididos]
            eleccion = self.fac.posible(opciones)
            if eleccion is not None:
                ok = True
                guardar = True
                if len(decididos) == 4:
                    self.eleccion_final = (g, [(s, i, mu) for (s, i), mu in zip(decididos, eleccion)])
                    guardar = self.eleccion_final
                break
        self._cache_fac[clave] = guardar
        return ok

    def collar_final(self):
        """Arma el collar (marco) de la solución encontrada."""
        g, partes = self.eleccion_final
        esq4 = self.lote[g][1]
        m = self.m
        collar = []
        seqs = {}
        for s, i, mu in partes:
            seqs[s] = self.grupos_de[s][i][g][mu]
        for s in range(4):
            collar.append(esq4[s])
            collar.extend(seqs[s])
        assert len(collar) == 4 * (m + 1)
        return collar, self.lote[g][0]

    def _collar_ejemplo(self, vivas):
        collar = []
        for s in range(4):
            collar.append(None)
            v = vivas[s]
            if v:
                i = (v & -v).bit_length() - 1
                g0 = next(iter(self.grupos_de[s][i]))
                seq = next(iter(self.grupos_de[s][i][g0].values()))
                collar.extend(seq)
            else:
                collar.extend([None] * self.m)
        return collar

    def _mostrar(self, vivas):
        if self.ctrl is None:
            return
        self.ctrl.tick(fase="interior", n=self.n, nodos=self.nodos,
                       interior=[None if o is None else self.orient[o][1] for o in self.celdas],
                       collar=self._collar_ejemplo(vivas),
                       firmas_vivas=" / ".join(str(bin(v).count("1")) for v in vivas))

    # ------------------------------------------------------------------ búsqueda
    def buscar(self):
        m = self.m
        vivas = tuple((1 << len(self.firmas[s])) - 1 for s in range(4))
        if any(v == 0 for v in vivas):
            return None
        if m == 0:
            return [] if self._factible(vivas) else None
        disponibles = 0
        for ti, c in enumerate(self.cuenta):
            if c > 0:
                disponibles |= self.mascara_tipo[ti]
        cand = []
        for k in range(m * m):
            c = disponibles
            for s_cel, s, q in self.toca_marco[k]:
                c &= self._permitido(s, q, vivas[s])
            cand.append(c)
        return self._rec(vivas, disponibles, m * m, cand)

    def _rec(self, vivas, disponibles, vacias, cand, prof=0):
        self.nodos += 1
        if self.ctrl is not None:
            self._mostrar(vivas)
        elif self.fin_tiempo is not None and (self.nodos & 1023) == 0 and time.perf_counter() > self.fin_tiempo:
            raise Detenido("límite de tiempo")
        if vacias == 0:
            if len([1 for v in vivas if v and not v & (v - 1)]) == 4 and self._factible(vivas):
                return [None if o is None else self.orient[o][1] for o in self.celdas]
            return None

        celdas = self.celdas
        mejor_k, mejor_n = -1, 1 << 30
        union = 0
        for k in range(len(celdas)):
            if celdas[k] is not None:
                continue
            c = cand[k]
            if not c:
                return None
            union |= c
            vec = 0
            for k2, _, _ in self.vecinos[k]:
                if celdas[k2] is not None:
                    vec += 1
            nb = c.bit_count() * 8 - vec
            if nb < mejor_n:
                mejor_k, mejor_n = k, nb
        for ti, cnt in enumerate(self.cuenta):
            if cnt > 0 and not (union & self.mascara_tipo[ti]):
                return None

        k = mejor_k
        c = cand[k]
        opciones = []
        while c:
            b = c & -c
            c ^= b
            opciones.append(b.bit_length() - 1)
        if self.rnd is not None:
            self.rnd.shuffle(opciones)
        lc = self.lado_color
        for o in opciones:
            ti, r = self.orient[o]
            nv = list(vivas)
            for s_cel, s, q in self.toca_marco[k]:
                nv[s] &= self.bits[s][q][r[s_cel]]
            if not all(nv):
                continue
            nv = tuple(nv)
            if nv != vivas and not self._factible(nv):
                continue
            if self.reparto is not None and prof == self.prof_reparto:
                self.contador_ramas += 1
                if self.contador_ramas % self.reparto[1] != self.reparto[0]:
                    continue
            celdas[k] = o
            self.cuenta[ti] -= 1
            nd = disponibles if self.cuenta[ti] > 0 else disponibles & ~self.mascara_tipo[ti]
            hijo = cand[:] if nd == disponibles else [x & nd for x in cand]
            for k2, s2, s1 in self.vecinos[k]:
                if celdas[k2] is None:
                    hijo[k2] &= lc[s2][r[s1]]
            if nv != vivas:
                cambiados = [s for s in range(4) if nv[s] != vivas[s]]
                perm = {}
                for k2 in range(len(celdas)):
                    if celdas[k2] is None:
                        for s_cel, s, q in self.toca_marco[k2]:
                            if s in cambiados:
                                key = (s, q)
                                if key not in perm:
                                    perm[key] = self._permitido(s, q, nv[s])
                                hijo[k2] &= perm[key]
            res = self._rec(nv, nd, vacias - 1, hijo, prof + 1)
            if res is not None:
                return res
            self.cuenta[ti] += 1
            celdas[k] = None
        return None
