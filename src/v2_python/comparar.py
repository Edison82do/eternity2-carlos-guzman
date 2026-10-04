"""
(VERSIÓN 2) Además de MkIV, puede medir la v2 con 1 núcleo y con varios:
    python comparar.py --tamanos 7 --semillas 10 --limite 60 --procesos 1 4
    python comparar.py --tamanos 7 --semillas 10 --sin-java      (solo la v2)

Compara el método por marcos contra un solver del Eternity II Editor
(por defecto Iterative Path MkIV | Human) en los MISMOS tableros.

    python comparar.py --tamanos 5 6 7 --colores 6 --semillas 5 --limite 30
    python comparar.py --modelos modelos_editor/model_yannick_0606.txt --semillas 5

Cada corrida parte de un tablero mezclado y girado al azar (cada método con su
propia mezcla, controlada por la semilla). Se miden tasa de éxito y tiempos.
"""
import argparse
import csv
import os
import re
import statistics
import subprocess
import sys
import tempfile
import time

from nucleo import generar, leer_tablero, mezclar, escribir_tablero
from resolver import resolver, resolver_paralelo

AQUI = os.path.dirname(os.path.abspath(__file__))


def correr_java(jar, modelo, solver, semilla, limite):
    cp = os.pathsep.join([jar, os.path.join(AQUI, "comparar_java")])
    cmd = ["java", "-Xss512m", "-cp", cp, "Cronometro", modelo, solver, str(semilla), str(limite)]
    try:
        out = subprocess.run(cmd, capture_output=True, text=True, timeout=limite + 30).stdout
    except subprocess.TimeoutExpired:
        return {"resuelto": False, "segundos": limite, "detalle": "timeout"}
    m = re.search(r"resuelto=(\w+) puntos=(\d+)/(\d+) ms=(\d+) iteraciones=(\d+)", out)
    if not m:
        return {"resuelto": False, "segundos": limite, "detalle": "error: " + out[-200:]}
    return {"resuelto": m.group(1) == "true", "segundos": int(m.group(4)) / 1000,
            "detalle": f"{m.group(2)}/{m.group(3)} pts, {m.group(5)} iter"}


def correr_marcos(n, piezas, semilla, limite, procesos=1, metodo="auto", estrategia="por_grupo"):
    p = mezclar(piezas, semilla)
    if procesos > 1:
        inf = resolver_paralelo(n, p, procesos, limite_segundos=limite, metodo=metodo,
                                estrategia=estrategia, registro=lambda *a: None)
    else:
        inf = resolver(n, p, limite_segundos=limite, metodo=metodo, estrategia=estrategia,
                       registro=lambda *a: None)
    return {"resuelto": inf["resuelto"], "segundos": inf["tiempos"]["total"],
            "detalle": f"método {inf.get('metodo_usado', '?')}, {inf.get('marcos_total', '-')} marcos, "
                       f"{inf.get('firmas_total', inf.get('firmas_lado_total', '?'))} firmas, "
                       f"{inf.get('nodos_interior', '?')} nodos"
                       + (f", {inf['motivo']} en fase {inf.get('fase_cortada', '?')}" if inf.get("motivo") else "")}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--tamanos", type=int, nargs="*", default=[])
    ap.add_argument("--colores", type=int, default=6)
    ap.add_argument("--modelos", nargs="*", default=[])
    ap.add_argument("--semillas", type=int, default=5)
    ap.add_argument("--limite", type=float, default=30)
    ap.add_argument("--jar", default=os.path.join(AQUI, "..", "EternityEditor-1.6.0.jar"))
    ap.add_argument("--solver", default="Iterative Path MkIV|Human")
    ap.add_argument("--csv", default=os.path.join(AQUI, "resultados_comparacion.csv"))
    ap.add_argument("--procesos", type=int, nargs="*", default=[1],
                    help="medir la v2 con estos números de procesos, p. ej. 1 4")
    ap.add_argument("--metodo", choices=["auto", "marcos", "lados"], default="auto")
    ap.add_argument("--sin-java", action="store_true", help="no correr MkIV")
    ap.add_argument("--estrategia", choices=["fundir", "por_grupo"], default="por_grupo")
    args = ap.parse_args()
    if not args.sin_java and not os.path.exists(args.jar):
        sys.exit(f"No encuentro el jar del Editor en {args.jar} (usa --jar RUTA)")

    tableros = []
    tmp = tempfile.mkdtemp(prefix="e2cmp_")
    for n in args.tamanos:
        for s in range(1, args.semillas + 1):
            grid, _ = generar(n, args.colores, semilla=1000 * n + s)
            ruta = os.path.join(tmp, f"gen_{n}x{n}_c{args.colores}_s{s}.txt")
            escribir_tablero(ruta, n, grid)
            tableros.append((f"{n}x{n} c{args.colores}", ruta, s))
    for ruta in args.modelos:
        for s in range(1, args.semillas + 1):
            tableros.append((os.path.basename(ruta), ruta, s))

    filas = []
    for nombre, ruta, s in tableros:
        n, piezas = leer_tablero(ruta)
        linea = f"{nombre:>22} s{s}:"
        for P in args.procesos:
            r = correr_marcos(n, piezas, s, args.limite, P, args.metodo, args.estrategia)
            etiqueta = f"Marcos v2 {args.estrategia} ({P} proc)"
            filas.append({"tablero": nombre, "semilla": s, "metodo": etiqueta, **r})
            linea += f"  v2x{P} {'OK' if r['resuelto'] else '--'} {r['segundos']:7.2f}s |"
        if not args.sin_java:
            r2 = correr_java(args.jar, ruta, args.solver, s, args.limite)
            filas.append({"tablero": nombre, "semilla": s, "metodo": args.solver, **r2})
            linea += f"  MkIV {'OK' if r2['resuelto'] else '--'} {r2['segundos']:7.2f}s"
        print(linea, flush=True)

    with open(args.csv, "w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=["tablero", "semilla", "metodo", "resuelto", "segundos", "detalle"])
        w.writeheader()
        w.writerows(filas)

    print("\nResumen (tiempo de las corridas no resueltas = límite):")
    print(f"{'tablero':>22} {'método':>28} {'éxitos':>8} {'mediana s':>10} {'media s':>9}")
    for nombre in dict.fromkeys(t[0] for t in tableros):
        for metodo in dict.fromkeys(f["metodo"] for f in filas):
            sel = [f for f in filas if f["tablero"] == nombre and f["metodo"] == metodo]
            ok = sum(f["resuelto"] for f in sel)
            ts = [f["segundos"] if f["resuelto"] else args.limite for f in sel]
            print(f"{nombre:>22} {metodo:>28} {ok:>4}/{len(sel):<3} {statistics.median(ts):>10.2f} "
                  f"{statistics.mean(ts):>9.2f}")
    print(f"\nDetalle en {args.csv}")


if __name__ == "__main__":
    main()
