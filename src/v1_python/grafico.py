"""
Ventana gráfica del solucionador por marcos.

 - Iniciar / Pausa / Continuar / Paso a paso / Detener
 - Cámara lenta: milisegundos de espera por cada paso (0 = sin espera)
 - Modo rápido: corre sin animación (máxima velocidad) y solo muestra el resultado
 - Cronómetro en vivo y registro de cada etapa
"""
import os
import queue
import threading
import time
import tkinter as tk
from tkinter import filedialog, ttk

from control import Control
from marcos import collar_a_casillas, pieza_desde_collar
from nucleo import rotar, leer_tablero, generar, mezclar
from resolver import resolver, resumen

PALETA = ["#7f7f7f",  # 0 = gris de la orilla
          "#e6194b", "#3cb44b", "#ffe119", "#4363d8", "#f58231", "#911eb4", "#46f0f0",
          "#f032e6", "#bcf60c", "#fabebe", "#008080", "#e6beff", "#9a6324", "#fffac8",
          "#800000", "#aaffc3", "#808000", "#ffd8b1", "#000075", "#a9a9a9", "#ffffff",
          "#1b1b1b", "#ff69b4", "#40e0d0", "#8b4513", "#6a5acd"]


def color(c):
    return PALETA[c % len(PALETA)]


