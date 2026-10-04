"""
Solucionador por marcos — VERSIÓN 2 (firmas por lado + varios núcleos opcional).

    python resolver.py --modelo modelos_editor/model_yannick_0706.txt
    python resolver.py --generar 7 --colores 6 --semilla 3 --limite 60
    python resolver.py --generar 8 --colores 8 --semilla 1 --procesos 4
    python resolver.py --modelo modelos_editor/model_yannick_0505.txt --grafico
"""
import argparse
import multiprocessing as mp
import os
import sys
import time
from collections import Counter

from nucleo import leer_tablero, generar, mezclar, verificar, escribir_tablero
from marcos import (clasificar, grupos_de_esquinas, decidir_fusion, colocar_marco,
                    enumerar_marcos, DemasiadosMarcos)
from interior_marcos import BuscadorInterior as BuscadorMarcos
from lados import preparar_grupo, Factibilidad
from interior import BuscadorInterior
from control import Detenido


def resolver(n, piezas, ctrl=None, limite_segundos=None, limite_fusion=200_000,
             semilla_orden=None, reparto=None, metodo="auto", tope_marcos=150_000,
             estrategia="por_grupo", solo_grupos=None, excluir_firmas=None, registro=print, **_):
    """metodo: "auto"   = marcos completos (v1) si salen como máximo `tope_marcos`;
                          si salen más, firmas por lado (v2).
               "marcos" = siempre marcos completos (como la v1).
               "lados"  = siempre firmas por lado.
    estrategia: "fundir"      = todos los grupos a la vez (se funden si conviene).
                "por_grupo"   = idea de Carlos: un grupo completo cada vez (marcos,
                                firmas e interior); si no tiene solución, el siguiente."""
    if estrategia == "por_grupo" and solo_grupos is None:
        return _resolver_por_grupo(n, piezas, ctrl, limite_segundos, limite_fusion, semilla_orden,
                                   reparto, metodo, tope_marcos, registro)
    inf = {"n": n, "resuelto": False, "tiempos": {}, "grupos": [], "version": 2}
    activo = {"buscador": None, "previos": 0}
    t0 = time.perf_counter()
    fin = t0 + limite_segundos if limite_segundos else None
    if ctrl is not None:
        ctrl.t0 = t0
        ctrl.limite = limite_segundos
    try:
        esquinas, orillas, interiores = clasificar(piezas)
        grupos = grupos_de_esquinas(esquinas) if solo_grupos is None else list(solo_grupos)
        t1 = time.perf_counter()
        inf["tiempos"]["grupos"] = t1 - t0
        registro(f"[1] Esquina fija {esquinas[0]}. Grupos distintos: {len(grupos)}")

        if metodo in ("auto", "marcos"):
            tope = tope_marcos if metodo == "auto" else 5_000_000
            inf["fase_actual"] = "marcos"
            if _ruta_marcos(n, piezas, grupos, orillas, interiores, tope, inf, activo, ctrl, fin,
                            limite_fusion, reparto, registro, t1, excluir_firmas):
                inf["tiempos"]["total"] = time.perf_counter() - t0
                if ctrl is not None and inf.get("solucion"):
                    ctrl.tick(fase="fin", n=n, grid=inf["solucion"])
                return inf
            registro(f"[2] Más de {tope:,} marcos completos: se pasa a FIRMAS POR LADO")
            inf["grupos"] = []
            t1 = time.perf_counter()
        inf["metodo_usado"] = "lados"
        inf["fase_actual"] = "lados"

        # -- Etapa 2 (v2): cadenas por lado, poda entre lados
        cache = {}
        contador = {}
        preparados = {}
        for gi, g in enumerate(grupos):
            tg = time.perf_counter()
            if ctrl is not None:
                ctrl.tick(fase="lados", n=n, grupo=gi + 1)
            lados, info = preparar_grupo(g, orillas, n, cache, fin_tiempo=fin, contador=contador)
            info.update({"grupo": gi + 1, "segundos": time.perf_counter() - tg})
            inf["grupos"].append(info)
            registro(f"[2] Grupo {gi + 1}: cadenas por lado {info['cadenas']}  "
                     f"firmas por lado {info['firmas_antes']} → tras poda {info['firmas_despues']}"
                     + ("  → GRUPO DESCARTADO" if lados is None else ""))
            if lados is not None:
                preparados[gi] = (gi + 1, g, lados)
        t2 = time.perf_counter()
        inf["tiempos"]["lados"] = t2 - t1
        if not preparados:
            registro("Ningún grupo admite un marco completo: el tablero no tiene solución.")
            inf["motivo"] = "sin marcos"
            inf["tiempos"]["total"] = time.perf_counter() - t0
            return inf

        # -- Etapa 3: ¿fundir grupos? (misma regla que la v1, contando firmas de lado)
        conjuntos = {gi: {(s, f) for s in range(4) for f in lados[s]}
                     for gi, (_, _, lados) in preparados.items()}
        fundir, motivo = decidir_fusion(conjuntos, limite_fusion=limite_fusion)
        registro(f"[3] {'Se funden los grupos' if fundir else 'Grupos separados'}: {motivo}")
        inf["fusion"] = motivo
        if fundir:
            lotes = [("todos", list(preparados.values()))]
        else:
            lotes = [(f"grupo {g[0]}", [g]) for g in preparados.values()]
        inf["firmas_lado_total"] = sum(len(l[s]) for _, _, l in preparados.values() for s in range(4))

        # -- Etapa 4: interior
        fac = Factibilidad(Counter(orillas))
        nodos = 0
        for nombre, lote in lotes:
            b = BuscadorInterior(n, interiores, lote, fac, ctrl=ctrl, fin_tiempo=fin,
                                 semilla_orden=semilla_orden, reparto=reparto)
            activo["buscador"], activo["previos"] = b, nodos
            res = b.buscar()
            nodos += b.nodos
            activo["buscador"] = None
            registro(f"[4] Interior con {nombre}: {'SOLUCIÓN' if res is not None else 'sin solución'}, "
                     f"{b.nodos} nodos, {b.comprobaciones} comprobaciones de piezas del marco")
            if res is not None:
                collar, gid = b.collar_final()
                grid = [None] * (n * n)
                colocar_marco(n, collar, grid)
                m = n - 2
                for k, p in enumerate(res):
                    i, j = divmod(k, m)
                    grid[(i + 1) * n + (j + 1)] = p
                ok, msg = verificar(n, grid, piezas)
                inf.update(resuelto=ok, verificacion=msg, solucion=grid, grupo_solucion=gid)
                break
        inf["nodos_interior"] = nodos
        inf["tiempos"]["interior"] = time.perf_counter() - t2
    except Detenido as e:
        inf["motivo"] = str(e)
        if activo["buscador"] is not None:
            inf["nodos_interior"] = activo["previos"] + activo["buscador"].nodos
            inf["fase_cortada"] = "interior"
        else:
            inf["fase_cortada"] = inf.get("fase_actual", "?")
        registro(f"Búsqueda cortada: {e}")
    inf["tiempos"]["total"] = time.perf_counter() - t0
    if ctrl is not None and inf.get("solucion"):
        ctrl.tick(fase="fin", n=n, grid=inf["solucion"])
    return inf


