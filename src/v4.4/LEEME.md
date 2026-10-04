# Versión 4.4: tres estrategias a la vez en los grupos grandes, y piezas fijas corregidas

Es la versión 4.3 con lo aprendido en la Fase B. El método por marcos y firmas no cambia, y las versiones anteriores no se tocaron.

## Qué trae

| Novedad | Qué hace |
|---|---|
| **Portafolio automático** | Si Knuth estima un grupo grande (más de 10⁹ nodos), los hilos se reparten en 3 equipos que buscan a la vez en el mismo grupo. Gana el primero que encuentra la solución o que termina el grupo. En los grupos chicos, una sola estrategia con todos los hilos, como antes. |
| Equipo 1: **normal** | Completo, con el orden elegido por Knuth. |
| Equipo 2: **pieza menos restrictiva** | Completo. En cada casilla prueba primero la pieza que deja más opciones a sus vecinas (`--valor 1`). |
| Equipo 3: **reinicios al azar** | Cada hilo busca solo, con las opciones en orden al azar y un tope de nodos que crece (serie de Luby). Al llegar al tope empieza de nuevo. |
| **Piezas fijas corregidas** (observación de Carlos) | Con una pieza fija ya no se puede girar el tablero, así que la esquina de arriba a la izquierda no se puede fijar: se prueban las 4 (hasta 24 grupos). |

### El error que corrige

En las versiones 4 a 4.3, una pieza fija (`-P`) tomada de una solución girada daba **"sin solución" aunque la había**: falló en 4 de 4 casos de prueba. Con la V4.4 salen 144 de 144: 48 tableros × 3 giros, con la pieza central fija.

## Por qué tres estrategias

Probamos 16 Harris 8x8 difíciles con cada estrategia, 12 hilos y 5 min:

- Cada una resolvió unos 9, **pero no los mismos**. Entre todas resolvieron 14 de 16.
- Es "cola pesada": cuál gana depende mucho del azar del tablero.
- Juntas, cada una con 4 hilos, sacaron dos tableros que ninguna había resuelto sola.

**El costo:** en un grupo grande sin solución, revisarlo entero tarda unas 3 veces más, porque los dos equipos completos tienen menos hilos. Por eso solo se activa en los grupos grandes.

## Opciones nuevas

| Opción | Qué hace |
|---|---|
| `--portafolio N` | Automático por defecto. `0` una sola estrategia; `3` completo + reinicios; `5` las tres estrategias. |
| `--umbral-portafolio X` | Nodos estimados desde los que se activa el portafolio automático (10⁹). |
| `--valor 1` | Pieza menos restrictiva primero, en toda la búsqueda. |
| `--reinicio-base N` | Primer tramo de los reinicios (20 000 nodos). |
| `--emparejar N`, `--umbral-orilla N`, `--orden 3`…`9` | Pruebas de la Fase B que no dieron mejora clara. Quedan para seguir investigando. |

En la ventana (`python ventana_v44.py`), "Portafolio" viene en `auto`.

## Archivos

| Archivo | Qué es |
|---|---|
| `e2marcos44.exe` | Motor para tu Ryzen (Zen 2). |
| `e2marcos44_compatible.exe` | Mismo motor, para cualquier PC de 64 bits. |
| `e2marcos44.c` | Código fuente. |
| `ventana_v44.py`, `bench_v44.py` | Ventana y prueba en lote. |
| `experimento_sat.py` | El experimento del cambio de representación a SAT: resultó de 10 a 100 veces más lento que nuestro motor. |
| `RESULTADOS_v44.md` | Mediciones. |
