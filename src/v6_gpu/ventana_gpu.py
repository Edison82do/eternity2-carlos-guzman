"""
ventana_gpu.py — Panel de la VERSIÓN 6 (tarjeta de video), proyecto de Carlos Edison Guzman Marte.
Recocido masivo: miles de tableros a la vez en la tarjeta, siempre con las piezas reales, buscando que
encajen todas las uniones (el puntaje del récord: X de 480 en el Eternity II).

Dos métodos: RECOCIDO (adivina mejorando el puntaje, muy rápido pero sin garantía) y BÚSQUEDA EXACTA
(recorre todo el árbol con poda, repartido en miles de hilos: si hay solución la encuentra, y si termina sin
hallarla, eso prueba que no la hay).

Se abre con doble clic en Abrir_v6.bat  (o: python ventana_gpu.py [TABLERO.txt]).
"""
import os, sys, subprocess, time, json, threading, queue, re, glob, hashlib
import tkinter as tk
from tkinter import ttk, filedialog, messagebox

AQUI = os.path.dirname(os.path.abspath(__file__))
MOTOR = os.path.join(AQUI, 'gpu_recocido.py')
MOTOR_EXACTO = os.path.join(AQUI, 'gpu_exacto.py')
CONFIG = os.path.join(AQUI, 'ventana_gpu_config.json')
FLAGS = subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0
RECORD_MUNDIAL = 470
CARPETAS_TABLEROS = [os.path.join(AQUI, 'tableros'), os.path.join(AQUI, '..', 'sistema_uniones_v5', 'ejemplos')]

PALETA = ['#808080', '#e6194b', '#3cb44b', '#ffe119', '#4363d8', '#f58231', '#911eb4', '#46f0f0', '#f032e6',
          '#bcf60c', '#fabebe', '#008080', '#e6beff', '#9a6324', '#fffac8', '#800000', '#aaffc3', '#808000',
          '#ffd8b1', '#000075', '#a9a9a9', '#ff7f50', '#7fff00', '#dc143c', '#00ced1', '#9400d3']

def rot(p, k):
    for _ in range(k % 4): p = (p[3], p[0], p[1], p[2])
    return p
def canon(p): return min(rot(p, k) for k in range(4))

