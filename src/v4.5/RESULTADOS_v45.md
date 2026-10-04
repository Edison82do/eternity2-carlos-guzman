# Resultados de la versión 4.5 — 1 de octubre de 2026

**Condiciones:** PC de Carlos, 12 hilos. Los tiempos son de búsqueda (`t_interior`); desde la V4.3 hay unos 3 s más del estimado de Knuth en tableros de 8x8 en adelante. Los registros completos están en `registros_pc/`.

## Evolución de los tiempos, versión por versión

| Tablero | V4 | V4.1 | V4.2 | V4.3 | V4.4 | **V4.5** |
|---|---|---|---|---|---|---|
| 11x11 (editor_1116) | 206,8 s | 61,6 s | 20,3 s | 23,5 s | 27,9 s | **0,6 s** |
| Harris 8x8 7:2 s01 | — | 230 s | 49 s | 3,2 s | 5,7 s | **1,4 s** |
| Harris 8x8 7:2 s02 | — | — | 152 s (*) | 4,7 s | 5,1 s | — |
| Harris 8x8 7:3 s02 | — | — | 33 s (*) | 31 s | — | **1,1 s** |
| 16 Harris 8x8 difíciles resueltos en 5 min | — | — | — | 9 de 16 | 11 de 16 | **14 de 16** |
| 9x9 9:3 s01, sin piezas fijas (estimado de Knuth, nodos) | — | — | 4,3 × 10¹⁵ | ~9 × 10¹³ | ~9 × 10¹³ | **4,6 × 10¹³** |
| 9x9 9:3 con 5 piezas fijas (5 tableros) | — | — | — | (daba "sin solución" por error) | 8 s a 6 min | **1 s a 88 s** |

(*) Medido con la V4.3 en el orden de la V4.2 (`--orden 0`).

## Qué cambió en la V4.5: esquinas juntas

**Hasta la V4.4:**
- Se fijaban las 4 esquinas de una manera (un "grupo") y se buscaba todo el resto; luego la siguiente manera, y así.
- Son 6 grupos sin piezas fijas y hasta 24 con piezas fijas.
- Con "interior primero", cada grupo repetía casi la misma búsqueda del interior.

**En la V4.5:**
- Las 4 esquinas son casillas más de la búsqueda, y el programa decide cuál va en cada lugar como cualquier otra casilla.
- Los mapas de la orilla descartan las esquinas que ya no caben.
- El interior se busca **una sola vez** para todos los grupos.
- Sin piezas fijas, la primera esquina sigue fija arriba a la izquierda, para no buscar el mismo tablero girado.
- Es la opción por defecto desde 8x8. En tableros más chicos se sigue por grupos (`--esquinas-por-grupo`): ahí a veces era más lenta.

## 9x9 9:3 con 5 piezas fijas (centro + 4 cerca de las esquinas)

Tableros nuevos, con la solución conocida (`tableros_conocidos/`):

| Tablero | Sin piezas (estimado) | V4.4, por grupos | **V4.5, esquinas juntas** |
|---|---|---|---|
| c9x9 s1 | ~10¹⁶ nodos | 352 s | **88 s** |
| c9x9 s2 | ~5 × 10¹⁵ | 26 s | **1,1 s** |
| c9x9 s3 | ~4 × 10¹⁴ | 8 s | **0,04 s** |
| c9x9 s4 | ~1,5 × 10¹⁷ | 194 s | **36 s** |
| c9x9 s5 | — | 19 s | **5,4 s** |

**Sin resolver en 15 min, aun con 5 piezas fijas** (tiempo estimado para revisar todo el árbol):

| Tablero | Tiempo estimado |
|---|---|
| 10x10 10:3 s2 | unas 13 h |
| 10x10 10:3 s1 | unos 12 días |
| 12x12 14:4 s1 | unos 21 años |

## Los 16 Harris 8x8 difíciles, sin piezas fijas (en segundos; X = más de 300 s)

| Tablero | V4.3 | V4.4 | **V4.5** |
|---|---|---|---|
| 7:3 s08 | X | X | **173** |
| 7:4 s03 | X | 245 | **70** |
| 7:4 s06 | X | X | X |
| 7:4 s07 | X | 8 | **4** |
| 7:4 s10 | X | X | X |
| 8:2 s07 | X | 151 | 196 |
| 8:2 s09 | X | X | **77** |
| 7:3 s01 | 119 | 272 | **15** |
| 7:3 s05 | 130 | 13 | **7** |
| 7:3 s10 | 119 | 142 | **30** |
| 7:4 s02 | 148 | 150 | 222 |
| 7:4 s04 | 256 | X | **89** |
| 7:4 s09 | 177 | 101 | **91** |
| 8:2 s01 | 298 | X | **22** |
| 8:2 s10 | 271 | 297 | **11** |
| 7:2 s05 | 68 | 22 | **17** |
| **Resueltos** | 9 | 11 | **14** |

Los que faltan, 7:4 s06 y s10, se estiman en unos 19 días y unos 4 meses para revisar todo el árbol.

## Completitud

| Prueba | Resultado |
|---|---|
| 48 tableros, con 1 y con 8 hilos, comparados con la V4.2 | **96 de 96** con el mismo resultado; los de menos de 8x8, también en el mismo grupo |
| Forzando las esquinas juntas en los 48 tableros | 96 de 96 resueltos |
| Pieza central fija, tomada de la solución girada (por grupos) | 144 de 144 |
| Lo mismo, forzando las esquinas juntas | 142 de 144 en 60 s; los otros 2 salen, pero tardan más (65 s) |
