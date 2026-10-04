# Sistema de uniones (versión 5, prototipo): armar desde un tablero sintético

**Idea de Carlos, 1 de octubre de 2026.** Es otro sistema, aparte del método por marcos y firmas.

## Cómo empezar (lo más fácil)

**Doble clic en `Abrir_uniones.bat`.** Se abre el panel:

| Parte del panel | Qué hace |
|---|---|
| **Tablero** | Elige uno de la carpeta `ejemplos` en la lista, o pulsa **Abrir otro…** para cualquier `.txt`. |
| **Opciones** | Hilos, modo, límite de tiempo (0 = sin límite), semilla, piezas fijas (`fila,col,pieza,giros ; …`), probabilidad de "introducir pieza" y temperaturas de las réplicas. |
| **Cómo trabajar** | *Ver el tablero mientras trabaja*: se dibuja en vivo, con la barra de velocidad de cámara muy lenta a muy rápida. *Solo el resultado (máxima velocidad)*: el motor no dibuja nada; el panel solo muestra una línea de estado cada segundo y, al final, dibuja el resultado. |
| **Botones** | ▶ Iniciar · ⏸ Pausa (solo en vivo) · Un paso · ⏹ Detener. En modo rápido, Detener para el motor y dibuja el mejor tablero que alcanzó. |
| **Información** | Piezas reales, mejor de todos, pasos, velocidad, reales de cada hilo, tiempo y el registro de lo que va diciendo el motor. |

El panel **recuerda** el último tablero y las opciones (`ventana_uniones_config.json`). Si Windows bloquea `e2uniones.exe`, el panel usa solo `e2uniones_compatible.exe`. Si se cambian las opciones durante una pausa, "Iniciar" empieza de nuevo con las opciones nuevas.

Las soluciones se guardan junto al tablero como `..._solucion_uniones.txt`.

## La idea

1. Se parte de un tablero **ya armado**, inventado, que tiene **la misma cantidad de cada color** que el tablero que queremos resolver.
2. Se van **introduciendo las piezas reales** una por una, sin que el tablero deje de estar armado ni cambie la proporción de colores.
3. **Esquinas:** sus colores son los reales, pero pueden caer en cualquier esquina y moverse.
4. **Piezas conocidas** (pistas): son fijas, no se mueven ni se giran, y van primero.
5. **Sin piezas conocidas:** se fija una sola esquina, arriba a la izquierda.

## Cómo lo hace el programa

- **Representación:** el programa no coloca piezas: elige el **color de cada unión** entre dos casillas. Así el tablero **siempre** está armado, porque cada casilla es la pieza que forman sus 4 uniones. Además, los colores solo se intercambian entre uniones, así que la proporción nunca cambia.
- **Piezas reales e inventadas:** una casilla es **real** si su pieza existe en el juego, contando copias. Si no, es inventada.
- **Movimientos:**
  - *intercambio*: cambia los colores de dos uniones;
  - *introducir pieza*: elige una casilla inventada y una pieza real que falta, y le trae sus colores desde otras uniones, sobre todo de casillas inventadas.
- **Estrategia:** recocido simulado. A veces acepta empeorar para no atascarse; la "temperatura" baja con el tiempo y se recalienta si se estanca.
- **Meta:** todas las casillas reales. Eso **es** una solución exacta, y el programa la verifica al final.

## Motor en C (2 de octubre de 2026)

Es la misma lógica del prototipo, sin cambios en la idea, pero escrita en C: hace **unos 1,5 millones de pasos por segundo**, unas 75 veces más que Python.

| Modo | Comando |
|---|---|
| Con el panel (Python dibuja, C trabaja) | doble clic en `Abrir_uniones.bat` |
| Sin gráficos, a toda velocidad | `e2uniones.exe ejemplos\gen_6x6_c6_s1.txt --limite 120` |

**Sin gráficos:**
- El motor solo mira el reloj cada 4 096 pasos y escribe una línea cada 2 s, así que no pierde velocidad.
- Opciones: `--semilla S`, `-P F,C,K,G`, `--introducir 0.3`, `-o solucion.txt`.
- Si Windows bloquea `e2uniones.exe`, usa `e2uniones_compatible.exe`.

