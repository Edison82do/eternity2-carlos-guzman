# Versión 4.5: esquinas juntas

Es la versión 4.4 con un cambio: **las 4 esquinas son parte de la búsqueda**. Así el interior se busca una sola vez para todos los grupos, en vez de repetirse en cada grupo. Las versiones anteriores no se tocaron.

**Resultado:**
- El 11x11 pasa de unos 20 s a 0,6 s.
- De los 16 Harris 8x8 difíciles, resuelve 14 (la V4.4, 11).
- Los 9x9 9:3 con 5 piezas fijas salen entre 0,04 s y 88 s.

Las cifras completas están en `RESULTADOS_v45.md`.

## Cómo hacer tus pruebas

Todo desde la carpeta de la versión:

```
cd E:\Hermes\data\solucionador_marcos_v4_5
```

### Con la ventana

```
python ventana_v45.py
```

Elige el tablero y pulsa "Iniciar". Las opciones nuevas vienen en `auto`, que es lo recomendado:

| Opción | `auto` significa |
|---|---|
| Esquinas | Juntas desde 8x8 |
| Orden de casillas | Lo elige Knuth |
| Portafolio | 3 estrategias si el grupo es grande |

Las piezas fijas se escriben en el campo "Piezas fijas", separadas por `;`. Por ejemplo, `5,5,5,1;3,3,23,3`.

### Con la línea de comandos

| Para | Comando |
|---|---|
| Resolver un tablero | `e2marcos45.exe bateria_investigacion\harris_8x8_7-3_s08.txt -t 12` |
| Con límite de tiempo (s) | agrega `-l 600` |
| Guardar la solución | agrega `-o solucion.txt` |
| Solo ver el tamaño estimado, sin resolver | agrega `--solo-estimar` |
| Comparar con la forma anterior (por grupos) | agrega `--esquinas-por-grupo` |

### Con piezas fijas

Cada pieza fija es `-P fila,columna,pieza,giros`:

- **fila y columna** cuentan desde 1;
- **pieza** es el número de la pieza en el archivo del tablero, contando desde 1;
- **giros** son los giros horarios respecto a como está en el archivo.

En `tableros_conocidos\trabajos_pistas.txt` están listas las piezas fijas de los tableros con solución conocida:

- **p1** = solo la del centro;
- **p5** = la del centro y 4 cerca de las esquinas.

Por ejemplo, el 9x9 9:3 s4 con 5 piezas fijas:

```
e2marcos45.exe tableros_conocidos\c9x9_9-3_s4.txt -t 12 -P 5,5,41,0 -P 3,3,39,2 -P 3,7,3,1 -P 7,3,2,0 -P 7,7,60,0
```

Con la V4.5 sale en unos 36 s de búsqueda; con la V4.4 tardaba 194 s. La solución verdadera de cada tablero está en el `.sol` del mismo nombre, para comprobarla.

### Tableros nuevos con solución conocida

```
python tableros_conocidos\generar_conocido.py 9 9 3 7 mi_9x9.txt mi_9x9.sol
```

Los argumentos son: tamaño, colores interiores, colores de orilla, semilla, archivo del tablero y archivo de la solución.

## Opciones nuevas

| Opción | Qué hace |
|---|---|
| `--esquinas-juntas` | Esquinas como parte de la búsqueda. Por defecto desde 8x8. |
| `--esquinas-por-grupo` | Como hasta la V4.4: un grupo por cada manera de poner las esquinas. |

Las demás opciones son las de la V4.4. Las explica `e2marcos45.exe` sin argumentos.

## Archivos

| Archivo | Qué es |
|---|---|
| `e2marcos45.exe` | Motor para tu Ryzen (Zen 2). |
| `e2marcos45_compatible.exe` | Mismo motor, para cualquier PC de 64 bits. |
| `e2marcos45.c` | Código fuente. |
| `ventana_v45.py`, `bench_v45.py` | Ventana y prueba en lote. |
| `tableros_conocidos\` | Tableros 9x9, 10x10 y 12x12 con solución conocida, el generador y la lista de piezas fijas. |
| `experimento_sat.py` | El experimento SAT de la Fase B. |
| `RESULTADOS_v45.md` | Mediciones y evolución de todas las versiones. |
