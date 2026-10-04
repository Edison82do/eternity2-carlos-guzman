# Versión 6: la tarjeta de video explora y el procesador cierra

**Proyecto de Carlos Edison Guzman Marte.** Trabajo hecho con ayuda de herramientas de IA.

## La idea

- Siempre están **las piezas reales**. El programa las intercambia y gira para que encajen más uniones, que es como se mide el récord (470 de 480 en el Eternity II).
- **La tarjeta de video** (GTX 1070) trabaja con miles de tableros a la vez, cada uno a una temperatura distinta (réplicas), con unos 750 millones de pasos por segundo.
- **El procesador** hace el **cierre exacto**. Cuando un tablero tiene pocas uniones malas, se rehace exactamente la zona alrededor de ellas, reacomodando sus piezas con una búsqueda por retroceso, como el método por marcos. Si lo logra, el tablero queda resuelto.
- Es la versión "inversa" del sistema de uniones. Allí el tablero siempre está armado y se persiguen piezas reales; aquí siempre hay piezas reales y se persiguen uniones.

## Cómo usarlo

**Doble clic en `Abrir_v6.bat`.**

| Parte del panel | Qué hace |
|---|---|
| **Tablero** | Lista de tableros, o **Abrir otro…**. Con el Eternity II oficial pone solo las 5 pistas. Con `eternity2_record_470_Blackwood.txt` empieza desde el récord, con solo el centro fijo. |
| **Tableros a la vez** | 30 720 llena la tarjeta. |
| **Temperaturas / Escalones** | La escalera de réplicas (por defecto, de 0,15 a 1,0 en 32 escalones). |
| **Cierre exacto** | Activo por defecto. Lo intenta cuando faltan pocas uniones (automático: una séptima parte de las uniones). |
| **Cómo trabajar** | Ver el mejor tablero cada segundo, con las uniones malas marcadas en rojo, o solo el resultado. |
| **Información** | Uniones que encajan, comparación con el récord (en 16x16), velocidad y registro del motor. |

- El mejor tablero se guarda junto al tablero como `..._mejor_v6.txt`. Si está completo, es la solución.
- Al final se verifica por separado: mismas piezas, gris hacia afuera, piezas fijas en su lugar y uniones que encajan.

Desde la línea de comandos:

```
python gpu_recocido.py TABLERO.txt --tableros 30720 --limite 600 -P 9,8,139,2 -o mejor.txt
```

## Búsqueda exacta en la tarjeta (nuevo)

El recocido adivina mejorando el puntaje; no tiene garantía. La **búsqueda exacta** es lo contrario: recorre
**todo** el árbol de posibilidades con poda por colores, como el método por marcos, pero repartido en miles de hilos.

- Las casillas se llenan en un orden fijo (por filas). Cada casilla ya tiene puesta la de arriba y la de la
  izquierda, así que sus candidatos salen directo de una lista por (color de arriba, color de la izquierda).
- El procesador corta el árbol en cientos de miles de **prefijos** (tableros a medio armar, de 6 a 14 casillas).
  Cada hilo de la tarjeta toma un prefijo, lo agota por completo y toma el siguiente.
- Si hay solución, la encuentra. Si se agotan **todos** los prefijos sin solución, eso es una **prueba** de que no
  hay solución con esas piezas fijas.
- Velocidad en la GTX 1070: unos **1 400 millones de nodos por segundo** (núcleo 3). El mismo código en el
  procesador, con 12 hilos, hace unos 290 millones.

En el panel: **Método → Búsqueda exacta**. Se guarda como `..._solucion_v6.txt`.

Desde la línea de comandos:

```
python gpu_exacto.py TABLERO.txt [-P F,C,K,G ...] [--limite S] [--orden filas|marco|espiral|voraz] [--hilos-gpu 16384] [--nucleo 3] [-o solucion.txt]
```

Resultados (tableros medianos, GTX 1070):

| Tableros | Búsqueda exacta (tarjeta) | Comparación |
|---|---|---|
| gen 8x8 s1, s2 | 7,1 s y 1,1 s (con la compilación) | — |
| Harris 8x8 (7-2) s01 a s10 | **10 de 10**: 2,0 / 0,8 / 1,7 / 4,8 / 29,2 / 24,4 / 1,2 / 0,4 / 1,2 / 0,4 s | V4.6: 10 de 10 en 3 a 16 s cada uno; el recocido v6: 0 de 3 |
| 9x9 con 5 pistas s1 a s5 | 61,6 / 8,0 / 9,4 / 157 / 6,0 s (total ~240 s) | V4.6: 150 s en total; V4.7 con creencias: 33 s |

**Lectura:** la tarjeta revisa unas 5 veces más nodos por segundo que el procesador con el mismo método, pero el
método por marcos (V4.6) poda muchísimo mejor: en el 9x9 s1 necesita del orden de cientos de millones de nodos, y la búsqueda
simple por filas, 86 000 millones. La tarjeta gana en los Harris y empata o pierde en los 9x9 con pistas. El
siguiente paso natural sería llevar la poda de la V4.6 (interior primero, la casilla con menos opciones) a la tarjeta.

### Prueba: la poda de la V4.6 en la tarjeta (`gpu_dominios.py`, `dominios.cu`)

Cada casilla con su lista de opciones (dominios), siempre la casilla con menos opciones, interior primero, quitar la
pieza puesta de las demás casillas, recortar vecinas y mapas de todas las filas y columnas (perezosos), como la V4.6.
Funciona y resuelve, pero la tarjeta hace solo unos 0,1 millones de nodos por segundo con este método (cada nodo es
mucho trabajo distinto por hilo), mientras que el mismo código en el procesador con 12 hilos hace 1 millón:

| Tablero | Tarjeta (dominios) | Procesador 12 hilos (dominios) | V4.6 |
|---|---|---|---|
| 9x9 s2 con 5 pistas | 30 s | 8,3 s | 3,7 s |
| 9x9 s5 con 5 pistas | 20 s | — | 8,0 s |
| Harris 8x8 s01 a s05 | 5 de 5 (6,6 a 118 s) | — | 5 de 5 |

**Conclusión:** la tarjeta sirve para búsquedas sencillas y masivas (la búsqueda por filas: 1 400 a 1 900 millones de
nodos por segundo), no para la poda pesada de la V4.6, que sigue siendo mejor en el procesador.

## Archivos

| Archivo | Qué es |
|---|---|
| `ventana_gpu.py`, `Abrir_v6.bat` | El panel. |
| `gpu_recocido.py` | El motor: tarjeta + procesador, réplicas, cierre y verificación. |
| `nucleo2.cu` | El código que corre en la tarjeta. El mismo código, compilado como `nucleo2_cpu.dll`, hace el cierre en el procesador. |
| `gpu_exacto.py`, `exacto.cu`, `exacto_c.dll` | La búsqueda exacta: el motor, el código de la tarjeta y la versión para el procesador (prefijos). |
| `gpu_dominios.py`, `dominios.cu`, `dominios_b.dll` | Prueba de la poda de la V4.6 en la tarjeta (ver arriba). |
| `gpu_creencias.py` | Las creencias, vectorizadas (sirven en el procesador o en la tarjeta). |
| `gpu_info.py`, `gpu_prueba_torch.py`, `gpu_instalar_cupy.py` | Diagnóstico e instalación de CuPy. |
