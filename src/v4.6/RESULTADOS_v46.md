# Resultados de la versión 4.6 — 1 de octubre de 2026

**Condiciones:** PC de Carlos, 12 hilos, tiempo de búsqueda. Los registros están en `registros_pc/`.

## Completitud

| Prueba | Resultado |
|---|---|
| 48 tableros, con 1 y con 8 hilos | **96 de 96** con el mismo resultado que la V4.2; también forzando las esquinas juntas |
| Pieza central fija, tomada de la solución girada | **144 de 144** |

## Velocidad

En el 9x9 9:3 s4 con 5 pistas, con 1 hilo durante 40 s:

| Versión | Nodos revisados | Estimado de Knuth (9x9 s1, 5 pistas) | Estimado de Knuth (10x10 s2, 5 pistas) |
|---|---|---|---|
| V4.5 (`--perezoso 0`) | 2,98 M | 1,22 × 10⁸ | 2,35 × 10¹⁰ |
| **V4.6** | **6,08 M** | 1,26 × 10⁸ | 2,41 × 10¹⁰ |

El doble de velocidad con el mismo árbol.

## V4.5 contra V4.6, en la PC

En segundos; X = sin resolver en 300 s.

| Tablero | V4.5 | V4.6 |
|---|---|---|
| 11x11 (1116) | 0,57 | **0,34** |
| Harris 8x8 7:2 s01 | 1,41 | **1,18** |
| 9x9 9:3 con 5 pistas: s1 | 88 | 95 |
| s2 | 1,1 | **0,77** |
| s3 | 0,04 | 0,03 |
| s4 | 36 | **30** |
| s5 | 5,4 | **2,5** |
| Harris 8x8 7:3 s08 | 173 | **86** |
| 7:4 s03 | 70 | **59** |
| 7:4 s07 | 4 | 11 |
| 8:2 s07 | 196 | **14** |
| 8:2 s09 | 77 | **54** |
| 7:3 s01 | 15 | **11** |
| 7:3 s05 | 7 | 8 |
| 7:3 s10 | 30 | **23** |
| 7:4 s02 | 222 | **194** |
| 7:4 s04 | 89 | **80** |
| 7:4 s09 | 91 | X |
| 8:2 s01 | 22 | 30 |
| 8:2 s10 | 11 | **4** |
| 7:2 s05 | 17 | **1,6** |
| **16 Harris difíciles resueltos** | 14 | 13 |

**Lectura:**

- Casi todo va igual o más rápido.
- En los Harris difíciles, cuál de las tres estrategias encuentra la solución primero cambia de una corrida a otra (cola pesada). Por eso el 7:4 s09 salió en una corrida y en la otra no.
- Sumando los 13 tableros que salieron con las dos versiones, la V4.6 tardó unos 576 s y la V4.5 unos 932 s.

## Prueba larga, detenida a mano

**Tablero:** 10x10 10:3 s2 con 5 pistas.

- Carlos la detuvo a los **45 minutos** para usar la PC.
- Llevaba 2 830 millones de nodos y un máximo de 73 de 100 piezas colocadas, sin solución todavía.
- **Estimado de Knuth** para revisar todo el árbol: unas 8 h.
- Queda pendiente repetirla con más tiempo.
