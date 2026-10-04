# Resultados de la versión 4.1 — 30 de septiembre de 2026

**Condiciones:** la nube, **solo 2 núcleos**, 30 s por tablero. Cada solución se verificó dos veces, en C y en Python.

Con 2 núcleos, el reparto antiguo de la V4 desperdiciaba poco, porque como mucho quedaba 1 hilo parado. **La mejora grande debería verse con 12 hilos en tu PC**; la medición de verdad es la tuya.

## Comprobación de que no se pierde nada

Un error en el reparto podría saltarse ramas sin avisar y dar por vacío un grupo que sí tiene solución. Por eso lo comprobé así:

- Resolví 48 tableros con 1 hilo y con 16 hilos: los 40 de siempre, salvo el 8x8 s5, más los 9 Harris 6x6 6:2.
- Con 16 hilos en solo 2 núcleos hay muchísimas cesiones de trabajo, así que la prueba fuerza al máximo el reparto.
- En los **48 casos** coinciden el resultado y el grupo donde apareció la solución.

Durante el desarrollo hubo un error de ese tipo: al ceder trabajo se corrompía el camino del hilo que cedía. **Esta comprobación lo detectó y está corregido.**

## 40 tableros de siempre (2 hilos, mediana; entre paréntesis, la media)

| Tableros | V4 | V4.1 |
|---|---|---|
| 5x5 (10) | 10/10 — 0,018 s | 10/10 — 0,018 s |
| 6x6 (10) | 10/10 — 0,019 s | 10/10 — 0,018 s |
| 7x7 (10) | 10/10 — 0,089 s (media 1,39 s) | 10/10 — 0,083 s (media 0,56 s) |
| 8x8, 8 colores (5) | 4/5 — 0,93 s | 4/5 — 0,67 s |
| modelo 0706 (5) | 5/5 — 0,29 s | 5/5 — 0,15 s |

## Harris (semillas 1 a 3, 2 hilos)

| Tipo | V4 | V4.1 |
|---|---|---|
| 6x6 6:2 | 3/3 — mediana 0,075 s | 3/3 — mediana 0,17 s (algo más lenta: el reparto cuesta más que el trabajo en tableros tan rápidos) |
| 7x7 6:4 | 3/3 — media 12,7 s | 3/3 — media 11,0 s |

## Reparto con 12 hilos

Hice una prueba corta con editor_1116, 12 hilos sobre 2 núcleos y 20 s de límite. La línea de progreso marcó **"hilos trabajando: 12 de 12" durante todo el grupo**. En la V4 el reparto era fijo y los hilos se iban quedando sin trabajo.

El tiempo real con 12 hilos solo se puede medir en tu PC.
