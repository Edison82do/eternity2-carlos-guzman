# Versión 4 — método por marcos y firmas (Carlos)

La versión 3 (`solucionador_marcos_v3`) **no se tocó**. Esta carpeta es independiente.

## Qué cambia respecto a la versión 3

1. **Cada lado es un mapa, no una lista de cadenas.**
   - La versión 3 generaba todas las cadenas de un lado y luego las filtraba.
   - Ahora cada lado es un mapa por capas: qué color puede haber en cada unión y qué piezas caben en cada casilla.
   - El mapa se poda **mientras se construye** (pasada de ida y de vuelta). Nunca se guarda la lista completa, así que no hay tope de 10 millones.
2. **Piezas repetidas y conteo, desde el principio.**
   - Cada vez que se coloca una pieza, se descuenta.
   - Si un tipo se agota, desaparece de todas las casillas al momento.
   - Si quedan menos casillas posibles para un tipo que piezas de ese tipo, la rama se corta enseguida.
3. **Orilla e interior en la misma búsqueda.**
   - Se elige siempre la casilla con menos opciones, sea de la orilla o del interior.
   - Lo que se decide en la orilla restringe al interior al instante, y al revés.
4. **Se conservan:** la esquina fija, los grupos (permutaciones de las otras 3 esquinas), el trabajo grupo a grupo con todos los núcleos, y las rutas anteriores como opción.
5. **Piezas fijas (pistas).** Sirven, por ejemplo, para la pieza 139 del Eternity II real.

## Archivos

| Archivo | Qué es |
|---|---|
| `e2marcos4.exe` | Motor para tu Ryzen (AMD Zen 2). |
| `e2marcos4_compatible.exe` | Mismo motor, para cualquier PC de 64 bits. |
| `e2marcos4.c` | Código fuente. |
| `ventana_v4.py` | Ventana: tablero, núcleos, límite, método, piezas fijas, cronómetro y dibujo de la solución. |
| `bench_v4.py` | Corre muchos tableros y verifica cada solución con el verificador de Python. |
| `tableros/` | Los 40 tableros de prueba. |
| `bateria_investigacion/` | Tableros tipo Harris y grandes, para **tus** pruebas. |
| `eternity2_real.txt` | Las 256 piezas del Eternity II real. |

La ventana y `bench_v4.py` usan el verificador de `..\solucionador_marcos_v2`, así que esa carpeta debe seguir al lado.

## Uso

```
python ventana_v4.py
```

En la línea de comandos:

```
e2marcos4.exe tableros\gen_8x8_c8_s4.txt -t 12
e2marcos4.exe eternity2_real.txt -P 8,9,139 -t 12 -l 600
python bench_v4.py --exe e2marcos4.exe --hilos 12 --limite 60 --carpeta bateria_investigacion --patron "harris_8x8_7-4*"
```

### Opciones del motor

| Opción | Qué hace |
|---|---|
| `-t N` | Núcleos (hilos). Por defecto usa todos. |
| `-l S` | Límite en segundos. |
| `-s K` | Mezcla las piezas con la semilla K (para modelos que vienen resueltos). |
| `-o ARCH` | Guarda la solución en ARCH. |
| `--unificado` | **Por defecto.** La ruta nueva de la versión 4. |
| `--auto` | Como la versión 3: marcos completos si caben, si no la ruta unificada. |
| `--marcos` | Solo marcos completos. |
| `--lados` | La ruta por lados de la versión 3. |
| `-P F,C,K[,G]` | Pieza fija: fila F y columna C (desde 1), pieza número K del archivo, G giros horarios opcionales. Sin G, se permite cualquier orientación. Se puede repetir. |
| `-q` | Muestra solo la línea final. |

Para el Eternity II, la casilla I8 es fila 8, columna 9 (I es la 9.ª letra): `-P 8,9,139`.

Mientras busca, el motor muestra **"máximo colocado: X de N piezas"**. Es la mayor cantidad de piezas que llegó a colocar, todas con sus bordes coincidiendo.
