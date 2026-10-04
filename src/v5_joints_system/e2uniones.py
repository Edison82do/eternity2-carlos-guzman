"""
e2uniones.py — Prototipo de la IDEA DE CARLOS (versión 5, otro sistema):
"armar desde un tablero sintético e ir introduciendo las piezas reales".

Representación: en vez de colocar piezas, se eligen los COLORES DE LAS UNIONES entre casillas.
  - El tablero SIEMPRE está armado: cada casilla es la pieza que forman sus 4 uniones.
  - La proporción de colores es SIEMPRE la del tablero objetivo: los colores solo se intercambian
    entre uniones, nunca se crean ni se borran.
  - Una casilla es "real" si su pieza existe en el juego (contando copias). Las demás son "inventadas".
  - Meta: que las N×N casillas sean reales -> eso ES una solución exacta (se verifica al final).

Reglas de Carlos:
  - Las esquinas no tienen lugar fijo: cualquier esquina real puede caer en cualquier esquina.
  - Las piezas conocidas (pistas, -P) quedan fijas: no se mueven ni se giran; sus uniones no cambian.
  - Si no hay piezas conocidas, se fija UNA esquina (arriba a la izquierda).

Movimientos:
  - intercambio: se cambian los colores de dos uniones del mismo tipo (marco con marco, resto con resto);
  - introducir pieza: se toma una casilla inventada y una pieza real que falta, y se le ponen sus colores
    trayéndolos (por intercambio) de otras uniones, de preferencia de casillas inventadas.
  Se aceptan según recocido simulado (a veces se acepta empeorar para no quedar atascado).

Uso:
  python e2uniones.py TABLERO.txt                  ventana gráfica (cámara lenta / rápida)
  python e2uniones.py TABLERO.txt --sin-graficos   lo más rápido, solo texto
  opciones: --semilla S  --limite SEGUNDOS  -P F,C,K,G (pieza fija, como en e2marcos)  -o solucion.txt
"""
import sys, os, random, math, time, argparse