**Con ventana:**
- Cámara muy lenta y lenta: un cambio por cuadro.
- Normal: 2 000 pasos por cuadro.
- Rápida: 100 000 pasos por cuadro.
- Muy rápida: 1 000 000 de pasos por cuadro.
- La solución, si aparece, se guarda junto al tablero como `..._solucion_uniones.txt`.

### Primeras mediciones del motor en C (nube, 30 s por tablero)

| Tablero | Resultado | Python (60 s) |
|---|---|---|
| gen 5x5 s1, s2, s3 | **resueltos en 0,02 a 0,11 s** | 1 s a 12 s |
| gen 6x6 s1, s2, s3 | **3 de 3 resueltos (2 a 5 s)** | 1 de 3 |
| Harris 6x6 6:2 s01, s02, s03 | **1 de 3** (s03 en 3,9 s); los otros llegaron a 34 y 35 de 36 | 0 de 3 |
| Puzle de pista 1 (6x6) | resuelto en 0,01 s | 0,8 s |
| gen 7x7 s1 | llegó a 46 de 49 | 45 de 49 |

Las soluciones encontradas se comprobaron por separado: mismas piezas que el juego, todas las uniones coinciden y gris hacia afuera.

## Todos los núcleos (2 de octubre de 2026)

El motor ahora usa **todos los hilos del procesador**. Cada hilo tiene **su propio tablero** y trabaja por su cuenta; el primero que lo arma completo gana. Hay dos formas de repartir el trabajo:

| Modo | Cómo trabaja | Cuándo conviene |
|---|---|---|
| `--modo replicas` (por defecto con 4 hilos o más) | Cada hilo trabaja a una temperatura fija, de fría (0,15) a caliente (0,8). Cada tanto, dos hilos vecinos **se intercambian la temperatura** si conviene. Los calientes exploran, los fríos afinan lo bueno que les llega. | En general, el mejor. |
| `--modo independiente` | Cada hilo hace el recocido de siempre, con otra semilla. Es como lanzar 12 programas a la vez. | Para comparar. |

| Opción | Qué hace |
|---|---|
| `-t 12` | Número de hilos. Sin `-t`, usa todos. |
| `--tfria 0.15 --tcaliente 0.8` | La escalera de temperaturas del modo réplicas. |

Desde la línea de comandos (sin panel):

```
e2uniones.exe ejemplos\gen_7x7_c6_s1.txt --limite 300
e2uniones.exe ejemplos\gen_7x7_c6_s1.txt --modo independiente -t 6 --limite 300
```

**En el panel, viendo el tablero:**
- En las velocidades normal, rápida y muy rápida, **todos los hilos avanzan** y se dibuja el que va mejor en ese momento.
- En cámara lenta se sigue a ese hilo, cambio a cambio.
- A la derecha se ven las piezas reales de cada hilo.

**Sin gráficos,** cada 2 s escribe los pasos totales, la velocidad, el mejor hilo, las piezas reales de cada hilo y, en modo réplicas, cuántos intercambios de temperatura se aceptaron.

### Primeras mediciones con varios hilos (nube, 4 hilos en 2 núcleos, 20 a 40 s por tablero)

| Tablero | Réplicas 0,15 a 0,8 | Réplicas 0,1 a 0,4 | Independiente |
|---|---|---|---|
| gen 6x6 s1 | 0,25 s | 0,25 s | 1,06 s |
| gen 6x6 s2 | 2,4 s | 0,48 s | 0,73 s |
| gen 6x6 s3 | 0,10 s | 0,07 s | 0,10 s |
| Harris 6x6 s01 | 17,7 s | 17,7 s | 4,7 s |
| Harris 6x6 s02 | **10,5 s** | no (40 s) | no (40 s) |
| gen 7x7 s1, s2 | no (40 s) | no | no |

**Lectura:**
- Con 1 hilo, el Harris 6x6 s01 y el s02 no salían. Ahora salen los dos.
- La escalera de réplicas 0,15 a 0,8 fue la única que resolvió los 5 tableros 6x6, por eso es la de defecto.
- Una primera escalera de 0,05 a 2,0 no servía: los hilos estaban tan separados que nunca se intercambiaban.
- **El 7x7 sigue sin salir.** Más hilos ayudan, pero el atasco cerca del final sigue ahí. Los próximos pasos de abajo son los que atacan eso.
- En la nube solo hay 2 núcleos. En tu Ryzen, con 12 hilos, debería ir unas 6 veces más rápido que aquí.

