# Método por marcos y firmas (Carlos) — versión 4.3

**Fecha:** 30 de septiembre de 2026, Calgary.

Es la versión 4.2 (con el mapa del segundo anillo, idea del autor) más:

1. **Avance del grupo:** grupo actual y % revisado en cada línea de progreso. El peso se reparte por igual entre las opciones y lo cedido entre hilos se descuenta.
2. **Estimado de Knuth (1975) por grupo:** sondeos al azar, en paralelo, estiman el tamaño del árbol de cada grupo antes de buscar.
3. **Orden automático por grupo:** el estimado compara "casilla con menos opciones" e "interior primero, orilla al final", y elige el segundo solo si su árbol es al menos 3 veces más chico. En tableros con pocos colores de orilla, el recorte medido es de 15 a 32 veces en tiempo y de unas 50 veces en el tamaño estimado del árbol.
4. **Portafolio opcional:** equipos de hilos con órdenes distintos, o con las opciones en orden al azar, recorren el mismo grupo. Gana el primero que encuentra una solución o que termina el grupo.
5. **`--anillo2 2` por defecto.**
6. **Opciones de prueba:** paridad de colores, conteo de piezas, forzar piezas con un solo lugar posible y mapas de filas hasta el punto fijo.

**Completitud:** 96 de 96 casos con el mismo resultado y el mismo grupo que la V4.2 (`cmp_grupos.txt`, y `cmp_grupos_portafolio4.txt` con el portafolio de 4 equipos).
