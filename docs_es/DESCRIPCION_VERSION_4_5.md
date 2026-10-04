# Método por marcos y firmas (Carlos) — versión 4.5

**Fecha:** 1 de octubre de 2026, Calgary.

Es la versión 4.4 más las **esquinas juntas**: las 4 esquinas son casillas de la búsqueda, con sus opciones filtradas por los mapas de la orilla. Así el interior se busca una sola vez para todos los grupos de esquinas, en vez de repetirse en cada grupo. Es la opción por defecto desde 8x8.

**Resultados en la PC del autor, con 12 hilos:**
- 11x11 (editor_1116): de 20 s a 0,6 s.
- 16 Harris 8x8 difíciles: 14 resueltos en 5 min (la V4.4, 11; la V4.3, 9).
- 9x9 9:3 con 5 piezas fijas: entre 0,04 s y 88 s (la V4.4, entre 8 s y 6 min).
- Tableros nuevos con solución conocida (`tableros_conocidos`) para investigar las piezas fijas.

**Completitud:**
- 96 de 96 casos con el mismo resultado que la V4.2.
- Piezas fijas tomadas de soluciones giradas: 144 de 144 por grupos; con las esquinas juntas forzadas, 142 de 144 en 60 s (los otros 2 tardan más).