def orden_de_grupos(piezas, n):
    """Ordena los grupos del más barato al más caro según el número de cadenas por
    lado (producto de los 4 lados). Contarlas cuesta milisegundos."""
    from lados import extremos_de_lados, enumerar_cadenas
    esquinas, orillas, _ = clasificar(piezas)
    cuenta = Counter(orillas)
    cache = {}
    res = []
    for g in grupos_de_esquinas(esquinas):
        coste = 1
        for (a, b) in extremos_de_lados(g):
            if (a, b) not in cache:
                cache[(a, b)] = sum(len(v) for v in enumerar_cadenas(a, b, cuenta, n - 2).values())
            coste *= cache[(a, b)]
        res.append((coste, g))
    res.sort(key=lambda x: x[0])
    return res


def _resolver_por_grupo(n, piezas, ctrl, limite_segundos, limite_fusion, semilla_orden, reparto,
                        metodo, tope_marcos, registro):
    t0 = time.perf_counter()
    orden = orden_de_grupos(piezas, n)
    registro(f"[G] Estrategia grupo a grupo. {len(orden)} grupos, del más barato al más caro "
             f"(producto de cadenas por lado): {[c for c, _ in orden]}")
    total = {"n": n, "resuelto": False, "tiempos": {"orden": time.perf_counter() - t0},
             "grupos": [], "version": 2, "estrategia": "por_grupo", "nodos_interior": 0,
             "grupos_recorridos": 0, "firmas_quitadas": 0}
    descartadas = set()
    for k, (coste, g) in enumerate(orden):
        resto = None
        if limite_segundos:
            resto = limite_segundos - (time.perf_counter() - t0)
            if resto <= 0:
                total["motivo"] = "límite de tiempo"
                break
        registro(f"[G] Grupo {k + 1}/{len(orden)} (coste {coste}): esquinas {g}")
        inf = resolver(n, piezas, ctrl=ctrl, limite_segundos=resto, limite_fusion=limite_fusion,
                       semilla_orden=semilla_orden, reparto=reparto, metodo=metodo,
                       tope_marcos=tope_marcos, solo_grupos=[g], excluir_firmas=descartadas,
                       registro=registro)
        total["grupos_recorridos"] = k + 1
        total["firmas_quitadas"] += sum(d.get("firmas_quitadas", 0) for d in inf.get("grupos", []))
        total["nodos_interior"] += inf.get("nodos_interior") or 0
        total["metodo_usado"] = inf.get("metodo_usado")
        for clave in ("marcos_total", "firmas_total", "firmas_lado_total"):
            if clave in inf:
                total[clave] = total.get(clave, 0) + inf[clave]
        if inf["resuelto"]:
            total.update(resuelto=True, verificacion=inf["verificacion"], solucion=inf["solucion"])
            break
        if inf.get("motivo") and inf["motivo"] != "sin marcos":
            total["motivo"] = inf["motivo"]
            total["fase_cortada"] = inf.get("fase_cortada")
            break
        probadas = inf.pop("_firmas_probadas", None)
        if probadas:
            descartadas |= probadas
        registro(f"[G] El grupo {k + 1} no tiene solución: se pasa al siguiente"
                 + (f" ({len(descartadas)} firmas descartadas para los siguientes)" if descartadas else ""))
    total["tiempos"]["total"] = time.perf_counter() - t0
    return total


