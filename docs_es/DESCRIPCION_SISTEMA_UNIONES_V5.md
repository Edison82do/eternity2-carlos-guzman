# Sistema de uniones (Carlos Edison Guzman Marte) — versión 5 (5.0 a 5.4)

**Fecha:** 2 de octubre de 2026, Calgary.

**La idea del autor:**
- Se parte de un tablero sintético ya armado, con la misma proporción de colores que el tablero objetivo, y se van introduciendo las piezas reales una por una, sin que el tablero deje de estar armado.
- Las esquinas tienen sus colores fijos, pero pueden moverse entre las cuatro esquinas.
- Las piezas conocidas (pistas) quedan fijas y van primero. Sin pistas, se fija una sola esquina.

**Implementación:**
- **Representación:** se eligen los colores de las uniones. Una casilla es "real" si su pieza existe en el juego.
- **Búsqueda:** recocido simulado con dos movimientos, intercambio de uniones e introducir pieza.
- **Versiones:**
  - prototipo en Python con ventana;
  - motor en C (1,5 millones de pasos por segundo por hilo);
  - todos los núcleos, en modo independiente o con réplicas (parallel tempering);
  - panel gráfico, con modo de máxima velocidad;
  - **cierre exacto**: la mezcla con el método por marcos, que rehace exactamente la zona problemática;
  - **carrera** contra la V4.6;
  - **puntaje normal** (uniones que encajan, sobre 480 en el Eternity II);
  - carga automática de las 5 pistas oficiales.

**Resultados en la PC del autor, 12 hilos:**
- Con cierre exacto, los 7x7 y 8x8 generados salen en 0,4 a 10 s. Sin cierre, 2 de 6.
- En los 8x8 generados, iguala o mejora a la V4.6.
- Harris 8x8 7:2: 4 de 10 (la V4.6, 10 de 10).
- Eternity II oficial con 5 pistas: 220 de 256 piezas reales en 15 s, equivalentes a 411 de 480 uniones.

**Hallazgos:**
- **Casi-soluciones falsas:** en el 9x9, 74 de 81 piezas reales, pero solo 7 en su sitio correcto.
- **Peso del marco:** con la mitad del marco correcto, el 9x9 sale al instante.
- **Probado y descartado:** ventana exacta, grupos de esquinas, precios de Lagrange, búsqueda guiada y esqueleto.

Trabajo hecho con ayuda de herramientas de IA. Las ideas, los métodos y las decisiones son del autor.
