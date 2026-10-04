# Versión 4.7: las creencias en el método por marcos (2 de octubre de 2026)

## Qué es
- `creencias.py` hace **propagación de creencias** (física estadística): cada casilla "conversa" con sus vecinas por los colores de sus uniones, y se estima qué tan probable es cada pieza, con su giro, en cada casilla. Además se equilibra para que cada pieza se use una sola vez (balance de Sinkhorn).
- La V4.7 usa esas probabilidades para decidir **qué pieza probar primero**.

## Cómo se usa
```
python creencias.py TABLERO.txt -P ... --salida creencias\cr_X.txt      (mismas piezas fijas que la búsqueda)
e2marcos47.exe TABLERO.txt -t 12 -P ... --creencias creencias\cr_X.txt --portafolio 0 --valor 3   (solo creencias)
e2marcos47b.exe TABLERO.txt -t 12 -P ... --creencias creencias\cr_X.txt --portafolio 7            (creencias + reinicios guiados)
```

| Opción | Qué hace |
|---|---|
| `--valor 3` | Todos los hilos prueban primero la pieza más probable. |
| `--portafolio 6` | Cuatro equipos: normal, menos restrictiva, reinicios y creencias. |
| `--portafolio 7` | Dos equipos. Uno recorre todo en el orden de las creencias. El otro hace reinicios al azar, eligiendo con más frecuencia las piezas más probables. |

Sin piezas fijas, las creencias no sirven: el tablero es simétrico y todas las casillas dan lo mismo. Con las 5 pistas sí.

## Calidad de las creencias (con las 5 pistas)
Puesto medio de la pieza verdadera entre todas las opciones de su casilla:

| Tablero | Puesto medio | Opciones por casilla |
|---|---|---|
| 9x9 s1 a s5 | 17,6 / 8,7 / 10,9 / 14,8 / 18,3 | unas 120 (al azar, ~60) |
| 10x10 s1, s2 | 30,8 / 19,9 | unas 163 (al azar, ~82) |

## 9x9 9:3 con 5 pistas (PC de Carlos, 12 hilos)

| Tablero | Automático (= V4.6) | Portafolio 5 | Portafolio 6 | Solo creencias | Creencias + reinicios (p7) |
|---|---|---|---|---|---|
| s1 | 101,6 s | 81,7 s | 83,1 s | **3,3 s** | 22,7 s |
| s2 | 3,7 s | 4,5 s | 9,0 s | 3,5 s | 3,3 s |
| s3 | 3,1 s | 3,1 s | 3,1 s | 3,1 s | 3,1 s |
| s4 | 33,4 s | 60,0 s | 74,1 s | **17,1 s** | 35,0 s |
| s5 | 8,0 s | 10,1 s | 12,7 s | 5,9 s | 3,3 s |
| **Total** | **150 s** | 159 s | 182 s | **33 s** | 67 s |

Unos 3 s de cada tiempo son la preparación (estimado de Knuth).

**Lectura:**
- Las creencias **sirven más como estrategia principal** (todos los hilos) que como un equipo más del portafolio.
- Con 1 hilo, en el s2 la búsqueda necesitó 20 veces menos nodos (0,1 M contra 2 M).
- Hay mucho azar en estos tiempos. En la nube, con 2 hilos, el s1 con solo creencias no salió en 5 min, y con reinicios guiados salió en 56 s.

## 10x10 10:3 con 5 pistas
- Referencia sin creencias: s1 sin resolver en 15 min y s2 sin resolver en 45 min; estimados de días.
- Solo creencias en s1: sin resolver en 32 min (máximo 76 de 100 colocadas).
- Creencias + reinicios (p7), 1 h por tablero: en curso.

## Probado y descartado
Ajustar la amortiguación, el balance o el número de rondas de las creencias no cambia nada: converge siempre a lo mismo.