def _ruta_marcos(n, piezas, grupos, orillas, interiores, tope, inf, activo, ctrl, fin,
                 limite_fusion, reparto, registro, t1, excluir_firmas=None):
    """Ruta de la versión 1 (marcos completos). Devuelve False si hay demasiados marcos."""
    firmas_por_grupo = {}
    acumulado = 0
    for gi, g in enumerate(grupos):
        try:
            nm, firmas, _ = enumerar_marcos(g, orillas, n, ctrl=ctrl,
                                            limite_marcos=tope - acumulado, fin_tiempo=fin)
        except DemasiadosMarcos:
            inf["tiempos"]["marcos_intento"] = time.perf_counter() - t1
            return False
        acumulado += nm
        quitadas = 0
        if excluir_firmas:
            # Idea de Carlos: una firma que ya se probó entera en un grupo anterior sin
            # solución no tiene interior posible (el interior solo depende de la firma).
            antes = len(firmas)
            firmas = {f: v for f, v in firmas.items() if f not in excluir_firmas}
            quitadas = antes - len(firmas)
        inf["grupos"].append({"grupo": gi + 1, "marcos": nm, "firmas": len(firmas),
                              "firmas_quitadas": quitadas})
        registro(f"[2] Grupo {gi + 1}: {nm} marcos únicos → {len(firmas) + quitadas} firmas"
                 + (f", menos {quitadas} ya descartadas en grupos anteriores = {len(firmas)}" if quitadas else "")
                 + ("  → GRUPO DESCARTADO" if not firmas else ""))
        if firmas:
            firmas_por_grupo[gi] = firmas
    t2 = time.perf_counter()
    inf["tiempos"]["marcos"] = t2 - t1
    inf["metodo_usado"] = "marcos"
    inf["marcos_total"] = acumulado
    if not firmas_por_grupo:
        registro("Ningún grupo admite un marco completo: el tablero no tiene solución.")
        inf["motivo"] = "sin marcos"
        return True
    fundir, motivo = decidir_fusion(firmas_por_grupo, limite_fusion=limite_fusion)
    registro(f"[3] {'Se funden los grupos' if fundir else 'Grupos separados'}: {motivo}")
    inf["fusion"] = motivo
    if fundir:
        unidas = {}
        for firmas in firmas_por_grupo.values():
            for f, (cnt, ej) in firmas.items():
                if f in unidas:
                    unidas[f][0] += cnt
                else:
                    unidas[f] = [cnt, ej]
        lotes = [("todos", unidas)]
    else:
        lotes = sorted(((f"grupo {gi + 1}", f) for gi, f in firmas_por_grupo.items()),
                       key=lambda x: len(x[1]))
    inf["firmas_total"] = sum(len(f) for _, f in lotes)
    inf["_firmas_probadas"] = set().union(*[set(f) for _, f in lotes])
    nodos = 0
    for nombre, firmas in lotes:
        lista = sorted(firmas.items())
        b = BuscadorMarcos(n, interiores, lista, ctrl=ctrl, fin_tiempo=fin, reparto=reparto)
        activo["buscador"], activo["previos"] = b, nodos
        res = b.buscar()
        nodos += b.nodos
        activo["buscador"] = None
        registro(f"[4] Interior con {nombre} ({len(lista)} firmas): "
                 f"{'SOLUCIÓN' if res else 'sin solución'}, {b.nodos} nodos")
        if res:
            celdas, idx = res
            grid = [None] * (n * n)
            colocar_marco(n, lista[idx][1][1], grid)
            m = n - 2
            for k, p in enumerate(celdas):
                i, j = divmod(k, m)
                grid[(i + 1) * n + (j + 1)] = p
            ok, msg = verificar(n, grid, piezas)
            inf.update(resuelto=ok, verificacion=msg, solucion=grid)
            break
    inf["nodos_interior"] = nodos
    inf["tiempos"]["interior"] = time.perf_counter() - t2
    return True


