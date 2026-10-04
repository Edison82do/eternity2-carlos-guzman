# Resultados de la versión 4.4 — 1 de octubre de 2026

**Condiciones:** PC de Carlos, 12 hilos y 300 s por tablero, salvo donde se indica. Los registros completos están en `registros_pc/`.

## Completitud

| Prueba | Resultado |
|---|---|
| 48 tableros, con 1 y con 8 hilos, comparados con la V4.2 | **96 de 96** con el mismo resultado y el mismo grupo (`cmp_grupos.txt`) |
| Lo mismo, forzando las tres estrategias (`--portafolio 5`) | 96 de 96 (`cmp_grupos_portafolio5.txt`) |
| Pieza central fija, tomada de la solución girada 90°, 180° y 270° | **144 de 144** resueltos. La V4.3 daba "sin solución" (falso) en 4 de 4 casos de prueba. |

## Los 16 Harris 8x8 difíciles (los lentos o sin resolver de la base)

En segundos; X = sin resolver en 300 s.

| Tablero | V4.3 | Reinicios | Pieza menos restrictiva | 3 estrategias, corrida 1 | **V4.4 por defecto** |
|---|---|---|---|---|---|
| 7:3 s08 | X | X | 74 | X | X |
| 7:4 s03 | X | X | X | 239 | **245** |
| 7:4 s06 | X | X | X | X | X |
| 7:4 s07 | X | 33 | 74 | 7 | **8** |
| 7:4 s10 | X | X | X | X | X |
| 8:2 s07 | X | X | X | 138 | **151** |
| 8:2 s09 | X | X | 271 | X | X |
| 7:3 s01 | 119 | 147 | X | X | 272 |
| 7:3 s05 | 130 | 97 | 22 | 11 | **13** |
| 7:3 s10 | 119 | X | 148 | 150 | 142 |
| 7:4 s02 | 148 | 152 | X | 164 | 150 |
| 7:4 s04 | 256 | 114 | X | X | X |
| 7:4 s09 | 177 | 39 | 235 | 105 | **101** |
| 8:2 s01 | 298 | X | X | X | X |
| 8:2 s10 | 271 | 156 | X | 290 | 297 |
| 7:2 s05 | 68 | 70 | 7 | 22 | **22** |
| **Resueltos** | **9** | 8 | 9 | 9 | **11** |

- **Tres tableros que la V4.3 nunca resolvió** salen ahora con la V4.4: 7:4 s03, 7:4 s07 y 8:2 s07.
- **Dentro del portafolio, el equipo de reinicios ganó la mayoría de las veces,** aunque tiene solo 4 hilos.
- **Las dos corridas con las tres estrategias dieron tiempos muy parecidos** (7:4 s03: 239 y 245 s; 7:4 s07: 7 y 8 s). Los reinicios usan semillas fijas, así que son reproducibles.

## Tableros medianos (V4.4 contra V4.3)

| Tablero | V4.3 | V4.4 | Nota |
|---|---|---|---|
| Harris 8x8 7:2 s01 / s02 / s03 | 3,2 / 4,7 / 1,8 s | 5,7 / 5,1 / 1,4 s | Las tres estrategias reparten los hilos, así que los fáciles van algo más lentos |
| 11x11 (1116) | 23,5 s | 27,9 s | Grupos chicos: una sola estrategia, como en la V4.3; la diferencia es ruido |

## Experimentos de la Fase B que no se adoptaron

Medidos con el estimado de Knuth del 9x9 9:3 s01, o resolviendo:

- **Paridad de colores, conteo de piezas, forzar piezas con un solo lugar, mapas hasta el punto fijo y emparejamiento global:** recortan de 0 a 30 %.
- **Ramificar por pieza:** peor.
- **Interior fila por fila (`--orden 8`):** 2,3 veces menos en el 9x9, pero no encuentra antes las soluciones de los Harris.
- **Cambio de representación a SAT (`experimento_sat.py`, CaDiCaL y Glucose):** de 10 a 100 veces más lento que nuestro motor.