## Mezcla con el método exacto: el cierre exacto (v5.3, 2 de octubre de 2026)

**Idea:** cuando el recocido deja el tablero casi armado, se rehace **exactamente** la zona problemática. Se toman las casillas inventadas más sus vecinas (radio 1 y luego 2). Las piezas reales de afuera quedan quietas y la zona se llena con una búsqueda por retroceso, como la del método por marcos. Esa búsqueda elige en cada paso la casilla con menos piezas posibles y tiene un límite de nodos.

**Cuándo se usa:**
- Se intenta cuando faltan **como mucho un cuarto** de las piezas.
- Nunca se repite sobre el mismo tablero.
- Nunca usa más tiempo que el recocido.

| Opción | Qué hace |
|---|---|
| `--cierre K` | Intenta el cierre cuando falten ≤ K piezas (0 = sin cierre). Por defecto, un cuarto del tablero. |
| `--cierre-nodos N` | Nodos por intento (por defecto 5 000 000). |
| `--cierre-radio R` | Radio máximo de la zona (por defecto 2). |

En el panel: **Opciones → Cierre exacto**.

### Resultados en tu PC (12 hilos, máximo 120 s por tablero)

| Tablero | Solo recocido | Con cierre (réplicas) | Con cierre (independiente) |
|---|---|---|---|
| Harris 6x6 s01, s02, s03 | 3,7 / 0,8 / 0,6 s | 1,7 / 2,3 / 0,6 s | 2,3 / 1,3 / 1,5 s |
| gen 7x7 s1 | 69 s | **0,47 s** | 0,37 s |
| gen 7x7 s2 | no (48/49) | **3,7 s** | 8,0 s |
| gen 7x7 s3 | no (48/49) | **2,4 s** | 2,5 s |
| gen 8x8 s1 | no (60/64) | **2,1 s** | 4,8 s |
| gen 8x8 s2 | no (60/64) | **4,4 s** | 10 s |
| gen 8x8 s3 | 42 s | **0,65 s** | 1,6 s |
| Harris 8x8 7:2 s01 | no (62/64) | no (62/64) | no (62/64) |
| Harris 8x8 7:2 s02 | 95 s | no (63/64) | **8,2 s** |
| 9x9 9:3 s1 (sin pistas, con 5 pistas) | no | no | no |

Referencia de la V4.6 en la nube (2 hilos): gen 8x8 s1–s3 entre 3,3 y 3,9 s; Harris 8x8 7:2 s01 y s02, 8,2 y 7,3 s. **En los 8x8 generados, uniones con cierre ya es tan rápido o más que la V4.6.** En los Harris 8x8 todavía no.

### Harris 8x8 7:2, los 10 tableros (tu PC, 12 hilos, máximo 120 s)

| Tablero | V4.6 | Uniones con réplicas | Uniones independiente |
|---|---|---|---|
| s01 | 4,1 s | no | no |
| s02 | 3,4 s | no | 8,3 s |
| s03 | 3,5 s | 61 s | no |
| s04 | 8,7 s | **7,1 s** | no |
| s05 | 4,6 s | no | no |
| s06 | 16,4 s | no | no |
| s07 | 12,6 s | no | no |
| s08 | 6,4 s | no | no |
| s09 | 4,0 s | 7,3 s | 42 s |
| s10 | 6,4 s | no | no |

- La V4.6 resuelve los 10.
- Uniones resuelve 4 de 10, y en el s04 le gana a la V4.6.
- En los Harris, el método exacto sigue siendo el fuerte.

### Hallazgo importante: las "casi-soluciones falsas"

En el 9x9 9:3, con la solución conocida, se midió el mejor tablero del recocido:
- tenía **74 de 81 piezas reales**;
- pero **solo 7 estaban en su sitio correcto**.

El recocido encuentra tableros casi completos que están **lejos** de la solución verdadera. Por eso el cierre no puede completarlos. En 7x7 y 8x8 esas casi-soluciones todavía están cerca de alguna solución; desde 9x9 ya no.

