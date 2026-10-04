# Exploración: huella del diseñador, tableros "tipo oficial" y creencias (2 de octubre de 2026)

## 1. La lista oficial de piezas
- Está ordenada por tipo: 1–4 esquinas, 5–60 orillas, 61–256 interiores; dentro de cada grupo, por colores.
- La numeración no guarda relación con la solución.

## 2. Reglas que sí siguió el diseñador
Comparación del oficial contra 300 tableros al azar con los mismos parámetros:

| Rasgo | Oficial | Al azar |
|---|---|---|
| Piezas repetidas | 0 | 4,5 de media |
| Piezas simétricas | 0 | 0,7 |
| Uniones por color | cada color de orilla, exactamente 12; cada color interior, 24 o 25 | muy desiguales |

## 3. Generador "tipo oficial"
- `tableros_tipo_oficial\\generar_oficial.py N I B semilla tablero.txt solucion.txt`
- Cumple las tres reglas: colores equilibrados exactos, sin repetidas, sin simétricas.

Dificultad, estimado de Knuth de la V4.6 sin pistas (en exponentes de 10, media geométrica de 8 tableros):

| Tamaño | Al azar | Tipo oficial |
|---|---|---|
| 9x9 9:3 (nube) | 10^15,4 (rango 10^14,3 a 10^16,8) | 10^15,0 (rango 10^14,7 a 10^15,4) |
| 8x8 7:3 (PC) | 10^13,6 (rango 10^12,5 a 10^15,2) | 10^13,6 (rango 10^13,2 a 10^13,8) |

**Lectura:** los tableros tipo oficial no son más difíciles en promedio, pero son mucho más **parejos**: no hay tableros de suerte ni de mala suerte. Nuestras pruebas con tableros al azar eran representativas en promedio.

## 4. ¿Otra huella? No
Contra 200 tableros tipo oficial 16x16 (mismas reglas), el oficial cae en la zona normal en todo:

| Estadística | Percentil del oficial |
|---|---|
| Pares de colores vecinos dentro de una pieza (chi²) | 46 % |
| Piezas con un color repetido | 78 % |
| Piezas de solo dos colores | 46 % |
| Lados opuestos iguales | 61 % |
| Lados vecinos iguales | 72 % |
| Pares repetidos en las orillas | 29 % |

Fuera de las tres reglas, el diseño se comporta como azar. No aparece una debilidad como la del Eternity I.

## 5. Los puzles de pista
- Sus interiores usan solo 3 colores, sin equilibrar (en el puzle 3, un color es escaso).
- Están hechos con otro criterio, para ser fáciles. No muestran la huella del tablero grande.

## 6. Creencias con más piezas correctas (10x10 s2, 5 pistas)

| Piezas correctas fijas | Aciertos en casillas libres | Puesto medio de la verdadera |
|---|---|---|
| 5 pistas | 11 de 95 | 19,9 |
| + 10 al azar | 33 de 85 | 3,9 |
| + 20 al azar | **75 de 75 (todo)** | 1,0 |
| + 20 del marco | 41 de 75 | 3,5 |
| + 30 del marco | **65 de 65 (todo)** | 1,0 |
| + 40 por filas | 55 de 55 (todo) | 1,0 |

**¿Distinguen las creencias un marco parcial correcto de uno equivocado?**
- Con 20 piezas del marco, no.
- Con 30 sí, y claramente: el correcto encaja en el 100 % y con confianza 0,999; los equivocados, como mucho en el 86 % y con 0,63.
- Pero a ese nivel la búsqueda exacta ya descarta los marcos equivocados en milisegundos. Las creencias no atacan el cuello de botella, que es la enorme cantidad de marcos posibles.
