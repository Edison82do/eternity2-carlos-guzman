# Versión 4.1: reparto dinámico del trabajo entre hilos

Es la versión 4 con un solo cambio: **cómo se reparte el trabajo entre los núcleos**. El método, el orden de búsqueda y las podas son los mismos. Las versiones 4 y anteriores no se tocaron.

## El problema que corrige

En la V4, las ramas del árbol se repartían **una sola vez**, al empezar cada grupo.

- Los hilos a los que les tocaban ramas cortas terminaban pronto y se quedaban parados.
- Al final de cada grupo trabajaban 1 o 2 hilos de 12.
- En tu prueba del 11x11 (editor_1116) la velocidad caía de unos 3,2 a unos 0,5 millones de nodos por segundo dentro de cada grupo.

## Cómo funciona ahora

1. Hay una **cola de tareas** compartida. Cada tarea es un camino de decisiones desde el inicio del grupo.
2. Al empezar, un hilo toma la tarea raíz y los demás esperan con "hambre".
3. Un hilo ocupado revisa cada 256 nodos si alguien tiene hambre. Si es así, **cede** las opciones que aún no ha probado en su nivel menos profundo, que son las ramas más grandes, y deja de probarlas él.
4. El hilo que recibe una tarea reconstruye el estado siguiendo el camino y sigue buscando desde ahí.
5. El grupo termina cuando la cola está vacía y ningún hilo trabaja.

Resultado: **todos los hilos trabajan hasta el final de cada grupo**. La línea de progreso ahora muestra "hilos trabajando: X de N" para que lo compruebes.

## Archivos

| Archivo | Qué es |
|---|---|
| `e2marcos41.exe` | Motor para tu Ryzen (Zen 2). |
| `e2marcos41_compatible.exe` | Mismo motor, para cualquier PC de 64 bits. |
| `e2marcos41.c` | Código fuente. |
| `ventana_v41.py` | Ventana, igual que la de la V4. |
| `bench_v41.py` | Prueba en lote con verificación en Python. |
| `tableros/`, `bateria_investigacion/`, `eternity2_real.txt` | Los mismos tableros que en la V4. |

Las opciones son las de la V4, más una:

| Opción | Qué hace |
|---|---|
| `--reparto-fijo` | Usa el reparto antiguo de la V4, para comparar en el mismo ejecutable. |

## La prueba que te interesa

Es la misma que hiciste con la V4, ahora con los dos repartos:

```
e2marcos41.exe bateria_investigacion\editor_1116_s01.txt -t 12
e2marcos41.exe bateria_investigacion\editor_1116_s01.txt -t 12 --reparto-fijo
```

Con la V4 tardó **206,8 s**; los grupos 1 y 2 se descartaron en 58 s y 54 s.
