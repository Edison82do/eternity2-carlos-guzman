# Resultados — método por marcos frente a Iterative Path MkIV | Human

29 de septiembre de 2026. Límite de 60 s por corrida, en la misma máquina. Cada tablero se probó con varias semillas; cada semilla da una mezcla distinta de las piezas.

Las columnas de tiempo dan la mediana y la media. Una corrida no resuelta cuenta como 60 s.

| Tablero | Marcos (Carlos): éxitos | Marcos: mediana / media | MkIV: éxitos | MkIV: mediana / media |
|---|---|---|---|---|
| 5×5, 6 colores (10 tableros) | 10/10 | 0.00 / 0.00 s | 10/10 | 0.05 / 0.05 s |
| 6×6, 6 colores (10 tableros) | 10/10 | 0.01 / 0.02 s | 10/10 | 0.15 / 0.14 s |
| 7×7, 6 colores (10 tableros) | **7/10** | **5.2 / 22.1 s** | 5/10 | 52.1 / 39.1 s |
| 8×8, 8 colores (5 tableros) | 0/5 | — | 0/5 | — |
| model_yannick_0706 (5 mezclas) | 0/5 | — | **4/5** | 20.8 / 27.9 s |

El detalle de cada corrida está en `resultados_comparacion.csv`: marcos, firmas y nodos del método de Carlos, y puntos e iteraciones de MkIV.

## Lectura

- **5×5 y 6×6:** los dos resuelven todo. El método por marcos es entre 5 y 10 veces más rápido, aun estando en Python contra Java.
- **7×7 (tableros generados):** el método por marcos resuelve más (7 frente a 5) y con una mediana mucho menor. Además usa muchos menos pasos: de 40 000 a 1 100 000 nodos, frente a 200 000 a 5 800 000 iteraciones de MkIV.
- **Dónde falla:** cuando hay demasiados marcos. Los casos que fallaron tenían entre unos 38 000 marcos (modelo 0706) y 2 700 000 (un 7×7), sumando todos los grupos. Entonces el filtro de firmas pierde fuerza y se vuelve lento, y en un caso generar los marcos ya costó casi un minuto. En el modelo 0706 gana MkIV. No es una regla fija: un 7×7 con 146 000 marcos sí se resolvió en 4 s.

## Siguiente mejora propuesta

**Firmas por lado.** En lugar de guardar marcos completos, se guardan las cadenas posibles de cada uno de los 4 lados, que son muchas menos. Al final se comprueba que los 4 lados elegidos no repitan piezas.

La idea es atacar justo el caso en que el número de marcos se dispara.
