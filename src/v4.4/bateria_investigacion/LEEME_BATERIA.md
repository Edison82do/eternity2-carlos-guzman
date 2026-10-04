# Batería de investigación: tableros para compararse con la literatura

Esta batería es **para que la corras tú en tu PC**, con todos tus núcleos. Para lanzarla desde la carpeta `solucionador_marcos_v3`:

```
python correr_bateria.py                      (12 hilos, 300 s por tablero)
python correr_bateria.py --hilos 1 6 12       (compara 1, 6 y 12 hilos)
python correr_bateria.py --patron "harris_7x7*" --limite 60
```

- **Salida:** los resultados quedan en `resultados_bateria.csv`, con cada solución comprobada por el verificador de Python.
- **Cuánto tarda:** si ningún tablero se resuelve, lo máximo es el número de tableros × el límite × el número de configuraciones de hilos. Con 84 tableros, 300 s y 1 configuración, son unas 7 horas en el peor caso.
- **Recomendación:** empieza por grupos pequeños usando `--patron`.

## Qué es cada grupo de tableros

El nombre `n x n_I-B` indica el tamaño, **I** colores interiores y **B** colores de orilla (los que solo aparecen entre piezas del borde).

### 1. `harris_*`: las configuraciones de Harris, Vanstone y Gepp (2018)

Son las mismas configuraciones de su Tabla 2. Ellos midieron 100 tableros por configuración con su algoritmo ZLA, en un portátil i7 de 2014. Estas son sus medianas y el mejor de los otros métodos que compararon:

| Configuración | Mediana ZLA (Harris 2018) | Mejor de los demás métodos |
|---|---|---|
| 6×6, 6:2 | 0,026 s | 0,55 s (SAT) |
| 7×7, 6:4 | 0,017 s | 0,73 s |
| 8×8, 7:2 | 1,15 s | 125 s |
| 8×8, 7:3 | 12,1 s | 646 s |
| 8×8, 7:4 | 0,80 s | 682 s |
| 8×8, 8:2 | 88 s | 359 s |

Todas esas configuraciones están resueltas en la literatura. La comparación es en **tiempo**, no en si se resuelven o no.

**Ojo:** nuestros tableros son de las mismas configuraciones, pero **no son los mismos tableros exactos**, porque no encontré publicados los suyos. La comparación es orientativa.

### 2. `grande_*`: tableros de 9×9 a 12×12

No encontré tiempos publicados para estos tamaños con esta forma de generarlos. Sirven para medir hasta dónde llega el método.

### 3. `editor_*`: los modelos grandes del Eternity II Editor

| Modelo | Tamaño | Estado |
|---|---|---|
| 1015 | 10×10, 15 colores | tiene solución: el archivo del Editor es un tablero resuelto |
| 1218 | 12×12, 18 colores | tiene solución: el archivo es un tablero resuelto |
| 1116 | 11×11, 16 colores | el archivo es una corrida incompleta del Editor (197 de 220 bordes); no está comprobado que tenga solución |
| 1623 | 16×16, 23 colores (tamaño Eternity II) | el archivo no es un tablero resuelto; no sé si tiene solución |

### Referencia: el Eternity II real

16×16, 256 piezas, 22 colores: 5 de orilla y 17 interiores. **Nadie lo ha resuelto.** El récord es 470 de 480 bordes (Blackwood 2021, igualado por Bucas en 2024).

## Lo que ya se ve en una prueba rápida (servidor de 2 núcleos, 30 s)

- **harris_6x6 6:2 (s01):** resuelto en 6 s. Harris: 0,026 s de mediana.
- **harris_7x7 6:4 y harris_8x8 7:4 (s01):** **no se resolvieron en 30 s.** Harris: menos de 1 s.

**Conclusión honesta:** cuando hay **pocos colores de orilla** (2 a 4), hay millones de marcos posibles. El filtro del marco pierde fuerza y el método queda muy por detrás de ZLA. Donde los colores de la orilla son los mismos que los del interior (los tableros `gen_*` y los modelos del Editor), el método va muy bien.

Esa diferencia es justo lo que conviene medir bien con tus 12 hilos, y es la pista para la siguiente mejora.
