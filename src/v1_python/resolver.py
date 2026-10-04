"""
Solucionador por marcos (método de Carlos) para tableros cuadrados de Eternity II.

Uso rápido (sin gráficos, máxima velocidad):
    python resolver.py --modelo modelos_editor/model_yannick_0706.txt
    python resolver.py --generar 7 --colores 6 --semilla 3
Con ventana gráfica (pausa, paso a paso, cámara lenta):
    python resolver.py --modelo modelos_editor/model_yannick_0505.txt --grafico
"""
import argparse
import sys
import time

from nucleo import leer_tablero, generar, mezclar, verificar, escribir_tablero
from marcos import (clasificar, grupos_de_esquinas, enumerar_marcos, decidir_fusion,
                    colocar_marco, DemasiadosMarcos)
from interior import BuscadorInterior
from control import Detenido


def colores_obligatorios(firmas):
    """Restricciones comunes a todos los marcos: posiciones del collar cuyo color
    hacia adentro es el mismo en todas las firmas."""
    if not firmas:
        return 0, 0
    lista = list(firmas)
    P = len(lista[0])
    fijas = sum(1 for p in range(P) if len({f[p] for f in lista}) == 1)
    return fijas, P


def resolver(n, piezas, ctrl=None, limite_segundos=None, limite_marcos=5_000_000,
             limite_fusion=200_000, registro=print):
    """Devuelve un informe (dict). Si ctrl es None corre a máxima velocidad."""
    inf = {"n": n, "resuelto": False, "tiempos": {}, "grupos": []}
    activo = {"buscador": None, "nodos_previos": 0}
    t0 = time.perf_counter()
    fin = t0 + limite_segundos if limite_segundos else None
    if ctrl is not None:
        ctrl.t0 = t0
        ctrl.limite = limite_segundos

    try:
        # -- Etapa 1: esquina fija y grupos
        esquinas, orillas, interiores = clasificar(piezas)
        grupos = grupos_de_esquinas(esquinas)
        t1 = time.perf_counter()
        inf["tiempos"]["grupos"] = t1 - t0
        registro(f"[1] Esquina fija {esquinas[0]}. Grupos distintos: {len(grupos)} "
                 f"(de 6 posibles; menos si hay esquinas iguales)")

        # -- Etapa 2: marcos por grupo (sin repetidos) y firmas
        firmas_por_grupo = {}
        for gi, g in enumerate(grupos):
            tg = time.perf_counter()
            nm, firmas, nodos = enumerar_marcos(g, orillas, n, ctrl=ctrl,
                                                limite_marcos=limite_marcos, fin_tiempo=fin)
            fijas, P = colores_obligatorios(firmas)
            dato = {"grupo": gi + 1, "esquinas": g, "marcos": nm, "firmas": len(firmas),
                    "colores_obligatorios": f"{fijas}/{P}", "segundos": time.perf_counter() - tg}
            inf["grupos"].append(dato)
            registro(f"[2] Grupo {gi + 1}: {nm} marcos únicos → {len(firmas)} firmas "
                     f"(colores obligatorios {fijas}/{P})"
                     + ("  → GRUPO DESCARTADO" if nm == 0 else ""))
            if nm:
                firmas_por_grupo[gi] = firmas
        t2 = time.perf_counter()
        inf["tiempos"]["marcos"] = t2 - t1
        inf["marcos_total"] = sum(d["marcos"] for d in inf["grupos"])

        if not firmas_por_grupo:
            registro("Ningún grupo admite un marco completo: el tablero no tiene solución.")
            inf["tiempos"]["total"] = time.perf_counter() - t0
            inf["motivo"] = "sin marcos"
            return inf

        # -- Etapa 3: ¿fundir grupos?
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

        # -- Etapa 4: interior
        nodos = 0
        for nombre, firmas in lotes:
            lista = sorted(firmas.items())
            b = BuscadorInterior(n, interiores, lista, ctrl=ctrl, fin_tiempo=fin)
            activo["buscador"], activo["nodos_previos"] = b, nodos
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
                inf["resuelto"] = ok
                inf["verificacion"] = msg
                inf["solucion"] = grid
                break
        inf["nodos_interior"] = nodos
        t3 = time.perf_counter()
        inf["tiempos"]["interior"] = t3 - t2
    except Detenido as e:
        inf["motivo"] = str(e)
        if activo["buscador"] is not None:
            inf["nodos_interior"] = activo["nodos_previos"] + activo["buscador"].nodos
            inf["fase_cortada"] = "interior"
        elif "firmas_total" not in inf:
            inf["fase_cortada"] = "marcos"
            inf["marcos_total"] = sum(d["marcos"] for d in inf["grupos"])
        registro(f"Búsqueda cortada: {e}")
    except DemasiadosMarcos as e:
        inf["motivo"] = str(e)
        registro(f"Demasiados marcos: {e}")
    inf["tiempos"]["total"] = time.perf_counter() - t0
    if ctrl is not None and inf.get("solucion"):
        ctrl.tick(fase="fin", n=n, grid=inf["solucion"])
    return inf


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
        # Los modelos del Editor vienen resueltos: se mezclan y giran como hace el Editor.
        if not args.sin_mezclar:
            piezas = mezclar(piezas, args.semilla)
        return n, piezas
    n = args.generar
    _, piezas = generar(n, args.colores, args.colores_borde, args.semilla)
    return n, piezas


def main():
    ap = argparse.ArgumentParser(description="Solucionador por marcos (método de Carlos)")
    ap.add_argument("--modelo", help="archivo de tablero del Eternity Editor")
    ap.add_argument("--generar", type=int, help="generar un tablero aleatorio de N x N")
    ap.add_argument("--colores", type=int, default=6)
    ap.add_argument("--colores-borde", type=int, default=None)
    ap.add_argument("--semilla", type=int, default=None)
    ap.add_argument("--sin-mezclar", action="store_true")
    ap.add_argument("--limite", type=float, default=None, help="segundos máximos")
    ap.add_argument("--limite-marcos", type=int, default=5_000_000)
    ap.add_argument("--grafico", action="store_true", help="abrir la ventana gráfica")
    ap.add_argument("--guardar", help="guardar la solución en este archivo")
    args = ap.parse_args()
    if not args.modelo and not args.generar:
        ap.error("usa --modelo ARCHIVO o --generar N")

    n, piezas = cargar(args)
    if args.grafico:
        from grafico import abrir_ventana
        abrir_ventana(n, piezas, limite_marcos=args.limite_marcos, limite=args.limite)
        return
    inf = resolver(n, piezas, limite_segundos=args.limite, limite_marcos=args.limite_marcos)
    print(resumen(inf))
    if inf.get("solucion") and args.guardar:
        escribir_tablero(args.guardar, n, inf["solucion"])
        print(f"Solución guardada en {args.guardar}")
    sys.exit(0 if inf["resuelto"] else 1)


if __name__ == "__main__":
    main()
