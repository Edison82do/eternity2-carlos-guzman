# Versión 4.6: mapas de filas "perezosos"

Es la versión 4.5 con un cambio en la velocidad, no en el método. Las versiones anteriores no se tocaron.

## Qué cambió

Con piezas todas distintas, cada pieza que se pone se quita de **todas** las casillas libres. La V4.5 tomaba eso como un cambio en todas las filas y columnas, y recalculaba sus mapas en cada paso.

La V4.6 solo recalcula una fila o columna cuando alguna de sus casillas cambió **por otra razón**: una vecina recién puesta o un mapa de la orilla.

- **No se pierde nada:** saltarse un filtro nunca descarta una solución. A lo sumo deja alguna opción que ya no sirve, y esa cae un poco después.
- **Resultado:** el doble de nodos por segundo (6,1 M contra 3,0 M en 40 s con 1 hilo), con el mismo árbol, porque el estimado de Knuth no cambia.

Además se corrigieron dos detalles de la línea de progreso:

- el equipo de reinicios ya no muestra un "% revisado" (no tenía sentido); muestra cuántos reinicios lleva;
- "hilos trabajando" ya cuenta también los hilos de reinicios.

## Opción nueva

| Opción | Qué hace |
|---|---|
| `--perezoso 0` | Recalcula los mapas como la V4.5, para comparar. Por defecto es `1`. |

Todo lo demás es igual que en la V4.5. Las instrucciones para hacer pruebas, incluidas las piezas fijas, están en el `LEEME.md` de la V4.5. Los tableros con solución conocida y las pistas oficiales siguen en `solucionador_marcos_v4_5\tableros_conocidos` y `solucionador_marcos_v4_5\pistas_oficiales`.

## Archivos

| Archivo | Qué es |
|---|---|
| `e2marcos46.exe` | Motor para tu Ryzen (Zen 2). |
| `e2marcos46_compatible.exe` | Mismo motor, para cualquier PC de 64 bits. |
| `e2marcos46_respaldo.exe` | El mismo motor que se probó en tu PC. Solo cambian los dos detalles de la línea de progreso. Úsalo si Windows bloquea el primero. |
| `e2marcos46.c`, `ventana_v46.py`, `bench_v46.py` | Código fuente, ventana y prueba en lote. |
| `RESULTADOS_v46.md` | Mediciones. |
