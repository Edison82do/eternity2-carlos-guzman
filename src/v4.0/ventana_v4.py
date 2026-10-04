"""
Ventana para el motor e2marcos4 (versión 4: ruta unificada con mapas por lado).

La ventana solo lanza el motor y lee lo que va escribiendo, así que **no le quita
velocidad**: el motor corre con todos los núcleos elegidos a máxima velocidad.

    python ventana_v4.py
"""
import os
import queue
import subprocess
import sys
import tempfile
import threading
import time
import tkinter as tk
from tkinter import filedialog, ttk

AQUI = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(AQUI, '..', 'solucionador_marcos_v2'))
from nucleo import leer_tablero, verificar   # noqa: E402
from grafico import color                    # noqa: E402

EXE = os.path.join(AQUI, 'e2marcos4.exe' if os.name == 'nt' else 'e2marcos4')
NUCLEOS = os.cpu_count() or 1


class Ventana:
    def __init__(self, raiz):
        self.raiz = raiz
        self.proc = None
        self.cola = queue.Queue()
        self.t0 = None
        self.sol = os.path.join(tempfile.gettempdir(), f'e2marcos_sol_{os.getpid()}.txt')
        raiz.title("e2marcos4 — método por marcos (Carlos), versión 4")

        izq = ttk.Frame(raiz, padding=6); izq.pack(side="left", fill="both", expand=True)
        der = ttk.Frame(raiz, padding=6); der.pack(side="right", fill="y")
        self.lienzo = tk.Canvas(izq, width=620, height=620, bg="#202020", highlightthickness=0)
        self.lienzo.pack(fill="both", expand=True)

        ttk.Label(der, text="Tablero", font=("Segoe UI", 10, "bold")).pack(anchor="w")
        self.var_ruta = tk.StringVar(value=os.path.join(AQUI, 'tableros', 'gen_7x7_c6_s2.txt'))
        ttk.Entry(der, textvariable=self.var_ruta, width=52).pack(anchor="w")
        ttk.Button(der, text="Elegir archivo…", command=self.elegir).pack(anchor="w", pady=2)

        f1 = ttk.Frame(der); f1.pack(fill="x", pady=4)
        self.var_mezclar = tk.BooleanVar(value=False)
        ttk.Checkbutton(f1, text="Mezclar piezas (modelos resueltos), semilla", variable=self.var_mezclar).pack(side="left")
        self.var_semilla = tk.IntVar(value=1)
        ttk.Spinbox(f1, from_=0, to=99999, width=6, textvariable=self.var_semilla).pack(side="left")

        f2 = ttk.Frame(der); f2.pack(fill="x", pady=4)
        ttk.Label(f2, text="Núcleos (hilos)").pack(side="left")
        self.var_hilos = tk.IntVar(value=NUCLEOS)
        ttk.Spinbox(f2, from_=1, to=max(64, NUCLEOS), width=4, textvariable=self.var_hilos).pack(side="left", padx=4)
        ttk.Label(f2, text=f"(este equipo: {NUCLEOS})").pack(side="left")

        f3 = ttk.Frame(der); f3.pack(fill="x", pady=4)
        ttk.Label(f3, text="Límite (s, 0 = sin límite)").pack(side="left")
        self.var_limite = tk.IntVar(value=0)
        ttk.Spinbox(f3, from_=0, to=10**7, width=8, textvariable=self.var_limite).pack(side="left", padx=4)

        f4 = ttk.Frame(der); f4.pack(fill="x", pady=4)
        ttk.Label(f4, text="Método").pack(side="left")
        self.var_metodo = tk.StringVar(value="auto")
        ttk.Combobox(f4, textvariable=self.var_metodo, values=["auto", "marcos", "unificado", "lados"], width=8,
                     state="readonly").pack(side="left", padx=4)

        f6 = ttk.Frame(der); f6.pack(fill="x", pady=4)
        ttk.Label(f6, text="Piezas fijas").pack(side="left")
        self.var_pistas = tk.StringVar(value="")
        ttk.Entry(f6, textvariable=self.var_pistas, width=22).pack(side="left", padx=4)
        ttk.Label(der, text="  formato fila,columna,pieza[,giros]; varias separadas por ;  (E2: 8,9,139)",
                  foreground="#555").pack(anchor="w")

        f5 = ttk.Frame(der); f5.pack(fill="x", pady=6)
        self.b_ini = ttk.Button(f5, text="Iniciar", command=self.iniciar); self.b_ini.pack(side="left")
        self.b_det = ttk.Button(f5, text="Detener", command=self.detener, state="disabled"); self.b_det.pack(side="left", padx=4)

        self.lbl_crono = ttk.Label(der, text="⏱ 0.0 s", font=("Consolas", 16, "bold")); self.lbl_crono.pack(anchor="w")
        self.lbl_estado = ttk.Label(der, text="Listo"); self.lbl_estado.pack(anchor="w")
        self.registro = tk.Text(der, width=64, height=22, font=("Consolas", 9), wrap="word")
        self.registro.pack(fill="both", expand=True, pady=4)
        if not os.path.exists(EXE):
            self.escribir(f"No encuentro el motor {EXE}")
        self.raiz.after(100, self.bucle)

    # ------------------------------------------------------------------
    def elegir(self):
        r = filedialog.askopenfilename(initialdir=os.path.join(AQUI, 'tableros'),
                                       filetypes=[("Tableros", "*.txt"), ("Todos", "*.*")])
        if r:
            self.var_ruta.set(r)

    def escribir(self, s):
        self.registro.insert("end", s + "\n"); self.registro.see("end")

    def iniciar(self):
        if self.proc:
            return
        ruta = self.var_ruta.get()
        if os.path.exists(self.sol):
            os.remove(self.sol)
        cmd = [EXE, ruta, '-t', str(self.var_hilos.get()), '-o', self.sol]
        if self.var_limite.get() > 0:
            cmd += ['-l', str(self.var_limite.get())]
        if self.var_mezclar.get():
            cmd += ['-s', str(self.var_semilla.get())]
        if self.var_metodo.get() == 'marcos':
            cmd.append('--marcos')
        elif self.var_metodo.get() == 'lados':
            cmd.append('--lados')
        elif self.var_metodo.get() == 'unificado':
            cmd.append('--unificado')
        for p in self.var_pistas.get().replace(' ', '').split(';'):
            if p:
                cmd += ['-P', p]
        self.registro.delete("1.0", "end")
        self.escribir("> " + " ".join(os.path.basename(c) if i == 0 else c for i, c in enumerate(cmd)))
        self.lienzo.delete("all")
        flags = subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0
        self.proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
                                     encoding='utf-8', errors='replace', creationflags=flags)
        self.t0 = time.perf_counter()
        self.b_ini.config(state="disabled"); self.b_det.config(state="normal")
        self.lbl_estado.config(text="Buscando…")
        threading.Thread(target=self.leer_salida, args=(self.proc,), daemon=True).start()

    def leer_salida(self, proc):
        for linea in proc.stdout:
            self.cola.put(linea.rstrip())
        proc.wait()
        self.cola.put(("FIN", proc.returncode))

    def detener(self):
        if self.proc:
            self.proc.kill()
            self.escribir("Detenido por el usuario.")

    def bucle(self):
        try:
            while True:
                m = self.cola.get_nowait()
                if isinstance(m, tuple):
                    self.terminar()
                else:
                    self.escribir(m)
        except queue.Empty:
            pass
        if self.proc and self.t0:
            self.lbl_crono.config(text=f"⏱ {time.perf_counter() - self.t0:.1f} s")
        self.raiz.after(100, self.bucle)

    def terminar(self):
        self.proc = None
        self.b_ini.config(state="normal"); self.b_det.config(state="disabled")
        if os.path.exists(self.sol):
            n, orig = leer_tablero(self.var_ruta.get())
            ns, sol = leer_tablero(self.sol)
            ok, msg = verificar(ns, sol, orig)
            self.escribir(f"Verificación independiente (Python): {msg}")
            self.lbl_estado.config(text="RESUELTO y verificado" if ok else "¡La verificación falló!")
            self.dibujar(ns, sol)
        else:
            self.lbl_estado.config(text="Sin solución (o detenido / límite de tiempo)")

    def dibujar(self, n, grid):
        self.lienzo.delete("all")
        w, h = self.lienzo.winfo_width(), self.lienzo.winfo_height()
        lado = max(4, min(w, h) - 10) / n
        x0, y0 = (w - lado * n) / 2, (h - lado * n) / 2
        for r in range(n):
            for c in range(n):
                p = grid[r * n + c]
                x, y = x0 + c * lado, y0 + r * lado
                cx, cy = x + lado / 2, y + lado / 2
                esq = [(x, y), (x + lado, y), (x + lado, y + lado), (x, y + lado)]
                for s in range(4):
                    a, b = esq[s], esq[(s + 1) % 4]
                    self.lienzo.create_polygon(a[0], a[1], b[0], b[1], cx, cy, fill=color(p[s]), outline="#202020")


if __name__ == "__main__":
    raiz = tk.Tk()
    raiz.geometry("1150x680")
    Ventana(raiz)
    raiz.mainloop()
