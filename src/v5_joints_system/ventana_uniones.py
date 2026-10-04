"""
ventana_uniones.py — Panel del SISTEMA DE UNIONES (idea de Carlos).
El trabajo lo hace el motor en C (e2uniones.exe); esta ventana elige el tablero, ajusta las opciones,
lanza el motor y muestra lo que pasa.

Se abre con doble clic en  Abrir_uniones.bat  (o:  python ventana_uniones.py [TABLERO.txt]).

Dos formas de trabajar (se eligen en la ventana):
  - Ver el tablero mientras trabaja: la ventana pide "fotos" al motor y las dibuja (cámara lenta o rápida).
  - Solo el resultado (máxima velocidad): el motor trabaja sin dibujar nada; el panel solo lee
    una línea de estado cada pocos segundos (no le quita velocidad) y al final dibuja el resultado.
"""
import os, sys, subprocess, time, json, threading, queue, re, glob
import tkinter as tk
from tkinter import ttk, filedialog, messagebox

AQUI = os.path.dirname(os.path.abspath(__file__))
EJEMPLOS = os.path.join(AQUI, 'ejemplos')
CONFIG = os.path.join(AQUI, 'ventana_uniones_config.json')
if os.name == 'nt':
    MOTORES = [os.path.join(AQUI, 'e2uniones.exe'), os.path.join(AQUI, 'e2uniones_compatible.exe')]
else:
    MOTORES = [os.path.join(AQUI, 'e2uniones')]
NUCLEOS = os.cpu_count() or 4
FLAGS = subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0

PALETA = ['#808080', '#e6194b', '#3cb44b', '#ffe119', '#4363d8', '#f58231', '#911eb4', '#46f0f0', '#f032e6',
          '#bcf60c', '#fabebe', '#008080', '#e6beff', '#9a6324', '#fffac8', '#800000', '#aaffc3', '#808000',
          '#ffd8b1', '#000075', '#a9a9a9', '#ff7f50', '#7fff00', '#dc143c', '#00ced1', '#9400d3']

VELOCIDADES = {1: ('cámara muy lenta (1 cambio cada 0,4 s)', 'A 1', 400),
               2: ('cámara lenta (1 cambio cada 0,08 s)', 'A 1', 80),
               3: ('normal (2 000 pasos por cuadro)', 'N 2000', 1),
               4: ('rápida (100 000 pasos por cuadro)', 'N 100000', 1),
               5: ('muy rápida (1 000 000 de pasos por cuadro)', 'N 1000000', 1)}

# Eternity II oficial: las 5 piezas conocidas (casilla fila,columna y colores N E S O ya girados).
# Se reconoce el archivo por su conjunto de piezas; hay dos numeraciones de colores conocidas.
RECORD_MUNDIAL = 470          # uniones de 480 (Joshua Blackwood 2021, igualado en 2024)
OFICIALES = {
    '4f6da4f17365ca1e4ab438ef7b819312426f2f45': [((9, 8), (18, 6, 6, 11)), ((3, 3), (22, 3, 11, 19)), ((3, 14), (22, 20, 22, 16)),
                                                 ((14, 3), (16, 6, 21, 11)), ((14, 14), (4, 21, 15, 12))],
    'e1a8e888b972814d43afea689ec7b913d25ba4f4': [((9, 8), (8, 9, 9, 12)), ((3, 3), (22, 11, 12, 13)), ((3, 14), (22, 18, 22, 20)),
                                                 ((14, 3), (20, 9, 21, 12)), ((14, 14), (16, 21, 15, 17))],
}
def pistas_oficiales(piezas):
    """Si el tablero es el Eternity II oficial, devuelve las 5 piezas fijas como 'f,c,pieza,giros; ...'."""
    import hashlib
    if len(piezas) != 256: return None
    h = hashlib.sha1(repr(sorted(canon(p) for p in piezas)).encode()).hexdigest()
    if h not in OFICIALES: return None
    out = []; usadas = set()
    for (f, c), q in OFICIALES[h]:
        hallada = None
        for k, p in enumerate(piezas):
            if k in usadas: continue
            for g in range(4):
                if rot(p, g) == q: hallada = (k, g); break
            if hallada: break
        if not hallada: return None
        usadas.add(hallada[0]); out.append(f'{f},{c},{hallada[0] + 1},{hallada[1]}')
    return '; '.join(out)