Prueba de control: **si se le da el marco correcto** como piezas fijas, el interior del 9x9 sale en **0,08 s**. La V4.6 también lo resuelve casi al instante con el marco correcto. Lo difícil, para los dos sistemas, es dar con el marco correcto.

### Lo que se probó y se descartó
- **Ventana exacta** (rehacer un bloque de 3x3 o 5x5 conservando los colores): casi nunca encaja (2 de 2 600 intentos).
- **Orden fijo en la búsqueda exacta:** mucho peor que elegir en cada paso la casilla más restringida.
- **Poda extra** (revisar las vecinas): no cambió nada.

## La carrera: los dos sistemas a la vez

```
python carrera.py ejemplos\gen_8x8_c8_s3.txt --hilos 12 --limite 600
```

- Lanza la V4.6 (de `..\solucionador_marcos_v4_6`) con la mitad de los hilos y uniones con la otra mitad.
- El primero que resuelve gana; la solución se verifica antes de darla por buena.
- En la nube, el gen 8x8 s3 lo ganó uniones en 0,7 s y el Harris 8x8 s02, marcos en 6,9 s.

## Ideas matemáticas probadas (2 de octubre de 2026)

| Idea | Qué es | Resultado |
|---|---|---|
| **Esqueleto** (física estadística) | 40 casi-soluciones del 9x9: ¿se repite alguna pieza siempre en el mismo sitio? | Señal débil. Solo cerca de la esquina fija (75 % y 65 % de repetición, correctas). En el resto, las casi-soluciones no coinciden entre sí: la pieza más repetida acierta en 6 de 80 casillas. |
| **Precios por tipo de pieza** (multiplicadores de Lagrange) | Suben los precios de las piezas que sobran y bajan los de las que faltan en cada atasco. | No ayuda. Con el mejor ajuste probado (precios 0,05), 74/81 reales, pero solo 4 en su sitio (antes, 7). Los Harris 8x8 siguen sin salir. |
| **Búsqueda guiada** (castigar lo que se repite en los atascos) | Penaliza las piezas que se repiten en los atascos, para que el recocido explore otras zonas. | No ayuda. 73/81 reales, solo 5 en su sitio. |
| **Conservación de colores en el exacto** | Paridad de colores, conteo y emparejamiento de Hall. | Ya se había probado en la V4.3/V4.4: recorta de 0 a 30 % de nodos, pero cada nodo cuesta más. |
| **Propagación de creencias** (`creencias.py`) | Cada casilla "conversa" con sus vecinas y se estima qué tan probable es cada pieza en cada sitio, equilibrado para que cada pieza se use una sola vez. | **Sí da señal:** con 9 piezas fijas, la pieza verdadera queda en promedio en el puesto 17 de 127 (al azar, ~64). Sin piezas fijas, puesto 40 de 126. Pero la más probable acierta poco (9 de 72). |
| **Creencias como orden de la búsqueda exacta** | La búsqueda exacta prueba primero las piezas más probables. | **Mezclado.** Mucho mejor en el Harris 6x6 s02 (10 veces menos nodos) y en el 7x7 s2 (resuelto, contra no resuelto en 100 M nodos). Peor en el 7x7 s1 y s3 (5 y 15 veces más nodos). Sirve como una estrategia más del portafolio, no como reemplazo. |

**Conclusión:**
- Las técnicas de búsqueda local (precios, búsqueda guiada) no rompen las casi-soluciones falsas.
- La única que aporta información nueva es la **propagación de creencias**, y aporta más cuantas más piezas fijas hay. Por eso encaja con las 5 pistas oficiales.
- Siguiente paso posible: añadirla como una estrategia más del portafolio de la V4.x, junto a la normal, la "menos restrictiva" y los reinicios.

Opciones nuevas del motor (experimentales): `--precios E`, `--gls A`, `--atasco P`, `--solo-exacto`, `--orden-creencias archivo`.

## Puntaje normal y Eternity II oficial (v5.4)

