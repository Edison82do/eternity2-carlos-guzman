# Método por marcos y firmas (Carlos) — versión 4.6

**Fecha:** 1 de octubre de 2026, Calgary.

Es la versión 4.5 más los mapas de filas "perezosos".

- **Qué cambia:** una fila o columna solo se recalcula si alguna de sus casillas cambió por algo distinto de quitar la pieza recién usada.
- **Efecto:** el doble de nodos por segundo, con el mismo árbol.
- **También:** dos correcciones de la línea de progreso, en el equipo de reinicios y en el conteo de hilos.

**Resultados en la PC del autor, con 12 hilos:**
- 11x11: 0,34 s.
- Los 13 Harris difíciles que salen con las dos versiones: 576 s en total (la V4.5, 932 s).
- 9x9 9:3 con 5 pistas: entre 0,03 s y 95 s.

**Completitud:** 96 de 96 casos con el mismo resultado que la V4.2, y 144 de 144 casos con pieza fija.
