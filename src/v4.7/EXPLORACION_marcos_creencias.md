# Exploración: cantidad de marcos, creencias dentro de la búsqueda y creencias por bloques de 2x2 (2 de octubre de 2026)

Todo se midió en la nube, en tableros con solución conocida. Scripts: `marcos.py`, `creencias2x2.py`.

## 1. ¿Cuántos marcos hay y cuántos sobreviven?
Estimados de Knuth (40 000 caminos al azar) con la esquina de arriba a la izquierda fija.
- **Firma:** la secuencia de colores que el marco muestra hacia adentro. Dos marcos con la misma firma son el mismo problema para el interior.
- **Anillo posible:** que el anillo siguiente, el primero del interior, se pueda armar con piezas interiores.

| | Marcos válidos | Firmas distintas | Marcos por firma (mediana) | Firmas con anillo posible |
|---|---|---|---|---|
| 9x9 9:3 | ~9,5 × 10¹⁶ | ~1,5 × 10¹⁵ | 48 | ~2,2 × 10¹⁴ (14 %) |
| 10x10 10:3 | ~6,0 × 10²⁰ | ~5,7 × 10¹⁹ | 8 | ~3,2 × 10¹⁹ (59 %) |

**Lectura:**
- Las firmas sí reducen: 63 veces menos en 9x9 y 10 veces menos en 10x10. Pero la reducción **se achica al crecer el tablero**.
- La prueba del anillo quita entre el 40 % y el 86 %.
- Aun así quedan ~10¹⁴ (9x9) y ~10¹⁹ (10x10) firmas, y cada una necesitaría su búsqueda del interior. La V4.6 recorre el 9x9 entero en ~10¹⁵–10¹⁶ nodos, buscando marco e interior a la vez.
- Recorrer marco por marco, aun agrupando por firma, sale **más caro** que la búsqueda unificada actual. La V4.5 ya había llegado a lo mismo: juntar las esquinas en un solo recorrido fue mucho más rápido que hacerlo por grupos.

## 2. Creencias recalculadas dentro de la búsqueda
10x10 s2 con 5 pistas; prefijos colocados fila por fila (el orden real de una búsqueda):

| Prefijo | ¿Las creencias distinguen el correcto de los equivocados? | V4.6 con el prefijo equivocado |
|---|---|---|
| 20 piezas | No (mismo encaje y confianza) | lo descarta **al instante, con 0 nodos** |
| 30 piezas | No | al instante, 0 nodos |

- El prefijo correcto de 20 piezas, la V4.6 lo completa en 3 806 nodos.
- Con 30 piezas **del marco** las creencias sí distinguen, pero ahí la búsqueda exacta también descarta lo equivocado al instante.

**Conclusión:** recalcular las creencias dentro de la búsqueda no da poda que la búsqueda exacta no tenga ya. Como orden de prueba, las creencias fijas de la V4.7 siguen siendo útiles.

## 3. Creencias por bloques de 2x2
Cada bloque solo admite 4 piezas distintas que encajan entre sí. Los bloques se "hablan" por el par de colores que comparten.
- 1,4 millones de combinaciones en total; unos 9 s en Python.

| 10x10 con 5 pistas | Puesto de la verdadera, 1x1 (media / mediana) | 2x2 (media / mediana) |
|---|---|---|
| s1 | 30,8 / 14 | 28,8 / 14 |
| s2 | 19,9 / 10 | 16,4 / 7 |

**Lectura:** mejora poco. Podría reemplazar a las creencias 1x1 en la V4.7 como una mejora menor, pero no cambia el panorama.
