# Resultados de la versión 4.3 — 30 de septiembre de 2026

**Condiciones:** PC de Carlos, 12 hilos, salvo donde se indica. Los registros completos están en `registros_pc/`.

## Completitud

| Prueba | Resultado |
|---|---|
| 48 tableros, con 1 y con 8 hilos, comparados con la V4.2 (`--anillo2 2`) | **96 de 96** con el mismo resultado y el mismo grupo |
| Lo mismo, con el portafolio de 2 equipos y de 4 equipos | 96 de 96 en los dos casos |
| % revisado al final de cada grupo sin solución | Exactamente 100 % con 1, 2, 4 y 8 hilos |

## Tiempos de búsqueda en la PC, 12 hilos

El estimado de Knuth suma unos 3 s más en tableros de 8x8 en adelante.

| Tablero | Orden de la V4.2 (menos opciones) | V4.3 (orden elegido por Knuth) | Mejora |
|---|---|---|---|
| Harris 8x8 7:2 s01 | 49 s, 65 M nodos (V4.2) | **3,2 s, 4,3 M nodos** (interior primero) | 15× |
| Harris 8x8 7:2 s02 | 152 s, 204 M nodos | **4,7 s, 5,6 M nodos** (interior primero) | 32× |
| Harris 8x8 7:2 s03 | 28 s, 36 M nodos | **1,8 s, 2,2 M nodos** (interior primero) | 16× |
| Harris 8x8 7:3 s01 | 114 s | 128 s (eligió el orden de siempre) | igual (ruido) |
| Harris 8x8 7:3 s02 | 33 s | 31 s (eligió el orden de siempre) | igual |
| 11x11 (editor_1116) | 20,3 s (V4.2) | 23,5 s (eligió el orden de siempre) | igual (ruido) |

## Estimados de Knuth del 9x9 9:3 s01 (el tablero de las 7,5 horas)

| Variante | Nodos estimados de los 3 grupos |
|---|---|
| V4.2 sin el mapa de filas (`--anillo2 0`) | 9,9 × 10¹⁶ |
| Solo el segundo anillo (`--anillo2 1`) | 2,6 × 10¹⁶ |
| V4.2, todas las filas (`--anillo2 2`) | 4,3 × 10¹⁵ |
| + paridad de colores | 4,4 × 10¹⁵ (no ayuda) |
| + conteo de piezas interiores | 4,3 × 10¹⁵ (no ayuda) |
| **+ interior primero (V4.3)** | **≈ 9 × 10¹³ (unas 50 veces menos)** |

**Lectura:** aun así, el grupo 1 solo tendría unos 3 × 10¹³ nodos. A 0,8 M nodos/s son más de 400 días. La corrida de 7,5 h se quedó en el grupo 1 porque había revisado una fracción ínfima.

## Qué se aprendió

1. **El estimado de Knuth es muy preciso para grupos sin solución.** En el 11x11 estimó 2 M y 2,7 M nodos, y fueron unos 1,9 M y 2,5 M. Sirve para medir una idea en minutos con `--solo-estimar`, sin resolver el tablero.
2. **El orden "interior primero" es enorme en tableros con pocos colores de orilla** (Harris 7:2 y el 9x9 9:3). Allí la orilla casi no restringe, aunque sus casillas tengan menos opciones. Con 4 o más colores de orilla gana el orden de siempre. Knuth distingue los dos casos.
3. **Knuth no predice cuándo aparece la primera solución.** Por eso el orden interior primero solo se elige si el árbol es al menos 3 veces más chico.
4. **Portafolio:** dos equipos con órdenes distintos ayudan a encontrar soluciones (Harris 7:2 s02: de 152 s a 5,7 s). Pero cada grupo sin solución tarda el doble (11x11: 46 s). Queda como opción.
5. **Paridad de colores, conteo de piezas, forzar piezas con un solo lugar y repetir los mapas hasta que nada cambie:** recortan de 0 a 30 % de nodos en tableros chicos, pero cada nodo cuesta más. Quedan como opciones.
6. **El % revisado es optimista** en árboles muy desparejos: en el 9x9 se quedó en 5,3 %. La cifra de Knuth es la confiable.
