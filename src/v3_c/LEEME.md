# Solucionador por marcos — VERSIÓN 3.1 (motor rápido en C)

El mismo método de la versión 2 (grupo a grupo, con marcos, firmas y descarte de firmas), reescrito en C para ir a máxima velocidad. Las versiones 1 y 2 no se tocaron.

## Archivos

| Archivo | Qué es |
|---|---|
| `ventana_v3.py` | **Ventana para el motor**: eliges tablero, **número de núcleos**, límite y método; tiene Iniciar, Detener, cronómetro en vivo, registro y el tablero resuelto al final. No le quita velocidad al motor |
| `correr_bateria.py`, `bateria_investigacion\` | Batería para compararse con la literatura (ver `bateria_investigacion\LEEME_BATERIA.md`) |
| `e2marcos.exe` | **El programa listo para usar**, optimizado para tu Ryzen 5 3600 (AVX2, POPCNT). No necesita instalar nada |
| `e2marcos_compatible.exe` | El mismo programa para cualquier PC con Windows de 64 bits, por si lo usas en otra máquina |
| `e2marcos.c` | El código fuente, un solo archivo |
| `bench_v3.py` | Corre el programa sobre todos los tableros de `tableros\` y **verifica cada solución con el verificador de Python** |
| `tableros\` | Los mismos 40 tableros mezclados de las pruebas de V1, V2 y MkIV |
| `exportar_tableros.py` | Vuelve a crear esos tableros |
| `generar_tablero.py` | Crea un tablero nuevo de N×N |
| `ver_tablero.py` | Muestra un tablero o una solución en una ventana |
| `RESULTADOS_v3.md`, `resultados_v3_cloud.csv` | Resultados de las pruebas |

Los scripts `.py` usan módulos de la carpeta `..\solucionador_marcos_v2`, así que las dos carpetas deben seguir una al lado de la otra.

## Uso con ventana (lo más cómodo)

```
python ventana_v3.py
```

## Uso en consola (dentro de esta carpeta)

```
e2marcos.exe tableros\gen_7x7_c6_s2.txt
e2marcos.exe tableros\gen_8x8_c8_s2.txt -o solucion.txt
python ver_tablero.py solucion.txt
```

Opciones:

- `-t N`: número de hilos. Por defecto usa **todos** los del procesador (12 en tu Ryzen 5 3600).
- `-l S`: límite de tiempo en segundos.
- `-s K`: mezcla y gira las piezas con la semilla K. Sirve para los modelos del Editor, que vienen ya resueltos.
- `-o archivo.txt`: guarda la solución. La guarda solo si pasa la verificación.
- `-m N`: si un grupo tiene más de N marcos completos (por defecto 3 millones), pasa solo a **firmas por lado**.
- `--marcos` o `--lados`: fuerza uno de los dos métodos.
- `-q`: muestra solo la línea final de resultado.

Ejemplos con modelos del Editor y con tableros nuevos:

```
e2marcos.exe ..\solucionador_marcos_v2\modelos_editor\model_yannick_0706.txt -s 1
python generar_tablero.py 9 9 1 t9.txt
e2marcos.exe t9.txt -s 1 -l 120
```

Prueba completa con verificación independiente, comparando 1, 6 y 12 hilos:

```
python bench_v3.py --hilos 1 6 12 --limite 60
```

## Qué hace el motor por dentro

1. Lee el tablero y renumera los colores (1..K), para no reservar memoria para colores que no existen.
2. Fija una esquina y arma los grupos distintos. Los ordena del más barato al más caro según las cadenas posibles de cada lado.
3. **Por cada grupo, en orden:**
   - Genera todos los marcos (por tipo de pieza, sin repetidos) y sus firmas.
   - Quita las firmas ya descartadas en grupos anteriores.
   - Busca el interior **con todos los hilos repartiéndose el trabajo**.
4. Filtro de firmas en palabras de 64 bits. El motor **salta las zonas donde ya no queda ninguna firma viva**: esta mejora aceleró hasta 5 veces los casos difíciles.
5. Toda solución pasa por un verificador en C antes de aceptarse. `bench_v3.py` la verifica de nuevo con el verificador de Python.

## Novedades de la 3.1

- **Firmas por lado** dentro del motor en C. Se activan solas cuando un grupo tiene demasiados marcos, e incluyen la comprobación de que los 4 lados no repitan piezas.
- El motor informa su avance cada 2 s. Lo hace el hilo principal, que solo vigila, así que no frena la búsqueda.

## Límites conocidos

- Con **pocos colores de orilla** (2 a 4, como en los tableros de Harris 2018 o en el Eternity II real) hay millones de marcos posibles. El filtro del marco pierde fuerza y el método es mucho más lento que el mejor publicado (ver `bateria_investigacion\LEEME_BATERIA.md`).
- Cuando la orilla comparte colores con el interior (tableros `gen_*` y modelos del Editor), el método rinde muy bien.