# ------------------------------------------------------------------ tablero y geometría
def leer_tablero(ruta):
    v = []
    for l in open(ruta, encoding='utf-8', errors='replace'):
        if l.lstrip().startswith('#'): continue
        v += [int(x) for x in l.split()]
    n = int(round((len(v) // 4) ** 0.5))
    if n < 2 or n * n * 4 != len(v): raise ValueError('el archivo no parece un tablero cuadrado (4 números por pieza)')
    return n, [tuple(v[4 * i:4 * i + 4]) for i in range(n * n)]

def rot(p, k):
    for _ in range(k % 4): p = (p[3], p[0], p[1], p[2])
    return p
def canon(p): return min(rot(p, k) for k in range(4))

class Geometria:
    """Misma numeración de uniones que el motor en C."""
    def __init__(self, n):
        self.n = n; ids = {}; e = 0
        for r in range(n):
            for c in range(n - 1): ids[('h', r, c)] = e; e += 1
        for r in range(n - 1):
            for c in range(n): ids[('v', r, c)] = e; e += 1
        self.ne = e
        self.lados = [(ids.get(('v', r - 1, c), -1), ids.get(('h', r, c), -1), ids.get(('v', r, c), -1), ids.get(('h', r, c - 1), -1))
                      for r in range(n) for c in range(n)]

def leer_config():
    try: return json.load(open(CONFIG, encoding='utf-8'))
    except Exception: return {}
def guardar_config(d):
    try: json.dump(d, open(CONFIG, 'w', encoding='utf-8'), indent=1, ensure_ascii=False)
    except Exception: pass

# ------------------------------------------------------------------ la aplicación
class App:
    def __init__(self, raiz, tablero_inicial=None):
        self.raiz = raiz; raiz.title('Sistema de uniones — idea de Carlos')
        self.cfg = leer_config()
        self.motor = None; self.modo_vivo = None; self.cola = queue.Queue()
        self.corre = False; self.t0 = None; self.t_fin = None; self.firma = None; self.lim = 10 ** 9
        self.foto = None; self.resuelto = False; self.fin = False; self.estado = 'Elige un tablero y pulsa Iniciar.'
        self.n = 0; self.geo = None; self.objetivo = {}; self.ruta = None; self.fijas = set()
        self.historial = []
        self.armar_ventana()
        ini = tablero_inicial or self.cfg.get('tablero')
        if ini and os.path.exists(ini): self.cargar(ini)
        elif self.lista_tableros: self.cargar(self.lista_tableros[0])
        raiz.protocol('WM_DELETE_WINDOW', self.cerrar)
        self.raiz.after(200, self.revisar_cola)

    # ---------- construcción de la ventana
    def armar_ventana(self):
        r = self.raiz; c = self.cfg
        izq = ttk.Frame(r); izq.grid(row=0, column=0, sticky='nsew', padx=6, pady=6)
        der = ttk.Frame(r); der.grid(row=0, column=1, sticky='nsew', padx=6, pady=6)
        r.columnconfigure(0, weight=1); r.rowconfigure(0, weight=1)

        # tablero
        ft = ttk.LabelFrame(der, text=' Tablero '); ft.grid(row=0, column=0, sticky='we')
        self.lista_tableros = sorted(f for f in glob.glob(os.path.join(EJEMPLOS, '*.txt')) if '_solucion' not in f)
        self.var_tab = tk.StringVar()
        self.combo = ttk.Combobox(ft, textvariable=self.var_tab, state='readonly', width=34,
                                  values=[os.path.basename(f) for f in self.lista_tableros])
        self.combo.grid(row=0, column=0, padx=4, pady=4)
        self.combo.bind('<<ComboboxSelected>>', lambda e: self.cargar(self.lista_tableros[self.combo.current()]))
        ttk.Button(ft, text='Abrir otro…', command=self.abrir_otro).grid(row=0, column=1, padx=4)
        self.lbl_tab = ttk.Label(ft, text='', foreground='#444'); self.lbl_tab.grid(row=1, column=0, columnspan=2, sticky='w', padx=4)

        # opciones
        fo = ttk.LabelFrame(der, text=' Opciones '); fo.grid(row=1, column=0, sticky='we', pady=4)
        def fila(i, texto, w, ayuda=''):
            ttk.Label(fo, text=texto).grid(row=i, column=0, sticky='w', padx=4, pady=1)
            w.grid(row=i, column=1, sticky='w', padx=4)
            if ayuda: ttk.Label(fo, text=ayuda, foreground='#666').grid(row=i, column=2, sticky='w')
        self.var_hilos = tk.IntVar(value=min(int(c.get('hilos', NUCLEOS)), NUCLEOS))
        fila(0, 'Hilos', ttk.Spinbox(fo, from_=1, to=NUCLEOS, textvariable=self.var_hilos, width=5), f'tu PC tiene {NUCLEOS}')
        self.var_modo = tk.StringVar(value=c.get('modo', 'automático'))
        fila(1, 'Modo', ttk.Combobox(fo, textvariable=self.var_modo, state='readonly', width=14,
                                    values=['automático', 'réplicas', 'independiente']), 'automático = réplicas con 4 hilos o más')
        self.var_lim = tk.IntVar(value=int(c.get('limite', 0)))
        fila(2, 'Límite (s)', ttk.Spinbox(fo, from_=0, to=10 ** 7, increment=60, textvariable=self.var_lim, width=8), '0 = sin límite')
        self.var_sem = tk.IntVar(value=int(c.get('semilla', 1)))
        fila(3, 'Semilla', ttk.Spinbox(fo, from_=1, to=10 ** 6, textvariable=self.var_sem, width=8), 'otro punto de partida')
        self.var_pistas = tk.StringVar(value='')
        fila(4, 'Piezas fijas', ttk.Entry(fo, textvariable=self.var_pistas, width=22), 'fila,col,pieza,giros ; …')
        self.var_pin = tk.DoubleVar(value=float(c.get('introducir', 0.3)))
        self.lbl_pin = ttk.Label(fo, text='')
        fila(5, 'Introducir pieza', ttk.Scale(fo, from_=0.0, to=1.0, variable=self.var_pin, length=150,
                                             command=lambda v: self.lbl_pin.config(text=f'{float(v):.2f}')))
        self.lbl_pin.grid(row=5, column=2, sticky='w'); self.lbl_pin.config(text=f'{self.var_pin.get():.2f}')
        self.var_tf = tk.DoubleVar(value=float(c.get('tfria', 0.15))); self.var_tc = tk.DoubleVar(value=float(c.get('tcaliente', 0.8)))
        fe = ttk.Frame(fo)
        ttk.Spinbox(fe, from_=0.01, to=5, increment=0.05, textvariable=self.var_tf, width=6).pack(side='left')
        ttk.Label(fe, text=' a ').pack(side='left')
        ttk.Spinbox(fe, from_=0.01, to=5, increment=0.05, textvariable=self.var_tc, width=6).pack(side='left')
        fila(6, 'Temperaturas', fe, 'fría a caliente (réplicas)')
        self.var_cierre = tk.BooleanVar(value=bool(c.get('cierre', True)))
        self.var_cierre_k = tk.IntVar(value=int(c.get('cierre_k', 0)))
        fc = ttk.Frame(fo)
        ttk.Checkbutton(fc, text='activo, cuando falten ≤', variable=self.var_cierre).pack(side='left')
        ttk.Spinbox(fc, from_=0, to=400, textvariable=self.var_cierre_k, width=5).pack(side='left', padx=3)
        fila(7, 'Cierre exacto', fc, '0 = automático (un cuarto del tablero)')

        # cómo mostrar
        fm = ttk.LabelFrame(der, text=' Cómo trabajar '); fm.grid(row=2, column=0, sticky='we', pady=4)
        self.var_vista = tk.StringVar(value=c.get('vista', 'grafico'))
        ttk.Radiobutton(fm, text='Ver el tablero mientras trabaja', value='grafico', variable=self.var_vista,
                        command=self.actualizar_botones).grid(row=0, column=0, columnspan=2, sticky='w', padx=4)
        ttk.Radiobutton(fm, text='Solo el resultado (máxima velocidad)', value='rapido', variable=self.var_vista,
                        command=self.actualizar_botones).grid(row=1, column=0, columnspan=2, sticky='w', padx=4)
        self.var_vel = tk.IntVar(value=int(c.get('velocidad', 3)))
        ttk.Label(fm, text='Velocidad').grid(row=2, column=0, sticky='w', padx=4)
        self.esc_vel = ttk.Scale(fm, from_=1, to=5, orient='horizontal', length=170, variable=self.var_vel,
                                 command=lambda v: self.lbl_vel.config(text=VELOCIDADES[int(round(float(v)))][0]))
        self.esc_vel.grid(row=2, column=1, sticky='w')
        self.lbl_vel = ttk.Label(fm, text=VELOCIDADES[self.var_vel.get()][0], foreground='#444')
        self.lbl_vel.grid(row=3, column=0, columnspan=2, sticky='w', padx=4)

        # botones
        fb = ttk.Frame(der); fb.grid(row=3, column=0, sticky='w', pady=4)
        self.b_ini = ttk.Button(fb, text='▶ Iniciar', command=self.iniciar); self.b_ini.pack(side='left')
        self.b_pau = ttk.Button(fb, text='⏸ Pausa', command=self.pausar); self.b_pau.pack(side='left', padx=3)
        self.b_paso = ttk.Button(fb, text='Un paso', command=self.un_paso); self.b_paso.pack(side='left')
        self.b_det = ttk.Button(fb, text='⏹ Detener', command=self.detener); self.b_det.pack(side='left', padx=3)

        # panel de información
        fi = ttk.LabelFrame(der, text=' Información '); fi.grid(row=4, column=0, sticky='nsew', pady=4)
        der.rowconfigure(4, weight=1)
        self.lbl_info = ttk.Label(fi, text='', font=('Consolas', 10), justify='left'); self.lbl_info.pack(anchor='w', padx=4)
        self.txt_hist = tk.Text(fi, height=7, width=60, font=('Consolas', 9), state='disabled', bg='#f6f6f6')
        self.txt_hist.pack(fill='both', expand=True, padx=4, pady=4)
        ttk.Label(der, text='Borde negro = pieza REAL · tenue = inventada · azul = fija · rojo = último cambio o zona del cierre',
                  foreground='#555').grid(row=5, column=0, sticky='w')

        self.cv = tk.Canvas(izq, width=560, height=560, bg='white', highlightthickness=0)
        self.cv.pack(fill='both', expand=True)
        self.cv.bind('<Configure>', lambda e: self.dibujar())
        self.actualizar_botones()

    # ---------- tablero
    def abrir_otro(self):
        ini = os.path.dirname(self.ruta) if self.ruta else EJEMPLOS
        f = filedialog.askopenfilename(title='Elige un tablero', initialdir=ini,
                                       filetypes=[('Tableros', '*.txt'), ('Todos', '*.*')])
        if f: self.cargar(f)

    def cargar(self, ruta):
        if self.corre or self.motor_vivo():
            self.detener(silencioso=True)
        try: n, piezas = leer_tablero(ruta)
        except Exception as e:
            messagebox.showerror('Tablero', f'No pude leer {os.path.basename(ruta)}:\n{e}'); return
        self.ruta = os.path.abspath(ruta); self.n = n; self.geo = Geometria(n)
        self.objetivo = {}
        for p in piezas: t = canon(p); self.objetivo[t] = self.objetivo.get(t, 0) + 1
        nombre = os.path.basename(ruta)
        absl = [os.path.abspath(f) for f in self.lista_tableros]
        if self.ruta in absl:
            self.combo['values'] = [os.path.basename(f) for f in self.lista_tableros]
            self.combo.current(absl.index(self.ruta))
        else:
            self.lista_tableros.append(self.ruta)
            self.combo['values'] = [os.path.basename(f) for f in self.lista_tableros]
            self.combo.current(len(self.lista_tableros) - 1)
        texto = f'{n}x{n}: {n * n} piezas, {len(set(c for p in piezas for c in p)) - 1} colores   ({os.path.dirname(self.ruta)})'
        cab = {}
        try:
            for l in open(ruta, encoding='utf-8', errors='replace'):
                if not l.lstrip().startswith('#'): break
                if ':' in l: k, v = l.lstrip('# ').split(':', 1); cab[k.strip().lower()] = v.strip()
        except OSError: pass
        self.desde_archivo = cab.get('inicio', '') == 'desde-archivo'
        po = pistas_oficiales(piezas)
        if 'fijas' in cab:
            self.var_pistas.set(cab['fijas']); self.pistas_auto = True
            texto += '\nPiezas fijas tomadas del archivo: ' + cab['fijas']
            if self.desde_archivo: texto += '\nEmpieza DESDE ESTE TABLERO (tal como está en el archivo), en frío'
        elif po:
            self.var_pistas.set(po); self.pistas_auto = True
            texto += '\nEternity II OFICIAL: se cargaron sus 5 piezas fijas'
        elif getattr(self, 'pistas_auto', False):
            self.var_pistas.set(''); self.pistas_auto = False
        self.lbl_tab.config(text=texto)
        self.foto = None; self.resuelto = False; self.fin = False; self.t0 = None; self.firma = None
        self.fijas = set()
        self.estado = 'Listo. Pulsa Iniciar.'; self.historial = []; self.escribir_hist()
        self.dibujar()

    # ---------- opciones -> argumentos del motor
    def leer_pistas(self):
        out = []
        for trozo in self.var_pistas.get().replace('\n', ';').split(';'):
            trozo = trozo.strip()
            if not trozo: continue
            nums = [int(x) for x in re.split(r'[,\s]+', trozo) if x]
            if len(nums) == 3: nums.append(0)
            if len(nums) != 4: raise ValueError(f'"{trozo}" debe ser fila,columna,pieza,giros')
            f, c, k, g = nums
            if not (1 <= f <= self.n and 1 <= c <= self.n and 1 <= k <= self.n * self.n):
                raise ValueError(f'"{trozo}" está fuera del tablero {self.n}x{self.n}')
            out.append(f'{f},{c},{k},{g}')
        return out

    def argumentos(self):
        pistas = self.leer_pistas()
        modo = {'réplicas': 'replicas', 'independiente': 'independiente'}.get(self.var_modo.get())
        a = [self.ruta, '-t', str(max(1, int(self.var_hilos.get()))), '--semilla', str(int(self.var_sem.get())),
             '--introducir', f'{self.var_pin.get():.3f}', '--tfria', f'{self.var_tf.get():g}', '--tcaliente', f'{self.var_tc.get():g}',
             '-o', self.salida()]
        if modo: a += ['--modo', modo]
        if getattr(self, 'desde_archivo', False): a += ['--desde-archivo']
        if not self.var_cierre.get(): a += ['--cierre', '0']
        elif int(self.var_cierre_k.get()) > 0: a += ['--cierre', str(int(self.var_cierre_k.get()))]
        for p in pistas: a += ['-P', p]
        self.fijas = {(int(p.split(',')[0]) - 1) * self.n + int(p.split(',')[1]) - 1 for p in pistas} or {0}
        return a

    def salida(self): return os.path.splitext(self.ruta)[0] + '_solucion_uniones.txt'
    def archivo_parar(self): return os.path.join(AQUI, '_parar_uniones.txt')

    def guardar_opciones(self):
        self.cfg.update(tablero=self.ruta, hilos=int(self.var_hilos.get()), modo=self.var_modo.get(), limite=int(self.var_lim.get()),
                        semilla=int(self.var_sem.get()), introducir=round(self.var_pin.get(), 3), tfria=self.var_tf.get(),
                        tcaliente=self.var_tc.get(), cierre=bool(self.var_cierre.get()), cierre_k=int(self.var_cierre_k.get()), vista=self.var_vista.get(), velocidad=int(round(self.var_vel.get())))
        guardar_config(self.cfg)

    # ---------- lanzar el motor
    def lanzar(self, extra, servidor):
        ultimo = None
        for exe in MOTORES:
            if not os.path.exists(exe): continue
            try:
                p = subprocess.Popen([exe] + extra, stdin=subprocess.PIPE if servidor else subprocess.DEVNULL,
                                     stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, bufsize=1,
                                     encoding='utf-8', errors='replace', creationflags=FLAGS)
                primera = p.stdout.readline()
                if primera.startswith('INICIO' if servidor else 'Tablero'):
                    return p, primera, os.path.basename(exe)
                ultimo = primera + p.stdout.read()[:300]; p.kill()
            except OSError as e:
                ultimo = str(e)          # p. ej. Windows bloqueó este exe: se prueba el siguiente
        raise RuntimeError(f'El motor no arrancó.\n{ultimo or "No encontré e2uniones.exe en " + AQUI}')

    def motor_vivo(self): return self.motor is not None and self.motor.poll() is None

    def iniciar(self):
        if not self.ruta: self.abrir_otro(); return
        try: args = self.argumentos()
        except ValueError as e: messagebox.showerror('Piezas fijas', str(e)); return
        vista = self.var_vista.get(); firma = (tuple(args), vista)
        if (vista == 'grafico' and self.motor_vivo() and self.modo_vivo == 'grafico' and firma == self.firma
                and not self.resuelto and not self.fin):
            self.corre = True; self.estado = 'Trabajando…'; self.actualizar_botones(); self.ciclo(); return   # seguir tras una pausa
        if self.motor_vivo(): self.detener(silencioso=True)
        self.foto = None; self.resuelto = False; self.fin = False
        self.guardar_opciones(); self.firma = firma; self.historial = []; self.escribir_hist()
        try:
            if os.path.exists(self.archivo_parar()): os.remove(self.archivo_parar())
        except OSError: pass
        lim = int(self.var_lim.get()) or 10 ** 9
        try:
            if vista == 'grafico':
                self.motor, cab, exe = self.lanzar(args + ['--servidor'], True)
                self.leer_foto(self.motor.stdout.readline())
            else:
                self.motor, cab, exe = self.lanzar(args + ['--limite', str(lim), '--cada', '1', '--parar', self.archivo_parar()], False)
                self.hist(cab.strip())
                threading.Thread(target=self.lector, args=(self.motor,), daemon=True).start()
        except RuntimeError as e:
            messagebox.showerror('Motor', str(e)); return
        self.modo_vivo = vista; self.t0 = time.time(); self.t_fin = None; self.lim = lim
        self.estado = f'Trabajando ({exe}, {self.var_hilos.get()} hilos)…'
        self.corre = True; self.actualizar_botones()
        if vista == 'grafico': self.pin_enviado = None; self.ciclo()
        self.dibujar()

    def lector(self, p):
        for l in p.stdout: self.cola.put((p, l.rstrip('\n')))
        self.cola.put((p, None))

    # ---------- modo gráfico
    def pedir(self, orden):
        self.motor.stdin.write(orden + '\n'); self.motor.stdin.flush()
        self.leer_foto(self.motor.stdout.readline())

    def leer_foto(self, linea):
        l = linea.split('|'); cab = l[0].split()
        if len(l) < 4 or not cab or cab[0] != 'F': raise RuntimeError('respuesta rara del motor: ' + linea[:100])
        self.foto = {'pasos': int(cab[1]), 'acept': int(cab[2]), 'reales': int(cab[3]), 'mejor': int(cab[4]),
                     'temp': float(cab[5]), 'hilo': int(cab[7]), 'col': list(map(int, l[1].split())),
                     'toc': set(map(int, l[2].split())), 'por_hilo': list(map(int, l[3].split()))}
        if len(cab) >= 11: self.foto.update(conv=int(cab[8]), conv_mejor=int(cab[9]), ne=int(cab[10]))

    def ciclo(self):
        if not self.corre or self.modo_vivo != 'grafico': return
        try:
            pin = round(self.var_pin.get(), 3)
            if pin != self.pin_enviado: self.pin_enviado = pin; self.pedir(f'I {pin:.3f}')
            _, orden, ms = VELOCIDADES[int(round(self.var_vel.get()))]
            self.pedir(orden)
        except Exception as e:
            self.corre = False; self.estado = f'El motor se cerró: {e}'; self.actualizar_botones(); self.dibujar(); return
        if self.foto['reales'] == self.n * self.n:
            self.corre = False; self.resuelto = True; self.t_fin = time.time()
            self.estado = '¡RESUELTO! Solución verificada y guardada en ' + os.path.basename(self.salida())
            self.actualizar_botones()
        elif time.time() - self.t0 > self.lim:
            self.corre = False; self.fin = True; self.t_fin = time.time()
            self.estado = 'Se acabó el tiempo límite.'; self.actualizar_botones()
        self.dibujar()
        if self.corre: self.raiz.after(ms, self.ciclo)

    def un_paso(self):
        if self.var_vista.get() != 'grafico' or self.resuelto: return
        if not (self.motor_vivo() and self.modo_vivo == 'grafico') or self.fin:
            self.iniciar()
            if not self.motor_vivo(): return
        self.corre = False
        try: self.pedir('A 1')
        except Exception as e: self.estado = f'El motor se cerró: {e}'; self.dibujar(); return
        if self.foto['reales'] == self.n * self.n:
            self.resuelto = True; self.t_fin = time.time(); self.estado = '¡RESUELTO! Solución guardada en ' + os.path.basename(self.salida())
        else: self.estado = 'En pausa (un paso). "Iniciar" sigue desde aquí.'
        self.actualizar_botones(); self.dibujar()

    def pausar(self):
        if self.modo_vivo == 'grafico' and self.corre:
            self.corre = False; self.estado = 'En pausa. "Iniciar" sigue desde aquí.'
            self.actualizar_botones(); self.dibujar()

    # ---------- modo rápido: solo lee líneas de estado
    def revisar_cola(self):
        try:
            while True:
                p, l = self.cola.get_nowait()
                if p is not self.motor: continue            # restos de un motor anterior
                if l is None:
                    if self.corre:
                        self.corre = False; self.t_fin = self.t_fin or time.time()
                        if not self.resuelto and not self.fin: self.estado = 'El motor terminó.'
                    self.actualizar_botones(); self.dibujar()
                    continue
                self.procesar_linea(l)
        except queue.Empty: pass
        if self.corre and self.modo_vivo == 'rapido': self.dibujar_info()
        self.raiz.after(250, self.revisar_cola)

    def procesar_linea(self, l):
        if l.startswith('FINAL'):
            cab, cols = l.split('|', 1); c = cab.split()
            F = self.foto or {}
            self.foto = {'pasos': F.get('pasos', 0), 'acept': 0, 'reales': int(c[2]), 'mejor': F.get('mejor', int(c[2])),
                         'temp': 0, 'hilo': int(c[1]), 'col': list(map(int, cols.split())), 'toc': set(),
                         'por_hilo': F.get('por_hilo', []), 'mps': F.get('mps', 0),
                         'conv': F.get('conv'), 'conv_mejor': F.get('conv_mejor'), 'ne': F.get('ne')}
            self.dibujar(); return
        self.hist(l)
        if l.startswith('CIERRE') and 'salió del cierre' in l and self.resuelto:
            self.estado += '\n(la última parte la completó el cierre exacto)'
        mc = re.search(r'convertido: (\d+)/(\d+)', l)
        if l.startswith('CONVERTIDO'):
            m2 = re.search(r'CONVERTIDO (\d+)/(\d+) \(el mejor visto: (\d+)', l)
            if m2 and self.foto is not None: self.foto.update(conv=int(m2.group(1)), conv_mejor=int(m2.group(3)), ne=int(m2.group(2)))
            return
        m = re.search(r'pasos (\d+) \(([\d.]+) M/s\).*?ahora: (\d+)/\d+ \| mejor de todos: (\d+) \|(?: convertido: \d+/\d+ \|)? reales por hilo:([\d ]+)', l)
        if m:
            self.foto = {'pasos': int(m.group(1)), 'acept': 0, 'reales': int(m.group(3)), 'mejor': int(m.group(4)), 'temp': 0,
                         'hilo': -1, 'col': None, 'toc': set(), 'por_hilo': list(map(int, m.group(5).split())),
                         'mps': float(m.group(2))}
            if mc: self.foto.update(conv_mejor=int(mc.group(1)), ne=int(mc.group(2)))
            return
        if l.startswith('RESULTADO'):
            d = dict(x.split('=', 1) for x in l.split()[1:] if '=' in x)
            self.t_fin = time.time(); self.corre = False
            if self.foto is None: self.foto = {'pasos': 0, 'acept': 0, 'reales': 0, 'mejor': 0, 'temp': 0, 'hilo': -1, 'col': None, 'toc': set(), 'por_hilo': []}
            self.foto['pasos'] = int(d.get('pasos', 0)); self.foto['mejor'] = int(d.get('mejor', '0/0').split('/')[0])
            self.foto['mps'] = float(d.get('pasos_por_s', 0)) / 1e6
            seg = float(d.get('s', 0))
            if d.get('resuelto') == '1':
                self.resuelto = True; self.foto['reales'] = self.n * self.n
                self.estado = f'¡RESUELTO en {seg:.2f} s! Guardado en ' + os.path.basename(self.salida())
            else:
                self.fin = True
                self.estado = f'Sin resolver tras {seg:.0f} s. Se dibuja el mejor tablero que se alcanzó.'
            self.actualizar_botones()

    def hist(self, l):
        self.historial.append(l); self.historial = self.historial[-200:]; self.escribir_hist()
    def escribir_hist(self):
        self.txt_hist.config(state='normal'); self.txt_hist.delete('1.0', 'end')
        self.txt_hist.insert('end', '\n'.join(self.historial)); self.txt_hist.see('end'); self.txt_hist.config(state='disabled')

    # ---------- detener / cerrar
    def detener(self, silencioso=False):
        self.corre = False
        if self.motor_vivo():
            if self.modo_vivo == 'grafico':
                try: self.motor.stdin.write('Q\n'); self.motor.stdin.flush()
                except Exception: pass
                try: self.motor.wait(timeout=2)
                except Exception: self.motor.kill()
                self.fin = True; self.t_fin = time.time()
                if not silencioso: self.estado = 'Detenido. "Iniciar" empieza de nuevo.'
            elif silencioso: self.motor.kill()
            else:
                try: open(self.archivo_parar(), 'w').close()    # el motor lo ve, para y manda el resultado
                except OSError: self.motor.kill()
                self.corre = True; self.estado = 'Deteniendo… (el motor manda el resultado)'
        if silencioso: self.motor = None
        self.actualizar_botones(); self.dibujar()

    def cerrar(self):
        try: self.guardar_opciones()
        except Exception: pass
        if self.motor_vivo():
            try:
                if self.modo_vivo == 'grafico': self.motor.stdin.write('Q\n'); self.motor.stdin.flush()
            except Exception: pass
            try: self.motor.kill()
            except Exception: pass
        self.raiz.destroy()

    def actualizar_botones(self):
        graf = self.var_vista.get() == 'grafico'
        self.b_pau.state(['!disabled'] if (graf and self.corre and self.modo_vivo == 'grafico') else ['disabled'])
        self.b_paso.state(['!disabled'] if (graf and not self.corre) else ['disabled'])
        self.b_det.state(['!disabled'] if self.motor_vivo() else ['disabled'])
        self.b_ini.state(['disabled'] if self.corre else ['!disabled'])
        self.esc_vel.state(['!disabled'] if graf else ['disabled'])

    # ---------- dibujo
    def dibujar_info(self):
        F = self.foto; nn = self.n * self.n
        dt = ((self.t_fin or time.time()) - self.t0) if self.t0 else 0
        lineas = [self.estado, '']
        if F:
            lineas.append(f'Piezas reales: {F["reales"]} / {nn}    mejor de todos: {F["mejor"]}')
            if F.get('ne'):
                ne = F['ne']; cm = F.get('conv_mejor') or F.get('conv') or 0
                linea = f'Puntaje normal (todas reales): mejor {cm} / {ne} uniones'
                if F.get('conv') is not None and self.modo_vivo == 'grafico': linea += f'   (este tablero: {F["conv"]})'
                if ne == 480: linea += f'\n   récord mundial: {RECORD_MUNDIAL} / 480  →  ' + ('¡¡SUPERADO!! Revisa el archivo _convertido.txt' if cm > RECORD_MUNDIAL else ('igualado' if cm == RECORD_MUNDIAL else f'faltan {RECORD_MUNDIAL - cm}'))
                lineas.append(linea)
            v = F.get('mps') or (F['pasos'] / dt / 1e6 if dt > 0 else 0)
            lineas.append(f'Pasos (todos los hilos): {F["pasos"]:,}   ({v:.1f} millones/s)')
            if F.get('hilo', -1) >= 0:
                extra = f'   temperatura {F["temp"]:.3f}' if self.modo_vivo == 'grafico' and self.corre else ''
                lineas.append(f'Dibujando el hilo {F["hilo"] + 1}{extra}')
            ph = F.get('por_hilo') or []
            if ph:
                lineas.append('Reales de cada hilo:')
                for i in range(0, len(ph), 6): lineas.append('   ' + ' '.join(f'{x:3d}' for x in ph[i:i + 6]))
        lineas.append(f'Tiempo: {dt:.1f} s')
        self.lbl_info.config(text='\n'.join(lineas))

    def dibujar(self):
        self.dibujar_info()
        cv = self.cv; cv.delete('all')
        if not self.n: return
        n = self.n
        W = max(100, min(cv.winfo_width(), cv.winfo_height()) - 8); L = W / n
        F = self.foto; colr = F['col'] if F and F.get('col') else None
        fijas = self.fijas or {0}
        if colr is None:                         # sin foto: rejilla y un aviso
            for r in range(n):
                for c in range(n):
                    x0, y0 = c * L + 4, r * L + 4
                    cv.create_rectangle(x0, y0, x0 + L, y0 + L, outline='#ccc',
                                        fill='#dfe8ff' if r * n + c in fijas else '#f4f4f4')
            msg = ('Trabajando a toda velocidad…\nel tablero se dibuja al terminar' if self.corre and self.modo_vivo == 'rapido'
                   else 'Pulsa ▶ Iniciar')
            cv.create_text(W / 2 + 4, W / 2 + 4, text=msg, font=('Segoe UI', 14), fill='#555', justify='center')
            return
        geo = self.geo; cnt = {}; tip = []
        for rc in range(n * n):
            p = tuple(0 if e < 0 else colr[e] for e in geo.lados[rc]); t = canon(p); tip.append((p, t)); cnt[t] = cnt.get(t, 0) + 1
        for rc in range(n * n):
            r, c = divmod(rc, n); p, t = tip[rc]
            real = t in self.objetivo and cnt[t] <= self.objetivo[t]
            x0, y0 = c * L + 4, r * L + 4; x1, y1 = x0 + L, y0 + L; xm, ym = (x0 + x1) / 2, (y0 + y1) / 2
            tri = [(x0, y0, x1, y0), (x1, y0, x1, y1), (x1, y1, x0, y1), (x0, y1, x0, y0)]
            for d in range(4):
                a1, b1, a2, b2 = tri[d]
                cv.create_polygon(a1, b1, a2, b2, xm, ym, fill=PALETA[p[d] % len(PALETA)],
                                  outline='#ffffff' if real else '#dddddd', stipple='' if real else 'gray50')
            if rc in fijas: cv.create_rectangle(x0 + 2, y0 + 2, x1 - 2, y1 - 2, outline='#1f4fd8', width=3)
            elif real: cv.create_rectangle(x0 + 1, y0 + 1, x1 - 1, y1 - 1, outline='black', width=2)
            if rc in F['toc']: cv.create_rectangle(x0 + 4, y0 + 4, x1 - 4, y1 - 4, outline='red', width=2)

def main():
    raiz = tk.Tk()
    try: raiz.geometry('1150x720')
    except Exception: pass
    app = App(raiz, sys.argv[1] if len(sys.argv) > 1 and os.path.exists(sys.argv[1]) else None)
    if os.environ.get('E2U_AUTO'):          # solo para pruebas automáticas: E2U_AUTO=ms[,rapido|grafico[,ms_detener]]
        partes = os.environ['E2U_AUTO'].split(',')
        if len(partes) > 1: app.var_vista.set(partes[1]); app.actualizar_botones()
        raiz.after(300, app.iniciar)
        if len(partes) > 2: raiz.after(int(partes[2]), app.detener)
        raiz.after(int(partes[0]), app.cerrar)
    raiz.mainloop()

if __name__ == '__main__':
    main()