# ---------------------------------------------------------------------------------------------- lectura
def leer_tablero(ruta):
    v = []
    for l in open(ruta, encoding='utf-8', errors='replace'):
        if l.lstrip().startswith('#'): continue
        v += [int(x) for x in l.split()]
    n = int(round((len(v) // 4) ** 0.5))
    if n * n * 4 != len(v): raise SystemExit('El archivo no tiene un tablero cuadrado')
    return n, [tuple(v[4 * i:4 * i + 4]) for i in range(n * n)]

def rot(p, k):          # k giros horarios (igual que e2marcos)
    for _ in range(k % 4): p = (p[3], p[0], p[1], p[2])
    return p

def canon(p): return min(rot(p, k) for k in range(4))

# ---------------------------------------------------------------------------------------------- modelo
class Uniones:
    """Tablero como colores de uniones. Casilla (r,c); lados N E S O."""
    def __init__(self, n, piezas, pistas=(), semilla=1):
        self.n = n; self.piezas = piezas; self.rnd = random.Random(semilla)
        N = n
        # --- uniones: 0..nh-1 horizontales H[r][c] entre (r,c)-(r,c+1); luego verticales V[r][c] entre (r,c)-(r+1,c)
        self.ids = {}
        self.cel_de = []         # cada unión -> (casilla1, lado en casilla1, casilla2, lado en casilla2)
        for r in range(N):
            for c in range(N - 1):
                self.ids[('h', r, c)] = len(self.cel_de); self.cel_de.append(((r, c), 1, (r, c + 1), 3))
        for r in range(N - 1):
            for c in range(N):
                self.ids[('v', r, c)] = len(self.cel_de); self.cel_de.append(((r, c), 2, (r + 1, c), 0))
        self.ne = len(self.cel_de)
        # uniones de cada casilla (N E S O); -1 = da hacia afuera (gris)
        self.lados = {}
        for r in range(N):
            for c in range(N):
                self.lados[(r, c)] = (self.ids.get(('v', r - 1, c), -1), self.ids.get(('h', r, c), -1),
                                      self.ids.get(('v', r, c), -1), self.ids.get(('h', r, c - 1), -1))
        # clase de cada unión: 1 = marco (entre dos casillas de la orilla, a lo largo del borde), 0 = las demás
        borde = lambda rc: rc[0] in (0, N - 1) or rc[1] in (0, N - 1)
        self.clase = []
        for (a, _, b, _) in self.cel_de:
            mismo_borde = (a[0] == b[0] and a[0] in (0, N - 1)) or (a[1] == b[1] and a[1] in (0, N - 1))
            self.clase.append(1 if mismo_borde else 0)
        # --- objetivo: cuántas piezas de cada tipo hay
        self.objetivo = {}
        for p in piezas: t = canon(p); self.objetivo[t] = self.objetivo.get(t, 0) + 1
        # colores por clase: medias uniones del marco = colores laterales de esquinas y orillas
        medias = [{}, {}]
        for p in piezas:
            g = sum(1 for x in p if x == 0)
            for d in range(4):
                if p[d] == 0: continue
                # en una pieza de orilla, los lados vecinos al gris van al marco; el opuesto al gris, adentro
                if g >= 1 and (p[(d + 1) % 4] == 0 or p[(d + 3) % 4] == 0): k = 1
                else: k = 0
                medias[k][p[d]] = medias[k].get(p[d], 0) + 1
        self.bolsa = [[], []]
        for k in (0, 1):
            for col, m in medias[k].items():
                if m % 2: raise SystemExit(f'El color {col} aparece un número impar de veces: el tablero no puede armarse')
                self.bolsa[k] += [col] * (m // 2)
        cuantas = [sum(1 for x in self.clase if x == k) for k in (0, 1)]
        if [len(self.bolsa[0]), len(self.bolsa[1])] != cuantas:
            raise SystemExit(f'Los colores no alcanzan para las uniones: {len(self.bolsa[0])}/{cuantas[0]} y {len(self.bolsa[1])}/{cuantas[1]}')
        # --- uniones fijas (pistas, o una esquina si no hay pistas)
        self.fija = [False] * self.ne; self.col = [None] * self.ne
        self.celda_fija = set()
        fijadas = list(pistas)
        if not fijadas:   # regla de Carlos: sin pistas, se fija UNA esquina arriba a la izquierda
            for k, p in enumerate(piezas):
                if sum(1 for x in p if x == 0) == 2:
                    for g in range(4):
                        q = rot(p, g)
                        if q[0] == 0 and q[3] == 0: fijadas = [(0, 0, k, g)]; break
                    break
        for (r, c, k, g) in fijadas:
            q = rot(piezas[k], g); self.celda_fija.add((r, c))
            for d in range(4):
                e = self.lados[(r, c)][d]
                if e < 0:
                    if q[d] != 0: raise SystemExit(f'La pieza fija en ({r+1},{c+1}) tiene color hacia afuera')
                    continue
                if self.col[e] is not None and self.col[e] != q[d]: raise SystemExit('Dos piezas fijas no encajan entre sí')
                if self.col[e] is None:
                    self.col[e] = q[d]; self.fija[e] = True
                    self.bolsa[self.clase[e]].remove(q[d])
        # --- reparto inicial al azar (el tablero queda armado y con la proporción exacta)
        for k in (0, 1):
            libres = [e for e in range(self.ne) if self.clase[e] == k and self.col[e] is None]
            b = self.bolsa[k][:]; self.rnd.shuffle(b)
            for e, x in zip(libres, b): self.col[e] = x
        self.libres = [[e for e in range(self.ne) if self.clase[e] == k and not self.fija[e]] for k in (0, 1)]
        # --- conteo de tipos en el tablero
        self.tipo = {}; self.cuenta = {}
        for rc in self.lados: t = canon(self.pieza(rc)); self.tipo[rc] = t; self.cuenta[t] = self.cuenta.get(t, 0) + 1
        self.reales = sum(min(v, self.objetivo.get(t, 0)) for t, v in self.cuenta.items())
        self.mejor = self.reales; self.pasos = 0; self.aceptados = 0

    def pieza(self, rc):
        return tuple(0 if e < 0 else self.col[e] for e in self.lados[rc])

    def es_real(self, rc):
        t = self.tipo[rc]; return self.cuenta.get(t, 0) <= self.objetivo.get(t, 0)

    # cambio del puntaje al quitar / poner un tipo
    def _quitar(self, t):
        c = self.cuenta[t]; d = -1 if c <= self.objetivo.get(t, 0) else 0
        if c == 1: del self.cuenta[t]
        else: self.cuenta[t] = c - 1
        return d
    def _poner(self, t):
        c = self.cuenta.get(t, 0); d = 1 if c < self.objetivo.get(t, 0) else 0
        self.cuenta[t] = c + 1; return d

    def _aplicar(self, cambios):
        """cambios: lista de (unión, color nuevo). Devuelve (delta, deshacer, casillas tocadas)."""
        tocadas = set()
        for e, _ in cambios: a, _, b, _ = self.cel_de[e]; tocadas.add(a); tocadas.add(b)
        delta = 0
        for rc in tocadas: delta += self._quitar(self.tipo[rc])
        viejos = [(e, self.col[e]) for e, _ in cambios]
        for e, x in cambios: self.col[e] = x
        for rc in tocadas: t = canon(self.pieza(rc)); self.tipo[rc] = t; delta += self._poner(t)
        return delta, viejos, tocadas

    def _deshacer(self, viejos, tocadas):
        for rc in tocadas: self._quitar(self.tipo[rc])
        for e, x in reversed(viejos): self.col[e] = x
        for rc in tocadas: t = canon(self.pieza(rc)); self.tipo[rc] = t; self._poner(t)

    # ------------------------------------------------------------------ movimientos
    def mov_intercambio(self):
        k = 0 if self.rnd.random() < 0.85 or not self.libres[1] else 1
        L = self.libres[k]
        # se prefiere tocar casillas inventadas
        e1 = self.rnd.choice(L)
        for _ in range(6):
            a, _, b, _ = self.cel_de[e1]
            if not (self.es_real(a) and self.es_real(b)): break
            e1 = self.rnd.choice(L)
        e2 = self.rnd.choice(L)
        if self.col[e1] == self.col[e2]: return None
        return [(e1, self.col[e2]), (e2, self.col[e1])]

    def mov_introducir(self):
        """Carlos: introducir una pieza real en una casilla inventada, manteniendo armado y proporción."""
        inventadas = [rc for rc in self.lados if rc not in self.celda_fija and not self.es_real(rc)]
        if not inventadas: return None
        rc = self.rnd.choice(inventadas)
        faltan = [t for t, v in self.objetivo.items() if self.cuenta.get(t, 0) < v]
        if not faltan: return None
        lados = self.lados[rc]
        # piezas que faltan y que caben en la casilla (gris hacia afuera, uniones fijas respetadas)
        opciones = []
        for t in faltan:
            for g in range(4):
                q = rot(t, g); ok = True; iguales = 0
                for d in range(4):
                    e = lados[d]
                    if (e < 0) != (q[d] == 0): ok = False; break
                    if e >= 0:
                        if self.fija[e] and self.col[e] != q[d]: ok = False; break
                        if self.col[e] == q[d]: iguales += 1
                if ok: opciones.append((iguales, q))
        if not opciones: return None
        m = max(o[0] for o in opciones)
        q = self.rnd.choice([o[1] for o in opciones if o[0] >= m - 1])
        cambios = {}; usados = set(e for e in lados if e >= 0)
        for d in range(4):
            e = lados[d]
            if e < 0 or self.col[e] == q[d]: continue
            k = self.clase[e]
            # buscar otra unión de la misma clase con el color que hace falta
            cand = [f for f in self.libres[k] if f not in usados and self.col[f] == q[d]]
            if not cand: return None
            cand.sort(key=lambda f: (self.es_real(self.cel_de[f][0]) + self.es_real(self.cel_de[f][2]), self.rnd.random()))
            f = cand[0]; usados.add(f)
            cambios[e] = q[d]; cambios[f] = self.col[e]
        return list(cambios.items()) if cambios else None

    def paso(self, temp):
        self.pasos += 1
        mv = self.mov_introducir() if self.rnd.random() < self.p_introducir else self.mov_intercambio()
        if not mv: return 0, ()
        delta, viejos, tocadas = self._aplicar(mv)
        if delta >= 0 or self.rnd.random() < math.exp(delta / max(temp, 1e-9)):
            self.reales += delta; self.aceptados += 1
            if self.reales > self.mejor: self.mejor = self.reales
            return delta, tocadas
        self._deshacer(viejos, tocadas); return 0, ()

    p_introducir = 0.3

    # ------------------------------------------------------------------ verificación y salida
    def solucion_valida(self):
        """Todas las casillas son piezas reales del juego (contando copias): el tablero armado ES una solución."""
        if self.reales != self.n * self.n: return False
        cont = {}
        for rc in self.lados: t = canon(self.pieza(rc)); cont[t] = cont.get(t, 0) + 1
        return cont == self.objetivo

    def guardar(self, ruta):
        with open(ruta, 'w') as f:
            for r in range(self.n):
                f.write('   '.join(' '.join(map(str, self.pieza((r, c)))) for c in range(self.n)) + '\n')

# ---------------------------------------------------------------------------------------------- búsqueda
class Recocido:
    """Temperatura que baja y, si se estanca, vuelve a subir (recalentar)."""
    def __init__(self, t0=2.0, t_min=0.05, enfriar=0.99995, paciencia=200000):
        self.t = t0; self.t0 = t0; self.t_min = t_min; self.enfriar = enfriar; self.paciencia = paciencia
        self.sin_mejora = 0; self.ultimo_mejor = -1; self.recalentadas = 0
    def avanzar(self, u):
        if u.mejor > self.ultimo_mejor: self.ultimo_mejor = u.mejor; self.sin_mejora = 0
        else: self.sin_mejora += 1
        self.t = max(self.t_min, self.t * self.enfriar)
        if self.sin_mejora > self.paciencia:
            self.t = self.t0 * 0.6; self.sin_mejora = 0; self.recalentadas += 1

def correr_texto(u, limite, salida):
    rec = Recocido(); t0 = time.time(); ult = t0
    total = u.n * u.n
    print(f'Tablero {u.n}x{u.n}: {total} casillas, {u.ne} uniones. Reales al empezar: {u.reales}/{total}')
    while time.time() - t0 < limite:
        for _ in range(2000):
            u.paso(rec.t); rec.avanzar(u)
            if u.reales == total: break
        if u.reales == total: break
        if time.time() - ult > 2:
            ult = time.time()
            print(f'  {ult - t0:6.1f} s | pasos {u.pasos:,} | reales {u.reales}/{total} (mejor {u.mejor}) | temperatura {rec.t:.3f} | recalentadas {rec.recalentadas}', flush=True)
    dt = time.time() - t0
    ok = u.solucion_valida()
    print(f'RESULTADO resuelto={int(ok)} s={dt:.2f} pasos={u.pasos} mejor={u.mejor}/{total} pasos_por_s={u.pasos / max(dt, 1e-9):,.0f}')
    if ok and salida: u.guardar(salida); print('Solución guardada en', salida)
    return ok

# ---------------------------------------------------------------------------------------------- ventana
PALETA = ['#808080', '#e6194b', '#3cb44b', '#ffe119', '#4363d8', '#f58231', '#911eb4', '#46f0f0', '#f032e6',
          '#bcf60c', '#fabebe', '#008080', '#e6beff', '#9a6324', '#fffac8', '#800000', '#aaffc3', '#808000',
          '#ffd8b1', '#000075', '#a9a9a9', '#ff7f50', '#7fff00', '#dc143c', '#00ced1', '#9400d3']

def ventana(u, salida):
    import tkinter as tk
    from tkinter import ttk
    raiz = tk.Tk(); raiz.title('Uniones — idea de Carlos: armar desde un tablero sintético')
    n = u.n; L = max(24, min(70, 640 // n))
    cv = tk.Canvas(raiz, width=n * L + 2, height=n * L + 2, bg='white'); cv.grid(row=0, column=0, rowspan=12, padx=6, pady=6)
    est = {'corre': False, 'rec': Recocido(), 't0': None, 'ult': set()}
    lbl = ttk.Label(raiz, text='', font=('Consolas', 11), justify='left'); lbl.grid(row=0, column=1, sticky='w')
    ttk.Label(raiz, text='Velocidad').grid(row=1, column=1, sticky='w')
    vel = tk.IntVar(value=3)
    escalas = {1: ('cámara muy lenta (1 paso, 400 ms)', 1, 400), 2: ('cámara lenta (1 paso aceptado, 80 ms)', 1, 80),
               3: ('normal (200 pasos por cuadro)', 200, 1), 4: ('rápida (5 000 pasos por cuadro)', 5000, 1),
               5: ('muy rápida (50 000 pasos por cuadro)', 50000, 1)}
    lbl_vel = ttk.Label(raiz, text=escalas[3][0]); lbl_vel.grid(row=3, column=1, sticky='w')
    ttk.Scale(raiz, from_=1, to=5, orient='horizontal', variable=vel,
              command=lambda v: lbl_vel.config(text=escalas[int(float(v))][0])).grid(row=2, column=1, sticky='we')
    ttk.Label(raiz, text='Probabilidad de "introducir pieza"').grid(row=4, column=1, sticky='w')
    pin = tk.DoubleVar(value=u.p_introducir)
    ttk.Scale(raiz, from_=0.0, to=1.0, orient='horizontal', variable=pin).grid(row=5, column=1, sticky='we')
    leyenda = ('Casilla con borde negro grueso = pieza REAL del juego\n'
               'Casilla gris y tenue = pieza inventada\n'
               'Casilla con borde azul = pieza fija (pista o esquina)\n'
               'Borde rojo = lo que cambió en el último paso')
    ttk.Label(raiz, text=leyenda, foreground='#444').grid(row=8, column=1, sticky='w')

    def dibujar():
        cv.delete('all')
        for r in range(n):
            for c in range(n):
                x0, y0 = c * L + 2, r * L + 2; x1, y1 = x0 + L, y0 + L; xm, ym = (x0 + x1) / 2, (y0 + y1) / 2
                p = u.pieza((r, c)); real = u.es_real((r, c))
                tri = [(x0, y0, x1, y0), (x1, y0, x1, y1), (x1, y1, x0, y1), (x0, y1, x0, y0)]
                for d in range(4):
                    a, b, cc, dd = tri[d]
                    col = PALETA[p[d] % len(PALETA)]
                    cv.create_polygon(a, b, cc, dd, xm, ym, fill=col, outline='#ffffff' if real else '#dddddd',
                                      stipple='' if real else 'gray50')
                if (r, c) in u.celda_fija: cv.create_rectangle(x0 + 2, y0 + 2, x1 - 2, y1 - 2, outline='#1f4fd8', width=3)
                elif real: cv.create_rectangle(x0 + 1, y0 + 1, x1 - 1, y1 - 1, outline='black', width=2)
                if (r, c) in est['ult']: cv.create_rectangle(x0 + 4, y0 + 4, x1 - 4, y1 - 4, outline='red', width=2)
        dt = (time.time() - est['t0']) if est['t0'] else 0
        lbl.config(text=f'Piezas reales: {u.reales} / {n * n}   (mejor {u.mejor})\n'
                        f'Pasos: {u.pasos:,}   aceptados: {u.aceptados:,}\n'
                        f'Temperatura: {est["rec"].t:.3f}   recalentadas: {est["rec"].recalentadas}\n'
                        f'Tiempo: {dt:.1f} s')

    def ciclo():
        if not est['corre']: return
        u.p_introducir = pin.get()
        _, cada, ms = escalas[vel.get()]
        tocadas = set()
        if cada == 1:   # cámara lenta: avanzar hasta que se acepte un cambio
            for _ in range(500):
                d, t = u.paso(est['rec'].t); est['rec'].avanzar(u)
                if t: tocadas = set(t); break
        else:
            for _ in range(cada):
                d, t = u.paso(est['rec'].t); est['rec'].avanzar(u)
                if t: tocadas = set(t)
                if u.reales == n * n: break
        est['ult'] = tocadas
        dibujar()
        if u.reales == n * n:
            est['corre'] = False
            ok = u.solucion_valida()
            lbl.config(text=lbl.cget('text') + ('\n\n¡RESUELTO! Solución verificada.' if ok else '\n\n(no verificó)'))
            if ok and salida: u.guardar(salida)
            return
        raiz.after(ms, ciclo)

    def iniciar():
        if not est['corre']:
            est['corre'] = True
            if est['t0'] is None: est['t0'] = time.time()
            ciclo()
    def pausar(): est['corre'] = False
    def un_paso():
        est['corre'] = False
        for _ in range(500):
            d, t = u.paso(est['rec'].t); est['rec'].avanzar(u)
            if t: est['ult'] = set(t); break
        dibujar()
    f = ttk.Frame(raiz); f.grid(row=6, column=1, sticky='w', pady=6)
    ttk.Button(f, text='Iniciar', command=iniciar).pack(side='left')
    ttk.Button(f, text='Pausa', command=pausar).pack(side='left', padx=4)
    ttk.Button(f, text='Un paso', command=un_paso).pack(side='left')
    dibujar()
    if os.environ.get('E2U_AUTO'):          # solo para pruebas automáticas: arranca solo y cierra a los N ms
        raiz.after(300, iniciar); raiz.after(int(os.environ['E2U_AUTO']), raiz.destroy)
    raiz.mainloop()

# ---------------------------------------------------------------------------------------------- principal
def main():
    ap = argparse.ArgumentParser(description='Prototipo: armar desde un tablero sintético (idea de Carlos)')
    ap.add_argument('tablero', nargs='?'); ap.add_argument('--sin-graficos', action='store_true')
    ap.add_argument('--semilla', type=int, default=1); ap.add_argument('--limite', type=float, default=60)
    ap.add_argument('-P', action='append', default=[], help='pieza fija F,C,K,G (fila, columna, pieza, giros; desde 1)')
    ap.add_argument('--introducir', type=float, default=0.3, help='probabilidad del movimiento "introducir pieza"')
    ap.add_argument('-o', default=None, help='guardar la solución')
    a = ap.parse_args()
    if not a.tablero:     # sin archivo: se elige con una ventana
        import tkinter as tk
        from tkinter import filedialog
        r = tk.Tk(); r.withdraw()
        a.tablero = filedialog.askopenfilename(title='Elige un tablero', filetypes=[('Tableros', '*.txt'), ('Todos', '*.*')])
        r.destroy()
        if not a.tablero: return
    n, piezas = leer_tablero(a.tablero)
    pistas = []
    for s in a.P:
        f, c, k, g = (list(map(int, s.split(','))) + [0])[:4]
        pistas.append((f - 1, c - 1, k - 1, g))
    u = Uniones(n, piezas, pistas, a.semilla); u.p_introducir = a.introducir
    if a.sin_graficos: ok = correr_texto(u, a.limite, a.o); sys.exit(0 if ok else 3)
    ventana(u, a.o)

if __name__ == '__main__':
    main()