# ---------------------------------------------------------------- varios núcleos

def _trabajador(i, n, piezas, limite, cola, modo, procesos, metodo, estrategia="por_grupo"):
    if modo == "dividir":
        inf = resolver(n, piezas, limite_segundos=limite, reparto=(i, procesos),
                       metodo=metodo, estrategia=estrategia, registro=lambda *a: None)
    else:
        inf = resolver(n, piezas, limite_segundos=limite, metodo=metodo, estrategia=estrategia,
                       semilla_orden=None if i == 0 else 7919 * i, registro=lambda *a: None)
    inf["trabajador"] = i
    cola.put(inf)


def resolver_paralelo(n, piezas, procesos, limite_segundos=None, modo="dividir", metodo="auto",
                      estrategia="por_grupo", registro=print):
    """Varias búsquedas a la vez; gana la primera que encuentra solución.
    modo="dividir":     cada proceso explora una parte distinta del árbol (sin repetir).
    modo="portafolio":  cada proceso explora todo, con las piezas en distinto orden."""
    t0 = time.perf_counter()
    cola = mp.Queue()
    ps = [mp.Process(target=_trabajador, args=(i, n, piezas, limite_segundos, cola, modo, procesos, metodo, estrategia),
                     daemon=True) for i in range(procesos)]
    for p in ps:
        p.start()
    registro(f"[P] {procesos} procesos en paralelo, modo {modo}")
    mejor = None
    recibidos = 0
    espera = (limite_segundos + 60) if limite_segundos else None
    while recibidos < procesos:
        try:
            inf = cola.get(timeout=espera)
        except Exception:
            break
        recibidos += 1
        if inf["resuelto"]:
            mejor = inf
            break
        mejor = mejor or inf
    for p in ps:
        if p.is_alive():
            p.terminate()
    if mejor is None:
        mejor = {"n": n, "resuelto": False, "tiempos": {}, "motivo": "ningún proceso respondió"}
    mejor["tiempos"]["total"] = time.perf_counter() - t0
    mejor["procesos"] = procesos
    mejor["modo_paralelo"] = modo
    registro(f"[P] Resultado del trabajador {mejor.get('trabajador')}")
    return mejor


