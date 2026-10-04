# Resultados de la versión 4.2 — 30 de septiembre de 2026

**Condiciones:** la nube, 2 núcleos (o 1, donde se indica).

## Completitud

| Prueba | Resultado |
|---|---|
| 48 tableros, cada uno con `--anillo2 1` y `--anillo2 2`, con 1 y con 16 hilos | En los 96 casos, el mismo resultado y el mismo grupo que sin el mapa (`cmp_grupos.txt`) |
| 40 tableros de siempre con `--anillo2 2`, verificados en Python | 39/40, igual que las versiones anteriores; el 8x8 s5 sigue sin salir en 30 s |

**El mapa no pierde soluciones.**

## Lo más claro: el 11x11 (editor_1116), 2 hilos

Los grupos 1 y 2 no tienen solución y hay que revisarlos enteros. Por eso su trabajo no depende de la suerte, y son la comparación más limpia.

| Modo | Grupo 1 (sin solución) | Grupo 2 (sin solución) | Total hasta la solución |
|---|---|---|---|
| `--anillo2 0` (como la V4.1) | 94 s, 94 M nodos | 53 s, 56 M nodos | 203 s, 205 M nodos |
| `--anillo2 1` (segundo anillo) | 42 s, 11 M nodos | 44 s, 11 M nodos | 121 s, 31 M nodos |
| `--anillo2 2` (todas las filas) | 26 s, 1,8 M nodos | 38 s, 2,4 M nodos | **98 s, 6,8 M nodos** |

- **Nodos:** el segundo anillo revisa unas 6,6 veces menos, y todas las filas unas 30 veces menos.
- **Tiempo:** baja 1,7 y 2,1 veces. Cada nodo cuesta más, porque hay más revisión en cada paso.

## Tableros chicos (1 hilo): aquí cuesta más de lo que ahorra

| Tablero | Nodos sin mapa → con segundo anillo | Tiempo sin mapa → con segundo anillo |
|---|---|---|
| 7x7 s2 | 21,4 M → 10,9 M | 11,8 s → 18,3 s |
| 8x8 s4 | 2,67 M → 1,13 M | 1,75 s → 2,19 s |
| Harris 6x6 6:2 (s01, s02, s03) | 0,97 / 0,25 / 0,50 M → 0,36 / 0,17 / 0,05 M | 0,49 / 0,10 / 0,18 s → 0,53 / 0,24 / 0,07 s |
| Harris 7x7 6:4 s01 | 48 M → 19 M | 25,5 s → 32,4 s |
| Harris 7x7 6:4 s02 | 49 M → 24 M | 23,2 s → 38,0 s |

**Lectura:**

- En tableros chicos el mapa corta los nodos a la mitad o menos, pero cada nodo cuesta unas 3 veces más, así que en total tarda algo más.
- En el 11x11 el recorte de nodos es tan grande que compensa de sobra.
- De ahí la opción automática: apagado hasta 8x8, todas las filas desde 9x9.

**Límite de esa regla:** se basa en un solo tablero grande. Tus pruebas con 12 hilos, sobre todo el Harris 8x8 7:2, dirán si conviene cambiarla.

## Tableros grandes de la batería (60 s, 2 hilos)

Los 9x9, 10x10 y 12x12 **no se resolvieron en 60 s con ningún modo**, así que no permiten comparar.

Con el mapa, el "máximo colocado" sale más bajo. Es lo esperable: el mapa corta las ramas malas antes de que lleguen lejos. Confirma que esa cifra no mide el avance.
