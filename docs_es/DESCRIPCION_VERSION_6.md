# Versión 6: la tarjeta de video (Carlos Edison Guzman Marte)

**Fecha:** 3 de octubre de 2026, Calgary.
**Equipo:** NVIDIA GeForce GTX 1070 (8 GB) y procesador de 12 hilos, en la PC del autor.

## Qué se probó

1. **Recocido masivo en la tarjeta** (`gpu_recocido.py`, `nucleo2.cu`).
   - Siempre con las piezas reales: se intercambian y giran para que encajen más uniones (el puntaje del récord).
   - Miles de tableros a la vez, en una escalera de temperaturas (réplicas), a unos 750 a 840 millones de pasos por segundo.
   - El procesador hace el **cierre exacto**: rehace la zona de las uniones malas con búsqueda por retroceso, como el método por marcos.
   - Resultados:
     - 7x7 y 8x8 generados: 6 de 6, en 2 a 37 s;
     - Harris 8x8: 0 de 3 (110 de 112 uniones);
     - 9x9 con 5 pistas: 137 de 144 uniones.
   - Adivina bien, pero no tiene garantía.

2. **Búsqueda exacta en la tarjeta** (`gpu_exacto.py`, `exacto.cu`).
   - Recorre todo el árbol, por filas, con poda por colores.
   - El procesador corta el árbol en cientos de miles de prefijos y cada hilo de la tarjeta agota los suyos.
   - Si termina sin solución, es una prueba de que no la hay.
   - Velocidad (tres versiones del núcleo, la 3 con las piezas usadas en la memoria compartida):
     - 1 400 a 1 900 millones de nodos por segundo;
     - el procesador con 12 hilos y el mismo método hace 290 millones.
   - Resultados:
     - Harris 8x8: 10 de 10, en 0,4 a 29 s;
     - 9x9 con 5 pistas: 62 / 8 / 9 / 157 / 6 s (unos 240 s en total; la V4.6, 150 s; la V4.7 con creencias, 33 s);
     - 10x10 con 5 pistas: sin resolver en 30 min (máximo 85 de 100 casillas; el árbol completo tomaría semanas).

3. **La poda de la V4.6 en la tarjeta** (`gpu_dominios.py`, `dominios.cu`).
   - Lo que se llevó:
     - dominios por casilla;
     - la casilla con menos opciones, el interior primero;
     - quitar la pieza puesta de las demás casillas y recortar las vecinas;
     - mapas perezosos de todas las filas y columnas;
     - revisar que cada pieza que falta quepa en algún lado.
   - Funciona y resuelve: 9x9 s2 en 30 s, 9x9 s5 en 20 s, Harris 8x8 5 de 5.
   - Velocidad: la tarjeta hace solo 0,1 millones de nodos por segundo; el mismo código en el procesador con 12 hilos, 1 millón.

4. **Creencias con la tarjeta.**
   - Las creencias normales (1x1) no necesitan la tarjeta: el 16x16 sale en unos 3 s en el procesador.
   - Las creencias por bloques de 2x2 en la V4.7, 10x10 con 5 pistas: no mejoraron a las de 1x1, sin resolver en 30 min.
   - Máximo colocado, 2x2 contra 1x1:
     - s1: 76 contra 76;
     - s2: 73 contra 73.

## Conclusión del autor

- La tarjeta de video sirve para trabajo sencillo y masivo: el recocido y la búsqueda simple por filas.
- La poda inteligente del método por marcos (V4.6/V4.7) rinde más en el procesador.
- Con pistas, el método por marcos con creencias sigue siendo el más rápido en los tableros medianos.

## Contenido del registro

- La carpeta `proyecto_gpu`: los motores, los núcleos de la tarjeta, las librerías para el procesador y el panel.
  - El panel tiene dos métodos: recocido y búsqueda exacta.
- Los registros de las pruebas en la PC del autor.
- El manifiesto con la huella SHA-256 de cada archivo.

Trabajo hecho con ayuda de herramientas de IA. Las ideas, los métodos y las decisiones son del autor.