def leer_tablero(ruta):
    v = []; cab = {}
    for l in open(ruta, encoding='utf-8', errors='replace'):
        if l.lstrip().startswith('#'):
            if ':' in l: k, x = l.lstrip('# ').split(':', 1); cab[k.strip().lower()] = x.strip()
            continue
        v += [int(x) for x in l.split()]
    n = int(round((len(v) // 4) ** 0.5))
    if n < 2 or n * n * 4 != len(v): raise ValueError('el archivo no parece un tablero cuadrado (4 números por pieza)')
    return n, [tuple(v[4 * i:4 * i + 4]) for i in range(n * n)], cab

OFICIALES = {
    '4f6da4f17365ca1e4ab438ef7b819312426f2f45': [((9, 8), (18, 6, 6, 11)), ((3, 3), (22, 3, 11, 19)), ((3, 14), (22, 20, 22, 16)),
                                                 ((14, 3), (16, 6, 21, 11)), ((14, 14), (4, 21, 15, 12))],
    'e1a8e888b972814d43afea689ec7b913d25ba4f4': [((9, 8), (8, 9, 9, 12)), ((3, 3), (22, 11, 12, 13)), ((3, 14), (22, 18, 22, 20)),
                                                 ((14, 3), (20, 9, 21, 12)), ((14, 14), (16, 21, 15, 17))],
}
def pistas_oficiales(piezas):
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

def leer_config():
    try: return json.load(open(CONFIG, encoding='utf-8'))
    except Exception: return {}
def guardar_config(d):
    try: json.dump(d, open(CONFIG, 'w', encoding='utf-8'), indent=1, ensure_ascii=False)
    except Exception: pass

class App:
    def __init__(self, raiz, inicial=None):
        self.raiz = raiz; raiz.title('Versión 6 — tarjeta de video (Carlos Edison Guzman Marte)')
        self.cfg = leer_config(); self.motor = None; self.cola = queue.Queue()
        self.corre = False; self.t0 = None; self.t_fin = None; self.tab = None; self.n = 0; self.ruta = None
        self.estado = 'Elige un tablero y pulsa Iniciar.'; self.info = {}; self.historial = []; self.fijas = set(); self.desde_archivo = False
        self.armar()
        ini = inicial or self.cfg.get('tablero')
        if ini and os.path.exists(ini): self.cargar(ini)
        elif self.lista: self.cargar(self.lista[0])
        raiz.protocol('WM_DELETE_WINDOW', self.cerrar); raiz.after(200, self.revisar)

    def armar(self):
        r = self.raiz; c = self.cfg
        izq = ttk.Frame(r); izq.grid(row=0, column=0, sticky='nsew', padx=6, pady=6)
        der = ttk.Frame(r); der.grid(row=0, column=1, sticky='nsew', padx=6, pady=6)
        r.columnconfigure(0, weight=1); r.rowconfigure(0, weight=1)
        ft = ttk.LabelFrame(der, text=' Tablero '); ft.grid(row=0, column=0, sticky='we')
        self.lista = []
        for carpeta in CARPETAS_TABLEROS:
            self.lista += sorted(f for f in glob.glob(os.path.join(carpeta, '*.txt')) if '_solucion' not in f and '_convertido' not in f and '_mejor' not in f)
        self.combo = ttk.Combobox(ft, state='readonly', width=40, values=[os.path.basename(f) for f in self.lista])
        self.combo.grid(row=0, column=0, padx=4, pady=4)
        self.combo.bind('<<ComboboxSelected>>', lambda e: self.cargar(self.lista[self.combo.current()]))
        ttk.Button(ft, text='Abrir otro…', command=self.abrir_otro).grid(row=0, column=1, padx=4)
        self.lbl_tab = ttk.Label(ft, text='', foreground='#444', justify='left'); self.lbl_tab.grid(row=1, column=0, columnspan=2, sticky='w', padx=4)

        fmet = ttk.LabelFrame(der, text=' Método '); fmet.grid(row=1, column=0, sticky='we', pady=4)
        self.v_met = tk.StringVar(value=c.get('metodo', 'recocido'))
        ttk.Radiobutton(fmet, text='Recocido: miles de tableros mejorando el puntaje (rápido, sin garantía)', value='recocido',
                        variable=self.v_met, command=self.ver_metodo).grid(row=0, column=0, sticky='w', padx=4)
        ttk.Radiobutton(fmet, text='Búsqueda exacta: recorre TODO el árbol con poda (si hay solución, la halla)', value='exacta',
                        variable=self.v_met, command=self.ver_metodo).grid(row=1, column=0, sticky='w', padx=4)
        fx = ttk.Frame(fmet); fx.grid(row=2, column=0, sticky='w', padx=20)
        ttk.Label(fx, text='Orden de casillas:').pack(side='left')
        self.v_orden = tk.StringVar(value=c.get('orden', 'filas'))
        self.cb_orden = ttk.Combobox(fx, state='readonly', width=9, values=['filas', 'marco', 'espiral'], textvariable=self.v_orden); self.cb_orden.pack(side='left', padx=4)
        ttk.Label(fx, text='  Hilos en la tarjeta:').pack(side='left')
        self.v_hgpu = tk.IntVar(value=int(c.get('hilos_gpu', 16384)))
        self.sp_hgpu = ttk.Spinbox(fx, from_=1024, to=262144, increment=4096, textvariable=self.v_hgpu, width=8); self.sp_hgpu.pack(side='left', padx=4)

        fg = ttk.LabelFrame(der, text=' Opciones generales '); fg.grid(row=2, column=0, sticky='we', pady=4)
        fo = ttk.LabelFrame(der, text=' Opciones del recocido '); fo.grid(row=5, column=0, sticky='we', pady=4); self.fo = fo
        def fila(i, t, w, ayuda=''):
            ttk.Label(fo, text=t).grid(row=i, column=0, sticky='w', padx=4, pady=1); w.grid(row=i, column=1, sticky='w', padx=4)
            if ayuda: ttk.Label(fo, text=ayuda, foreground='#666').grid(row=i, column=2, sticky='w')
        self.v_tab = tk.IntVar(value=int(c.get('tableros', 30720)))
        fila(0, 'Tableros a la vez', ttk.Spinbox(fo, from_=256, to=262144, increment=1024, textvariable=self.v_tab, width=8), 'en la tarjeta (30 720 llena la GTX 1070)')
        self.v_lim = tk.IntVar(value=int(c.get('limite', 0)))
        ttk.Label(fg, text='Límite (s)').grid(row=0, column=0, sticky='w', padx=4, pady=1)
        ttk.Spinbox(fg, from_=0, to=10 ** 7, increment=60, textvariable=self.v_lim, width=8).grid(row=0, column=1, sticky='w', padx=4)
        ttk.Label(fg, text='0 = sin límite', foreground='#666').grid(row=0, column=2, sticky='w')
        self.v_sem = tk.IntVar(value=int(c.get('semilla', 1)))
        fila(2, 'Semilla', ttk.Spinbox(fo, from_=1, to=10 ** 6, textvariable=self.v_sem, width=8), 'otro punto de partida')
        self.v_pistas = tk.StringVar(value='')
        ttk.Label(fg, text='Piezas fijas').grid(row=1, column=0, sticky='w', padx=4, pady=1)
        ttk.Entry(fg, textvariable=self.v_pistas, width=30).grid(row=1, column=1, sticky='w', padx=4)
        ttk.Label(fg, text='fila,col,pieza,giros ; …', foreground='#666').grid(row=1, column=2, sticky='w')
        self.v_tf = tk.DoubleVar(value=float(c.get('tfria', 0.15))); self.v_tc = tk.DoubleVar(value=float(c.get('tcaliente', 1.0)))
        fe = ttk.Frame(fo)
        ttk.Spinbox(fe, from_=0.01, to=5, increment=0.05, textvariable=self.v_tf, width=6).pack(side='left')
        ttk.Label(fe, text=' a ').pack(side='left')
        ttk.Spinbox(fe, from_=0.01, to=5, increment=0.05, textvariable=self.v_tc, width=6).pack(side='left')
        fila(4, 'Temperaturas', fe, 'fría a caliente (réplicas)')
        self.v_esc = tk.IntVar(value=int(c.get('escalones', 32)))
        fila(5, 'Escalones', ttk.Spinbox(fo, from_=2, to=256, textvariable=self.v_esc, width=6), 'temperaturas distintas por grupo')
        self.v_cie = tk.BooleanVar(value=bool(c.get('cierre', True))); self.v_ciek = tk.IntVar(value=int(c.get('cierre_k', 0)))
        fc = ttk.Frame(fo)
        ttk.Checkbutton(fc, text='activo, cuando falten ≤', variable=self.v_cie).pack(side='left')
        ttk.Spinbox(fc, from_=0, to=200, textvariable=self.v_ciek, width=5).pack(side='left', padx=3)
        ttk.Label(fc, text='uniones').pack(side='left')
        fila(6, 'Cierre exacto', fc, '0 = automático; lo hace el procesador')

        fm = ttk.LabelFrame(der, text=' Cómo trabajar '); fm.grid(row=6, column=0, sticky='we', pady=4)
        self.v_ver = tk.StringVar(value=c.get('ver', 'ver'))
        ttk.Radiobutton(fm, text='Ver el mejor tablero mientras trabaja (cada segundo)', value='ver', variable=self.v_ver).grid(row=0, column=0, sticky='w', padx=4)
        ttk.Radiobutton(fm, text='Solo el resultado (un poco más rápido)', value='rapido', variable=self.v_ver).grid(row=1, column=0, sticky='w', padx=4)

        fb = ttk.Frame(der); fb.grid(row=3, column=0, sticky='w', pady=4)
        self.b_ini = ttk.Button(fb, text='▶ Iniciar', command=self.iniciar); self.b_ini.pack(side='left')
        self.b_det = ttk.Button(fb, text='⏹ Detener', command=self.detener); self.b_det.pack(side='left', padx=4)

        fi = ttk.LabelFrame(der, text=' Información '); fi.grid(row=4, column=0, sticky='nsew', pady=4); der.rowconfigure(4, weight=1)
        self.lbl_info = ttk.Label(fi, text='', font=('Consolas', 10), justify='left'); self.lbl_info.pack(anchor='w', padx=4)
        self.txt = tk.Text(fi, height=7, width=64, font=('Consolas', 9), state='disabled', bg='#f6f6f6'); self.txt.pack(fill='both', expand=True, padx=4, pady=4)
        ttk.Label(der, text='Marca roja = unión que NO encaja · borde azul = pieza fija', foreground='#555').grid(row=7, column=0, sticky='w')
        self.cv = tk.Canvas(izq, width=560, height=560, bg='white', highlightthickness=0); self.cv.pack(fill='both', expand=True)
        self.cv.bind('<Configure>', lambda e: self.dibujar())
        self.botones(); self.ver_metodo()

    def ver_metodo(self):
        ex = self.v_met.get() == 'exacta'
        for w in (self.cb_orden, self.sp_hgpu): w.state(['!disabled'] if ex else ['disabled'])
        for w in self.fo.winfo_children():
            try: w.state(['disabled'] if ex else ['!disabled'])
            except Exception:
                for x in w.winfo_children():
                    try: x.state(['disabled'] if ex else ['!disabled'])
                    except Exception: pass

    def abrir_otro(self):
        f = filedialog.askopenfilename(title='Elige un tablero', initialdir=os.path.dirname(self.ruta) if self.ruta else AQUI,
                                       filetypes=[('Tableros', '*.txt'), ('Todos', '*.*')])
        if f: self.cargar(f)

    def cargar(self, ruta):
        if self.motor_vivo(): self.detener(silencioso=True)
        try: n, piezas, cab = leer_tablero(ruta)
        except Exception as e: messagebox.showerror('Tablero', f'No pude leer {os.path.basename(ruta)}:\n{e}'); return
        self.ruta = os.path.abspath(ruta); self.n = n; self.piezas = piezas
        absl = [os.path.abspath(f) for f in self.lista]
        if self.ruta not in absl: self.lista.append(self.ruta); absl.append(self.ruta)
        self.combo['values'] = [os.path.basename(f) for f in self.lista]; self.combo.current(absl.index(self.ruta))
        texto = f'{n}x{n}: {n * n} piezas, {2 * n * (n - 1)} uniones   ({os.path.dirname(self.ruta)})'
        self.desde_archivo = cab.get('inicio', '') == 'desde-archivo'
        po = pistas_oficiales(piezas)
        if 'fijas' in cab: self.v_pistas.set(cab['fijas']); texto += '\nPiezas fijas tomadas del archivo: ' + cab['fijas']
        elif po: self.v_pistas.set(po); texto += '\nEternity II OFICIAL: se cargaron sus 5 piezas fijas'
        else: self.v_pistas.set('')
        if self.desde_archivo: texto += '\nEmpieza DESDE ESTE TABLERO (tal como está en el archivo)'
        self.lbl_tab.config(text=texto)
        self.tab = None; self.info = {}; self.t0 = None; self.estado = 'Listo. Pulsa Iniciar.'; self.historial = []; self.escribir()
        self.dibujar()

    def pistas(self):
        out = []
        for t in self.v_pistas.get().replace('\n', ';').split(';'):
            t = t.strip()
            if not t: continue
            nums = [int(x) for x in re.split(r'[,\s]+', t) if x]
            if len(nums) == 3: nums.append(0)
            if len(nums) != 4: raise ValueError(f'"{t}" debe ser fila,columna,pieza,giros')
            out.append(','.join(map(str, nums)))
        self.fijas = {(int(p.split(',')[0]) - 1) * self.n + int(p.split(',')[1]) - 1 for p in out}
        return out

    def salida(self): return os.path.splitext(self.ruta)[0] + ('_solucion_v6.txt' if self.v_met.get() == 'exacta' else '_mejor_v6.txt')
    def archivo_parar(self): return os.path.join(AQUI, '_parar_v6.txt')
    def motor_vivo(self): return self.motor is not None and self.motor.poll() is None

    def iniciar(self):
        if not self.ruta: self.abrir_otro(); return
        try: pist = self.pistas()
        except ValueError as e: messagebox.showerror('Piezas fijas', str(e)); return
        self.cfg.update(tablero=self.ruta, tableros=int(self.v_tab.get()), limite=int(self.v_lim.get()), semilla=int(self.v_sem.get()),
                        tfria=self.v_tf.get(), tcaliente=self.v_tc.get(), escalones=int(self.v_esc.get()), ver=self.v_ver.get(), cierre=bool(self.v_cie.get()), cierre_k=int(self.v_ciek.get()),
                        metodo=self.v_met.get(), orden=self.v_orden.get(), hilos_gpu=int(self.v_hgpu.get()))
        guardar_config(self.cfg)
        try:
            if os.path.exists(self.archivo_parar()): os.remove(self.archivo_parar())
        except OSError: pass
        lim = int(self.v_lim.get()) or 10 ** 9
        self.exacta = self.v_met.get() == 'exacta'
        if self.exacta:
            cmd = [sys.executable, '-u', MOTOR_EXACTO, self.ruta, '--limite', str(int(self.v_lim.get())), '--orden', self.v_orden.get(),
                   '--hilos-gpu', str(int(self.v_hgpu.get())), '--cada', '1', '--parar', self.archivo_parar(), '-o', self.salida()]
            if os.environ.get('E2V6_CPU'): cmd += ['--dispositivo', 'cpu']
            for p in pist: cmd += ['-P', p]
            return self.lanzar(cmd)
        cmd = [sys.executable, '-u', MOTOR, self.ruta, '--tableros', str(int(self.v_tab.get())), '--limite', str(lim),
               '--semilla', str(int(self.v_sem.get())), '--tfria', f'{self.v_tf.get():g}', '--tcaliente', f'{self.v_tc.get():g}',
               '--escalones', str(int(self.v_esc.get())), '--cada', '1', '--parar', self.archivo_parar(), '-o', self.salida()]
        if self.v_ver.get() == 'ver': cmd.append('--mostrar')
        if not self.v_cie.get(): cmd += ['--cierre', '0']
        elif int(self.v_ciek.get()) > 0: cmd += ['--cierre', str(int(self.v_ciek.get()))]
        if self.desde_archivo: cmd.append('--desde-archivo')
        if os.environ.get('E2V6_CPU'): cmd += ['--dispositivo', 'cpu']
        for p in pist: cmd += ['-P', p]
        self.lanzar(cmd)

    def lanzar(self, cmd):
        env = dict(os.environ, PYTHONIOENCODING='utf-8', PYTHONWARNINGS='ignore')
        self.motor = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, bufsize=1, encoding='utf-8',
                                      errors='replace', creationflags=FLAGS, env=env, cwd=AQUI)
        threading.Thread(target=self.lector, args=(self.motor,), daemon=True).start()
        self.t0 = time.time(); self.t_fin = None; self.corre = True; self.info = {}; self.historial = []
        self.estado = 'Arrancando la tarjeta (los primeros segundos compila el núcleo)…'; self.botones(); self.dibujar()

    def lector(self, p):
        for l in p.stdout: self.cola.put((p, l.rstrip('\n')))
        self.cola.put((p, None))

    def revisar(self):
        try:
            while True:
                p, l = self.cola.get_nowait()
                if p is not self.motor: continue
                if l is None:
                    self.corre = False; self.t_fin = self.t_fin or time.time()
                    if not self.estado.startswith(('¡', 'Sin resolver')): self.estado = 'El motor terminó.'
                    self.botones(); self.dibujar(); continue
                self.linea(l)
        except queue.Empty: pass
        if self.corre: self.mostrar_info()
        self.raiz.after(250, self.revisar)

    def linea(self, l):
        if l.startswith('TABLERO '):
            v = list(map(int, l.split()[1:]))
            if len(v) == 4 * self.n * self.n: self.tab = [tuple(v[4 * i:4 * i + 4]) for i in range(self.n * self.n)]; self.dibujar()
            return
        if 'Warning' in l or 'warnings.warn' in l: return
        self.historial.append(l); self.historial = self.historial[-200:]; self.escribir()
        if l.startswith('Tablero '): self.estado = 'Trabajando en la tarjeta…'
        m = re.search(r'pasos ([\d,]+) \(([\d.]+) M/s\) \| mejor ahora: (\d+)/(\d+) \| mejor de todos: (\d+)/\d+', l)
        if m: self.info = dict(pasos=int(m.group(1).replace(',', '')), mps=float(m.group(2)), ahora=int(m.group(3)), ne=int(m.group(4)), mejor=int(m.group(5)))
        m = re.search(r'nodos ([\d,]+) \(([\d.]+) M/s\) \| prefijos agotados ([\d,]+)/([\d,]+) \(([\d.]+)%\) \| faltan ~([\d.inf]+) h.*máximo colocado (\d+) de (\d+)', l)
        if m: self.info = dict(nodos=int(m.group(1).replace(',', '')), mps=float(m.group(2)), hechos=int(m.group(3).replace(',', '')),
                               npref=int(m.group(4).replace(',', '')), pct=float(m.group(5)), eta=m.group(6), maxd=int(m.group(7)), NN=int(m.group(8)))
        m = re.search(r'Reparto: ([\d,]+) prefijos de (\d+) casillas', l)
        if m: self.estado = f'Búsqueda exacta en marcha: el árbol se repartió en {m.group(1)} trozos de {m.group(2)} casillas.'
        if l.startswith('RESULTADO') and getattr(self, 'exacta', False):
            d = dict(x.split('=', 1) for x in l.split()[1:] if '=' in x)
            self.t_fin = time.time(); self.corre = False
            self.info['nodos'] = int(d.get('nodos', 0))
            if d.get('resuelto') == '1': self.estado = f'¡RESUELTO en {float(d["s"]):.2f} s! Verificado y guardado en ' + os.path.basename(self.salida())
            elif d.get('SIN_SOLUCION') == '1': self.estado = f'SIN SOLUCIÓN: se revisó TODO el árbol en {float(d["s"]):.0f} s (es una prueba).'
            else: self.estado = f'Detenido tras {float(d.get("s", 0)):.0f} s, sin solución todavía (revisado {self.info.get("pct", 0):.3f}%).'
            self.botones(); return
        if l.startswith('RESULTADO'):
            d = dict(x.split('=', 1) for x in l.split()[1:] if '=' in x)
            self.t_fin = time.time(); self.corre = False
            mj = d.get('mejor', '0/0').split('/'); self.info['mejor'] = int(mj[0]); self.info['ne'] = int(mj[1])
            self.info['pasos'] = int(d.get('pasos', 0)); self.info['mps'] = float(d.get('pasos_por_s', 0)) / 1e6
            if d.get('resuelto') == '1': self.estado = f'¡RESUELTO en {float(d["s"]):.2f} s! Verificado y guardado en ' + os.path.basename(self.salida())
            else: self.estado = f'Sin resolver tras {float(d.get("s", 0)):.0f} s. Se dibuja el mejor tablero; guardado en ' + os.path.basename(self.salida())
            self.botones()
        if l.startswith('Traceback') or 'Error' in l: self.estado = 'El motor tuvo un error: mira el registro de abajo.'

    def escribir(self):
        self.txt.config(state='normal'); self.txt.delete('1.0', 'end'); self.txt.insert('end', '\n'.join(self.historial))
        self.txt.see('end'); self.txt.config(state='disabled')

    def detener(self, silencioso=False):
        if self.motor_vivo():
            if silencioso: self.motor.kill()
            else:
                try: open(self.archivo_parar(), 'w').close(); self.estado = 'Deteniendo… (el motor manda el resultado)'
                except OSError: self.motor.kill()
        self.botones(); self.dibujar()

    def cerrar(self):
        if self.motor_vivo():
            try: self.motor.kill()
            except Exception: pass
        self.raiz.destroy()

    def botones(self):
        self.b_ini.state(['disabled'] if self.corre else ['!disabled'])
        self.b_det.state(['!disabled'] if self.motor_vivo() else ['disabled'])

    def mostrar_info(self):
        dt = ((self.t_fin or time.time()) - self.t0) if self.t0 else 0
        L = [self.estado, '']; I = self.info
        if I and 'nodos' in I:
            L.append(f'Nodos revisados: {I["nodos"]:,}   ({I.get("mps", 0):,.0f} millones por segundo)')
            if 'npref' in I:
                L.append(f'Trozos del árbol agotados: {I["hechos"]:,} de {I["npref"]:,}  ({I["pct"]:.3f}%)')
                L.append(f'Falta (estimado lineal, muy aproximado): {I["eta"]} h')
                L.append(f'Máximo colocado: {I["maxd"]} de {I["NN"]} casillas')
        elif I:
            ne = I.get('ne', 2 * self.n * (self.n - 1))
            L.append(f'Uniones que encajan: mejor {I.get("mejor", 0)} / {ne}' + (f'   (ahora {I["ahora"]})' if 'ahora' in I else ''))
            if ne == 480:
                cm = I.get('mejor', 0)
                L.append(f'   récord mundial: {RECORD_MUNDIAL} / 480  →  ' + ('¡¡SUPERADO!! Guarda el archivo _mejor_v6.txt' if cm > RECORD_MUNDIAL else ('igualado' if cm == RECORD_MUNDIAL else f'faltan {RECORD_MUNDIAL - cm}')))
            L.append(f'Pasos: {I.get("pasos", 0):,}   ({I.get("mps", 0):.0f} millones por segundo)')
        L.append(f'Tiempo: {dt:.1f} s')
        self.lbl_info.config(text='\n'.join(L))

    def dibujar(self):
        self.mostrar_info(); cv = self.cv; cv.delete('all')
        if not self.n: return
        n = self.n; W = max(100, min(cv.winfo_width(), cv.winfo_height()) - 8); L = W / n
        if not self.tab:
            for r in range(n):
                for c in range(n):
                    x0, y0 = c * L + 4, r * L + 4
                    cv.create_rectangle(x0, y0, x0 + L, y0 + L, outline='#ccc', fill='#dfe8ff' if r * n + c in self.fijas else '#f4f4f4')
            cv.create_text(W / 2 + 4, W / 2 + 4, text='Trabajando…' if self.corre else 'Pulsa ▶ Iniciar', font=('Segoe UI', 14), fill='#555')
            return
        T = self.tab
        for r in range(n):
            for c in range(n):
                p = T[r * n + c]; x0, y0 = c * L + 4, r * L + 4; x1, y1 = x0 + L, y0 + L; xm, ym = (x0 + x1) / 2, (y0 + y1) / 2
                tri = [(x0, y0, x1, y0), (x1, y0, x1, y1), (x1, y1, x0, y1), (x0, y1, x0, y0)]
                for d in range(4):
                    a1, b1, a2, b2 = tri[d]
                    cv.create_polygon(a1, b1, a2, b2, xm, ym, fill=PALETA[p[d] % len(PALETA)], outline='#ffffff')
                if r * n + c in self.fijas: cv.create_rectangle(x0 + 2, y0 + 2, x1 - 2, y1 - 2, outline='#1f4fd8', width=3)
        w = max(2, L / 8)
        for r in range(n):
            for c in range(n):
                p = T[r * n + c]; x0, y0 = c * L + 4, r * L + 4
                if c < n - 1 and p[1] != T[r * n + c + 1][3]: cv.create_line(x0 + L, y0 + L * 0.2, x0 + L, y0 + L * 0.8, fill='red', width=w)
                if r < n - 1 and p[2] != T[(r + 1) * n + c][0]: cv.create_line(x0 + L * 0.2, y0 + L, x0 + L * 0.8, y0 + L, fill='red', width=w)

def main():
    raiz = tk.Tk()
    try: raiz.geometry('1180x740')
    except Exception: pass
    app = App(raiz, sys.argv[1] if len(sys.argv) > 1 and os.path.exists(sys.argv[1]) else None)
    if os.environ.get('E2V6_AUTO'):          # solo para pruebas automáticas: ms[,ms_detener]
        x = os.environ['E2V6_AUTO'].split(',')
        raiz.after(300, app.iniciar)
        if len(x) > 1: raiz.after(int(x[1]), app.detener)
        raiz.after(int(x[0]), app.cerrar)
    raiz.mainloop()

if __name__ == '__main__':
    main()
