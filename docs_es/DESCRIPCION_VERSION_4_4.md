# Método por marcos y firmas (Carlos) — versión 4.4

**Fecha:** 1 de octubre de 2026, Calgary.

Es la versión 4.3 más:

1. **Portafolio automático por grupo:** si el estimado de Knuth de un grupo supera 10⁹ nodos, tres equipos de hilos buscan a la vez en el mismo grupo:
   - búsqueda normal, completa;
   - primero la pieza menos restrictiva, completa;
   - reinicios con las opciones en orden al azar y un tope que crece según la serie de Luby.

   Gana el primero que encuentra una solución o que termina el grupo. En los 16 Harris 8x8 más difíciles resolvió 11 (la V4.3, 9), con tres que la V4.3 nunca resolvía.
2. **Piezas fijas corregidas** (observación del autor): con una pieza fija se prueban las 4 esquinas arriba a la izquierda, hasta 24 grupos. Antes se podía responder "sin solución" por error. Ahora resuelve 144 de 144 casos de prueba.
3. **Opciones de investigación de la Fase B:** pieza menos restrictiva, reinicios, emparejamiento global, órdenes del interior y el experimento SAT.

**Completitud:** 96 de 96 casos con el mismo resultado y el mismo grupo que la V4.2, también con las tres estrategias forzadas.