def resumen(inf):
    t = inf["tiempos"]
    lineas = [f"Resultado: {'RESUELTO' if inf['resuelto'] else 'NO resuelto'}"
              + (f" — {inf.get('verificacion')}" if inf.get("verificacion") else "")
              + (f" ({inf['motivo']}"
                 + (f", cortado en la fase de {inf['fase_cortada']}" if inf.get("fase_cortada") else "")
                 + ")" if inf.get("motivo") else "")]
    lineas.append("Cronómetro:  " + "   ".join(f"{k} {v:.3f} s" for k, v in t.items()))
    return "\n".join(lineas)


def cargar(args):
    if args.modelo:
        n, piezas = leer_tablero(args.modelo)
        if not args.sin_mezclar:
            piezas = mezclar(piezas, args.semilla)
        return n, piezas
    _, piezas = generar(args.generar, args.colores, args.colores_borde, args.semilla)
    return args.generar, piezas


def main():
    ap = argparse.ArgumentParser(description="Solucionador por marcos — versión 2")
    ap.add_argument("--modelo")
    ap.add_argument("--generar", type=int)
    ap.add_argument("--colores", type=int, default=6)
    ap.add_argument("--colores-borde", type=int, default=None)
    ap.add_argument("--semilla", type=int, default=None)
    ap.add_argument("--sin-mezclar", action="store_true")
    ap.add_argument("--limite", type=float, default=None)
    ap.add_argument("--procesos", type=int, default=1,
                    help=f"búsquedas en paralelo (este equipo tiene {os.cpu_count()} núcleos)")
    ap.add_argument("--modo", choices=["dividir", "portafolio"], default="dividir",
                    help="cómo repartir el trabajo entre procesos")
    ap.add_argument("--metodo", choices=["auto", "marcos", "lados"], default="auto",
                    help="auto: marcos completos si son pocos, si no firmas por lado")
    ap.add_argument("--tope-marcos", type=int, default=150_000)
    ap.add_argument("--estrategia", choices=["fundir", "por_grupo"], default="por_grupo",
                    help="por_grupo: un grupo completo cada vez (idea de Carlos)")
    ap.add_argument("--grafico", action="store_true")
    ap.add_argument("--guardar")
    args = ap.parse_args()
    if not args.modelo and not args.generar:
        ap.error("usa --modelo ARCHIVO o --generar N")
    n, piezas = cargar(args)
    if args.grafico:
        from grafico import abrir_ventana
        abrir_ventana(n, piezas, limite=args.limite)
        return
    if args.procesos > 1:
        inf = resolver_paralelo(n, piezas, args.procesos, limite_segundos=args.limite, modo=args.modo,
                                metodo=args.metodo, estrategia=args.estrategia)
    else:
        inf = resolver(n, piezas, limite_segundos=args.limite, metodo=args.metodo,
                       tope_marcos=args.tope_marcos, estrategia=args.estrategia)
    print(resumen(inf))
    if inf.get("solucion") and args.guardar:
        escribir_tablero(args.guardar, n, inf["solucion"])
        print(f"Solución guardada en {args.guardar}")
    sys.exit(0 if inf["resuelto"] else 1)


if __name__ == "__main__":
    mp.freeze_support()
    main()