- **Puntaje normal (todas reales).** El panel y el motor muestran cuántas de las uniones encajarían en el formato del Eternity normal, con todas las piezas reales. Para eso se dejan las piezas reales y en las casillas inventadas se ponen las que faltan, cada una con el giro que mejor encaja. En 16x16 se compara con el **récord mundial: 470 de 480**.
- El motor escribe `convertido: X/480` en cada línea y `CONVERTIDO X/480` al final. Con `-o`, guarda además el tablero convertido como `..._convertido.txt`, que se puede comprobar o cargar en otro programa.
- **Eternity II oficial:** al abrir `ejemplos\eternity2_real.txt` (o la lista de piezas de Julia), el panel lo reconoce y pone solo las **5 piezas fijas** oficiales en "Piezas fijas".
- El cierre exacto automático ahora empieza cuando faltan como mucho 16 piezas (antes, un cuarto del tablero), para no frenar los tableros grandes.
- Primera prueba en la nube (2 hilos, 20 s, con las 5 pistas): 215 de 256 piezas reales, que equivalen a **397 de 480** uniones.

## Cómo usar el prototipo en Python

Necesita Python con tkinter, que viene con el Python normal de Windows.

```
cd E:\Hermes\data\sistema_uniones_v5
python e2uniones.py ejemplos\gen_5x5_c6_s1.txt
```

### La ventana

| Elemento | Qué hace |
|---|---|
| **Iniciar / Pausa / Un paso** | "Un paso" avanza hasta el siguiente cambio aceptado. |
| **Velocidad** | 1: cámara muy lenta (un cambio cada 0,4 s). 2: cámara lenta. 3: normal. 4: rápida. 5: muy rápida (50 000 pasos por cuadro). |
| **Probabilidad de "introducir pieza"** | Cuántas veces usa ese movimiento en vez del intercambio simple. |

Cómo leer el tablero:

| Lo que se ve | Qué significa |
|---|---|
| Borde negro grueso | Pieza real |
| Casilla tenue | Pieza inventada |
| Borde azul | Pieza fija (pista, o la esquina fijada) |
| Borde rojo | Lo que cambió en el último paso |

Sin archivo (`python e2uniones.py`), el programa abre una ventana para elegir el tablero.

### Modo rápido, sin gráficos

```
python e2uniones.py ejemplos\gen_6x6_c6_s3.txt --sin-graficos --limite 120
```

| Opción | Qué hace |
|---|---|
| `--semilla S` | Otro punto de partida al azar. |
| `-P F,C,K,G` | Pieza fija, igual que en e2marcos. |
| `--introducir 0.3` | Probabilidad del movimiento "introducir pieza". |
| `-o solucion.txt` | Guarda la solución si la encuentra. |

## Primeras mediciones (nube, Python, 60 s por tablero)

| Tablero | Resultado | Tiempo |
|---|---|---|
| gen 5x5 (s1 a s5) | **5 de 5 resueltos** | 1 s a 12 s |
| Puzles de pista oficiales 1 y 3 (6x6) | **2 de 2 resueltos** | 0,8 s cada uno |
| gen 6x6 (s1 a s3) | **1 de 3 resuelto** (s3, en 30 s) | los otros llegaron a 33 y 32 de 36 |
| Harris 6x6 6:2 (s01 a s03) | 0 de 3 | se quedaron en 34 de 36 |
| gen 7x7 s1 | 0 de 1 | se quedó en 45 de 49 |

**Lectura honesta:**

- **La idea funciona:** arma tableros completos y verificados partiendo de un tablero inventado.
- **El riesgo que veíamos se confirma:** en tableros de 6x6 y más grandes **se atasca cerca del final**, con 2 a 4 piezas inventadas que no logra quitar.
- **Velocidad:** es Python, unos 20 000 pasos por segundo. En C sería unas 50 a 100 veces más rápido.
- **Comparación:** nuestro motor exacto (V4.6) resuelve todos estos tableros en menos de 1 s. El prototipo sirve para estudiar y ver la idea, todavía no para competir.

## Próximos pasos posibles

1. ~~**Cierre exacto**~~ (hecho en la v5.3).
2. **Movimientos más inteligentes cerca del final:** cambiar una cadena de uniones en vez de dos, o reacomodar 2x2 casillas de una vez.
3. **Introducir las piezas en orden,** como propuso Carlos: primero las pistas, luego las esquinas y después las demás, congelando las que ya quedaron reales.
4. ~~Pasarlo a C~~ (hecho) y ~~usar todos los núcleos~~ (hecho).