class Ventana:
    def __init__(self, raiz, n, piezas, limite_marcos=5_000_000, limite=None):
        self.raiz = raiz
        self.n, self.piezas = n, piezas
        self.limite_marcos, self.limite = limite_marcos, limite
        self.ctrl = None
        self.hilo = None
        self.cola = queue.Queue()
        self.t_inicio = None
        self.ultima_version = -1
        self.informe = None

        raiz.title("Solucionador por marcos — Eternity II")
        izq = ttk.Frame(raiz, padding=6)
        izq.pack(side="left", fill="both", expand=True)
        der = ttk.Frame(raiz, padding=6)
        der.pack(side="right", fill="y")

        self.lienzo = tk.Canvas(izq, width=640, height=640, bg="#202020", highlightthickness=0)
        self.lienzo.pack(fill="both", expand=True)
        self.lienzo.bind("<Configure>", lambda e: self.redibujar(forzar=True))

        ttk.Label(der, text="Tablero", font=("Segoe UI", 10, "bold")).pack(anchor="w")
        self.lbl_tablero = ttk.Label(der, text="")
        self.lbl_tablero.pack(anchor="w")
        fila = ttk.Frame(der)
        fila.pack(fill="x", pady=2)
        ttk.Button(fila, text="Abrir modelo…", command=self.abrir).pack(side="left")
        ttk.Button(fila, text="Generar", command=self.generar).pack(side="left", padx=4)
        fila2 = ttk.Frame(der)
        fila2.pack(fill="x")
        ttk.Label(fila2, text="N").pack(side="left")
        self.var_n = tk.IntVar(value=n)
        ttk.Spinbox(fila2, from_=3, to=16, width=4, textvariable=self.var_n).pack(side="left")
        ttk.Label(fila2, text=" colores").pack(side="left")
        self.var_col = tk.IntVar(value=6)
        ttk.Spinbox(fila2, from_=2, to=24, width=4, textvariable=self.var_col).pack(side="left")
        ttk.Label(fila2, text=" semilla").pack(side="left")
        self.var_sem = tk.IntVar(value=1)
        ttk.Spinbox(fila2, from_=0, to=99999, width=6, textvariable=self.var_sem).pack(side="left")

        ttk.Separator(der).pack(fill="x", pady=6)
        self.var_rapido = tk.BooleanVar(value=False)
        ttk.Checkbutton(der, text="Modo rápido (sin animación)", variable=self.var_rapido).pack(anchor="w")
        ttk.Label(der, text="Cámara lenta (ms por paso)").pack(anchor="w", pady=(6, 0))
        self.var_lento = tk.IntVar(value=0)
        ttk.Scale(der, from_=0, to=1000, variable=self.var_lento, orient="horizontal",
                  command=self.cambiar_lento).pack(fill="x")
        self.lbl_lento = ttk.Label(der, text="0 ms")
        self.lbl_lento.pack(anchor="w")

        botones = ttk.Frame(der)
        botones.pack(fill="x", pady=6)
        self.b_iniciar = ttk.Button(botones, text="Iniciar", command=self.iniciar)
        self.b_iniciar.grid(row=0, column=0, sticky="ew")
        self.b_pausa = ttk.Button(botones, text="Pausa", command=self.pausa, state="disabled")
        self.b_pausa.grid(row=0, column=1, sticky="ew")
        self.b_paso = ttk.Button(botones, text="Paso", command=self.paso, state="disabled")
        self.b_paso.grid(row=1, column=0, sticky="ew")
        self.b_detener = ttk.Button(botones, text="Detener", command=self.detener, state="disabled")
        self.b_detener.grid(row=1, column=1, sticky="ew")

        ttk.Separator(der).pack(fill="x", pady=6)
        self.lbl_crono = ttk.Label(der, text="⏱ 0.000 s", font=("Consolas", 16, "bold"))
        self.lbl_crono.pack(anchor="w")
        self.lbl_fase = ttk.Label(der, text="Fase: —")
        self.lbl_fase.pack(anchor="w")
        self.lbl_info = ttk.Label(der, text="", justify="left")
        self.lbl_info.pack(anchor="w")

        self.registro = tk.Text(der, width=52, height=18, font=("Consolas", 9), wrap="word")
        self.registro.pack(fill="both", expand=True, pady=4)

        self.actualizar_tablero_lbl()
        self.dibujar_vacio()
        self.raiz.after(40, self.bucle)

    # ------------------------------------------------------------ tablero
    def actualizar_tablero_lbl(self):
        self.lbl_tablero.config(text=f"{self.n} x {self.n}  ({len(self.piezas)} piezas)")

    def abrir(self):
        if self.corriendo():
            return
        ruta = filedialog.askopenfilename(filetypes=[("Tableros", "*.txt"), ("Todos", "*.*")])
        if ruta:
            n, piezas = leer_tablero(ruta)
            self.n, self.piezas = n, mezclar(piezas, self.var_sem.get())
            self.informe = None
            self.var_n.set(n)
            self.escribir(f"Cargado {os.path.basename(ruta)} y mezclado (semilla {self.var_sem.get()})")
            self.actualizar_tablero_lbl()
            self.dibujar_vacio()

    def generar(self):
        if self.corriendo():
            return
        n = self.var_n.get()
        _, piezas = generar(n, self.var_col.get(), semilla=self.var_sem.get())
        self.n, self.piezas = n, piezas
        self.informe = None
        self.escribir(f"Generado {n}x{n} con {self.var_col.get()} colores (semilla {self.var_sem.get()})")
        self.actualizar_tablero_lbl()
        self.dibujar_vacio()

    # ------------------------------------------------------------ control
    def corriendo(self):
        return self.hilo is not None and self.hilo.is_alive()

    def cambiar_lento(self, _=None):
        ms = int(float(self.var_lento.get()))
        self.lbl_lento.config(text=f"{ms} ms")
        if self.ctrl:
            self.ctrl.retardo = ms / 1000

    def iniciar(self):
        if self.corriendo():
            return
        self.informe = None
        self.ultima_version = -1
        rapido = self.var_rapido.get()
        self.ctrl = None if rapido else Control(retardo=int(float(self.var_lento.get())) / 1000)
        self.escribir(f"--- Inicio ({'modo rápido' if rapido else 'modo gráfico'}) ---")
        self.t_inicio = time.perf_counter()

        def trabajo():
            inf = resolver(self.n, self.piezas, ctrl=self.ctrl, limite_segundos=self.limite,
                           limite_marcos=self.limite_marcos, registro=lambda s: self.cola.put(s))
            self.cola.put(("FIN", inf))

        self.hilo = threading.Thread(target=trabajo, daemon=True)
        self.hilo.start()
        self.b_iniciar.config(state="disabled")
        estado = "disabled" if rapido else "normal"
        self.b_pausa.config(state=estado, text="Pausa")
        self.b_paso.config(state=estado)
        self.b_detener.config(state=estado)

    def pausa(self):
        if not self.ctrl:
            return
        if self.ctrl.pausado:
            self.ctrl.continuar()
            self.b_pausa.config(text="Pausa")
        else:
            self.ctrl.pausar()
            self.b_pausa.config(text="Continuar")

    def paso(self):
        if self.ctrl:
            if not self.ctrl.pausado:
                self.ctrl.pausar()
                self.b_pausa.config(text="Continuar")
            self.ctrl.un_paso()

    def detener(self):
        if self.ctrl:
            self.ctrl.detener = True
            self.ctrl.continuar()

    # ------------------------------------------------------------ bucle de la interfaz
    def escribir(self, s):
        self.registro.insert("end", s + "\n")
        self.registro.see("end")

    def bucle(self):
        try:
            while True:
                msg = self.cola.get_nowait()
                if isinstance(msg, tuple) and msg[0] == "FIN":
                    self.terminar(msg[1])
                else:
                    self.escribir(msg)
        except queue.Empty:
            pass
        if self.corriendo() and self.t_inicio:
            self.lbl_crono.config(text=f"⏱ {time.perf_counter() - self.t_inicio:.3f} s")
        self.redibujar()
        self.raiz.after(40, self.bucle)

    def terminar(self, inf):
        self.informe = inf
        self.redibujar(forzar=True)
        self.escribir(resumen(inf))
        self.lbl_crono.config(text=f"⏱ {inf['tiempos']['total']:.3f} s")
        self.lbl_fase.config(text="Fase: terminado — " + ("RESUELTO" if inf["resuelto"] else "sin resolver"))
        self.lbl_info.config(text=f"marcos: {inf.get('marcos_total', 0):,}\nfirmas: {inf.get('firmas_total', 0):,}"
                                  f"\nnodos interior: {inf.get('nodos_interior', 0):,}")
        if self.ctrl is not None:
            self.escribir("(Modo gráfico: el tiempo incluye la animación. Para comparar "
                          "velocidad usa el modo rápido.)")
        for b in (self.b_pausa, self.b_paso, self.b_detener):
            b.config(state="disabled")
        self.b_iniciar.config(state="normal")

    # ------------------------------------------------------------ dibujo
    def geometria(self):
        w = self.lienzo.winfo_width()
        h = self.lienzo.winfo_height()
        lado = max(4, min(w, h) - 10) / self.n
        x0 = (w - lado * self.n) / 2
        y0 = (h - lado * self.n) / 2
        return lado, x0, y0

    def dibujar_vacio(self):
        self.pintar([None] * (self.n * self.n))

    def pintar(self, grid, resaltar=None):
        self.lienzo.delete("all")
        lado, x0, y0 = self.geometria()
        n = self.n
        for r in range(n):
            for c in range(n):
                x, y = x0 + c * lado, y0 + r * lado
                p = grid[r * n + c]
                if p is None:
                    self.lienzo.create_rectangle(x, y, x + lado, y + lado, fill="#303030", outline="#454545")
                    continue
                cx, cy = x + lado / 2, y + lado / 2
                esquinas = [(x, y), (x + lado, y), (x + lado, y + lado), (x, y + lado)]
                for s in range(4):   # N, E, S, O
                    a, b = esquinas[s], esquinas[(s + 1) % 4]
                    self.lienzo.create_polygon(a[0], a[1], b[0], b[1], cx, cy,
                                               fill=color(p[s]), outline="#202020")
                if lado >= 44:
                    off = lado * 0.3
                    for s, (dx, dy) in enumerate(((0, -off), (off, 0), (0, off), (-off, 0))):
                        self.lienzo.create_text(cx + dx, cy + dy, text=str(p[s]),
                                                font=("Consolas", max(7, int(lado / 7))))
        if resaltar:
            for (r, c) in resaltar:
                x, y = x0 + c * lado, y0 + r * lado
                self.lienzo.create_rectangle(x, y, x + lado, y + lado, outline="white", width=2)

    def redibujar(self, forzar=False):
        if self.informe and self.informe.get("solucion") and (forzar or not self.corriendo()):
            if forzar:
                self.n = self.informe["n"]
                self.pintar(self.informe["solucion"])
            return
        if not self.ctrl:
            if forzar:
                self.dibujar_vacio()
            return
        with self.ctrl.lock:
            est = dict(self.ctrl.estado)
            ver = self.ctrl.version
        if not est or (ver == self.ultima_version and not forzar):
            return
        self.ultima_version = ver
        n = est.get("n", self.n)
        grid = [None] * (n * n)
        fase = est.get("fase")
        collar = est.get("collar")
        if collar:
            pos = collar_a_casillas(n)
            for k, e in enumerate(collar):
                if e is not None:
                    (r, c), g = pos[k]
                    grid[r * n + c] = rotar(pieza_desde_collar(e), g)
        if fase == "interior":
            m = n - 2
            for k, p in enumerate(est.get("interior") or []):
                if p is not None:
                    i, j = divmod(k, m)
                    grid[(i + 1) * n + (j + 1)] = p
            self.lbl_fase.config(text="Fase: interior (marco mostrado = un ejemplo compatible)")
            self.lbl_info.config(text=f"nodos: {est.get('nodos', 0):,}\nfirmas vivas: {est.get('firmas_vivas', 0):,}")
        elif fase == "marcos":
            self.lbl_fase.config(text="Fase: generando marcos")
            self.lbl_info.config(text=f"nodos: {est.get('nodos', 0):,}\nmarcos: {est.get('marcos', 0):,}"
                                      f"\nfirmas: {est.get('firmas', 0):,}")
        elif fase == "fin":
            grid = est.get("grid", grid)
        self.pintar(grid)


def abrir_ventana(n, piezas, limite_marcos=5_000_000, limite=None):
    raiz = tk.Tk()
    raiz.geometry("1100x700")
    Ventana(raiz, n, piezas, limite_marcos=limite_marcos, limite=limite)
    raiz.mainloop()


if __name__ == "__main__":
    _, p = generar(6, 6, semilla=1)
    abrir_ventana(6, p)
