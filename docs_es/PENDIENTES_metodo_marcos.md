# Pendientes del método por marcos y firmas

Anotado el 30 de septiembre de 2026. **Hacerlo cuando Carlos esté en casa y la PC esté libre.**

## Para la próxima versión

1. **Indicador de avance:** porcentaje aproximado del grupo ya revisado, calculado a partir de las ramas grandes que ya terminaron.
2. **Tiempo estimado por grupo:** con el método de Knuth (1975), unos cientos de recorridos al azar desde la raíz estiman el tamaño del árbol.
3. **Grupo actual en cada línea de progreso**, por ejemplo: "grupo 2/6".
4. **`--anillo2 2` por defecto.** Las pruebas de Carlos lo justifican: Harris 8x8 7:2 bajó de 223 s a 49 s y el 11x11 de 61,6 s a 20,3 s.

## Ideas para bajar combinaciones (probar en este orden)

5. **Reinicios aleatorios:** si una búsqueda se alarga, reiniciarla con otro orden al azar. Cada hilo puede usar un orden distinto. Es lo que corta los casos que tardan una eternidad.
6. **Balance de colores:** las mitades de cada color en las piezas que quedan deben poder emparejarse con las del borde abierto, y su diferencia debe ser par.
7. **Aprender de los fracasos:** anotar las combinaciones pequeñas que resultaron imposibles para no volver a probarlas.
8. **Partir el tablero por la mitad:** usar firmas de la fila central, calculadas desde arriba y desde abajo.
9. **Probabilidades por casilla (propagación de creencias):** probar primero la pieza más probable en cada casilla.

## Ideas de Carlos: el inverso, el contrario, el complemento (anotado el 30-sep-2026)

Carlos preguntó si existe algo como un inversor, un contrario o un complemento que reduzca las combinaciones o cambie las probabilidades. Así se traduce cada idea:

| Idea | Cómo existe | Qué haría aquí |
|---|---|---|
| **El contrario:** buscar lo imposible en vez de lo posible | Demostrar que una zona no tiene solución y descartarla entera | Ya se usa al descartar grupos y firmas. Se amplía con "aprender de los fracasos" (punto 7). |
| **El complemento:** mirar las piezas que faltan, no las puestas | Los colores de las piezas sin usar deben cuadrar con los huecos abiertos | Es el "balance de colores" (punto 6). |
| **El inverso:** ver el problema al revés | Preguntar "en qué casilla puede ir esta pieza" en vez de "qué pieza va en esta casilla" | Si una pieza solo cabe en un sitio, se coloca sin probar. Se puede ampliar mucho. |
| **Cambiar las probabilidades** | Propagación de creencias: cada casilla estima qué pieza es más probable según sus vecinas | Se prueba primero lo más probable (punto 9). |
| **Cambio de representación** (le gusta especialmente; como en matemáticas, pasar de geometría a cálculo) | Ver el tablero como sus 480 uniones de colores, no como sus 256 piezas. Otras formas: un problema SAT, un grafo de emparejamientos o un sistema de ecuaciones de conteo de colores | Un problema que parece imposible de una forma puede volverse manejable de otra. Explorarlo a fondo. |

**Prioridad sugerida:** el balance de colores, por barato; las probabilidades; y explorar el cambio de representación.

## Estrategia general (decisión de Carlos, 30-sep-2026)

Primero se desarrollan y prueban **todas las optimizaciones** sobre la representación actual (piezas en casillas), en este orden:

1. Las del primer bloque: avance, estimado y grupo actual.
2. Las ideas para bajar combinaciones.
3. El inverso, el contrario y el complemento.

**Después**, esas mismas optimizaciones se trasladan a **otra representación que dé ventajas**: uniones de colores, ecuaciones de conteo, grafo de emparejamientos o SAT.

Pasos:

- Medir cada optimización por separado, con el mismo banco de tableros. Así se sabe cuál aporta y cuál no.
- Elegir la representación nueva probándola primero en tableros chicos.
- Comparar las dos representaciones con las mismas optimizaciones y los mismos tableros.

## Descartado por ahora

- Modo que permite errores para perseguir el récord de 470/480. Decisión de Carlos: es un avance ficticio.

## Hecho el 30 de septiembre de 2026 (noche): versión 4.3, registrada

- **Puntos 1 a 4:** grupo actual, % revisado, estimado de Knuth y `--anillo2 2` por defecto.
- **Orden automático por grupo:** "interior primero" cuando Knuth lo justifica. Harris 8x8 7:2: de 15 a 32 veces más rápido.
- **Portafolio opcional:** equipos de hilos, también con orden al azar. Cubre la idea 5 de los reinicios.
- **Medidas y con poco efecto:** paridad de colores (6), conteo de piezas, forzar piezas con un solo lugar ("el inverso") y mapas hasta el punto fijo. Quedan como opciones.
- **Prueba larga del 9x9 9:3 s01:** el grupo 1 tiene unos 3 × 10¹³ nodos con el orden nuevo. Revisarlo entero llevaría más de 400 días.
- **Lanzador de pruebas:** `lanzador_pruebas.bat` vigila `cola_pruebas\`. Claude deja ahí las pruebas y lee los registros.

## Próximo (Fase B, sigue)

- **Mejor orden dentro del interior,** medido con `--solo-estimar`. Por ejemplo, empezar por el centro o por una esquina, o elegir según la cantidad de vecinas puestas.
- **Aprender de los fracasos (7)** y **partir el tablero por la mitad (8).**
- **Probabilidades por casilla (9),** para ordenar las opciones.
- **Ramificar por pieza** cuando una pieza tiene menos lugares posibles que la casilla con menos opciones ("el inverso" completo).

## Fase B, mediciones del 1 de octubre de 2026 (madrugada)

Todo medido con el estimado de Knuth del 9x9 9:3 s01 (`--solo-estimar`). El código experimental está en `solucionador_marcos_v4_4_exp\`.

| Idea | Resultado |
|---|---|
| Emparejamiento completo casillas ↔ piezas (Hall), en la orilla y en el interior | Unos 20 % menos (dentro del ruido). No alcanza. |
| Ramificar por pieza ("¿dónde va esta pieza?", el inverso completo) | Peor: 1,7 veces más nodos. |
| Orden dentro del interior: fila por fila, y en la fila la casilla con menos opciones (`--orden 8`) | 2,3 veces menos en el 9x9. En los Harris 8x8 no encuentra la solución antes. Queda como opción. |
| Espiral, del centro hacia afuera, por diagonales | Mucho peor. |
| **Cambio de representación a SAT** (CaDiCaL y Glucose, que aprenden de cada fracaso) | **De 10 a 100 veces más lento que nuestro motor**, y el Harris 8x8 no salió en 4 min. Cambiar de representación sin más no da ventaja; haría falta un híbrido. |

**Conclusión:** el árbol del 9x9 9:3 casi no baja con trucos locales. Se calcula que estos tableros tienen alrededor de una sola solución, así que hay que revisar casi todo el árbol. Las ideas que quedan con potencial grande:

- Un solo recorrido del interior para todos los grupos a la vez (hasta 3 a 6 veces menos).
- Aprender de los fracasos dentro de nuestro motor (un híbrido).

**Base para comparar:** en la cola hay 40 Harris 8x8 (7:2, 7:3, 7:4 y 8:2) con la V4.3, 12 hilos y 300 s cada uno (pruebas 100 a 139).

## Fase B, 1 de octubre de 2026 (mañana): estrategias para encontrar la solución dentro del grupo

**Base nocturna (V4.3, 12 hilos, 300 s):** 33 de los 40 Harris 8x8 resueltos. Todas las soluciones salieron en el grupo 1, y los 7 sin resolver también se quedaron en el grupo 1.

Los 16 tableros lentos o sin resolver, con cada estrategia (en segundos; X = más de 300 s):

| Tablero | V4.3 | Reinicios (`--portafolio 3`) | Pieza menos restrictiva (`--valor 1`) | Tres estrategias (`--portafolio 5`) |
|---|---|---|---|---|
| 7:3 s08 | X | X | 74 | X |
| 7:4 s03 | X | X | X | 239 |
| 7:4 s06 | X | X | X | X |
| 7:4 s07 | X | 33 | 74 | 7 |
| 7:4 s10 | X | X | X | X |
| 8:2 s07 | X | X | X | 138 |
| 8:2 s09 | X | X | 271 | X |
| 7:3 s01 | 119 | 147 | X | X |
| 7:3 s05 | 130 | 97 | 22 | 11 |
| 7:3 s10 | 119 | X | 148 | 150 |
| 7:4 s02 | 148 | 152 | X | 164 |
| 7:4 s04 | 256 | 114 | X | X |
| 7:4 s09 | 177 | 39 | 235 | 105 |
| 8:2 s01 | 298 | X | X | X |
| 8:2 s10 | 271 | 156 | X | 290 |
| 7:2 s05 | 68 | 70 | 7 | 22 |
| **Resueltos** | 9 | 8 | 9 | 9 |

**Conclusión:**

- Cada estrategia resuelve unos 9 de 16, pero no los mismos. Entre todas resuelven 14 de 16; solo faltan 7:4 s06 y s10.
- Es "cola pesada": cuál gana depende mucho del azar.
- En el portafolio de 3 estrategias, el equipo de reinicios ganó 7 de las 9 veces, aunque tenía solo 4 hilos.
- Para decidir el valor por defecto hacen falta varias corridas por tablero, porque una sola corrida tiene mucho ruido.
- Las tres opciones son completas o van junto a un equipo completo: 96 de 96 en la prueba de completitud.

**Próximo:**

- Prueba con repeticiones (varias semillas) para elegir el valor por defecto.
- Probar un portafolio con más hilos en los reinicios.

## Idea de Carlos para probar después (1 de octubre de 2026): construir alrededor de una pieza fija de la solución

En el Eternity II oficial se da una pieza de la solución: la 139, obligatoria, en una de las 4 casillas centrales. Además había 4 piezas de pista más, de los puzles de pista (hay que confirmarlas).

**Idea:** usar todo lo que ya tenemos más una pieza fija, e investigar técnicas que partan de esa pieza.

**Grupos (observación de Carlos, correcta):** con una pieza fija y su giro dado, el tablero ya no se puede girar. Entonces la esquina de arriba a la izquierda no se puede fijar como antes: cualquiera de las 4 esquinas puede ir ahí. Son **4 × 6 = 24 grupos** (menos los repetidos).
- **Ojo:** el motor actual con `-P` sigue fijando la primera esquina arriba a la izquierda. Con una pieza fija y su giro dado, eso puede dejar fuera la solución. Hay que corregirlo antes de cualquier prueba.

**Plan de prueba:**

1. Tomar tableros chicos ya resueltos (6x6 a 9x9, Harris y gen). Fijar una pieza de su solución conocida, con su giro: primero en el centro y después en otras casillas.
2. Medir con Knuth (`--solo-estimar`) y con tiempos reales cuánto achica el árbol la pieza fija, y en qué grupos.
3. Técnicas alrededor de la pieza fija:
   - Que la búsqueda empiece por la pieza y crezca desde ella (sus vecinas quedan muy restringidas).
   - Descartar enseguida los grupos que no son compatibles.
   - Mapas de filas que pasan por la pieza.
   - Varias piezas fijas (como las 5 pistas oficiales).
4. Comparar: sin pieza fija, con 1 en el centro, con 1 fuera del centro y con 5.

## Informe sobre usar IA local (1 de octubre de 2026), para tratar con calma

Guardado en `ideas_IA_eternity\` (el informe y su análisis). En resumen: la IA como guía del orden de búsqueda, y nuestro motor exacto como garantía. Ya hay base en nuestros datos: `--valor 1` es la versión a mano de lo que la IA aprendería.

## Versión 4.4 cerrada y registrada (1 de octubre de 2026, 15:29)

- Portafolio automático en los grupos grandes. Resuelve 11 de los 16 Harris difíciles; la V4.3, 9.
- Piezas fijas corregidas: se prueban las 4 esquinas arriba a la izquierda. 144 de 144 casos.
- **Siguiente:** investigar la pieza fija (idea de Carlos, ver arriba), ya con la corrección hecha.
- **Pendientes:**
  - la prueba con repeticiones en la madrugada;
  - el informe sobre IA (`ideas_IA_eternity\`);
  - sellar en OpenTimestamps los zips de la V4.3 y la V4.4.

## Versión 4.5 cerrada y registrada (1 de octubre de 2026, 19:53): esquinas juntas

- El 11x11 pasa de 20 s a 0,6 s. De los 16 Harris difíciles resuelve 14. Los 9x9 9:3 con 5 piezas fijas, entre 0,04 s y 88 s.
- **Pendiente:** 10x10 con 5 piezas (s2 se estima en unas 13 h), 7:4 s06 y s10, la prueba con repeticiones y el informe de IA.

## Pistas oficiales (1 de octubre de 2026, noche)

- Las 5 pistas oficiales y los puzles de pista están en `solucionador_marcos_v4_5\pistas_oficiales\`. Los de 6x6 se resolvieron en 5 ms.
- Nuestra numeración de piezas coincide con la oficial: las mismas 256 piezas y la misma orientación. Solo las piezas 112 y 113 están intercambiadas, y eso no afecta a ninguna pista.
- **La prueba del tablero real con las 5 pistas la hace Carlos.** El comando está en el LEEME de esa carpeta.
- **Pendiente:** adaptar el motor a rectángulos para los puzles de pista de 12x6 (pistas 2 y 4). No es urgente.
- **Siguiente:** optimizar el uso de 5 pistas en nuestros tableros con solución conocida (8x8, 9x9 y 10x10).

## Versión 4.6 cerrada y registrada (1 de octubre de 2026, 23:15): mapas perezosos

- El doble de nodos por segundo. El 10x10 s2 con 5 pistas quedó sin resolver: Carlos lo detuvo a los 45 min. Pendiente repetirlo.

## Idea nueva de Carlos (1 de octubre de 2026): armar desde un tablero sintético

**La idea:**
- Se parte de un tablero ya armado con colores elegidos libremente, pero con la misma cantidad de cada color que el tablero objetivo.
- Se van introduciendo, una por una, las piezas reales, sin que el tablero deje de estar armado ni cambie la proporción de colores.

**Las reglas que puso Carlos:**
- **Esquinas:** se les fijan los colores, pero pueden caer en cualquier esquina y moverse.
- **Piezas conocidas** (pistas): no se mueven ni se giran, y conviene colocarlas primero.
- **Sin pistas:** se fija una sola esquina.

**Representación:** se eligen los colores de las uniones, no las piezas. Así el tablero queda armado siempre, y el reto es que las piezas que se forman sean las reales.

**Plan:**
1. ~~Prototipo en Python con ventana gráfica~~ (hecho).
2. ~~Probarlo en tableros chicos y medianos~~ (hecho).
3. ~~Pasarlo a C, con todos los núcleos (réplicas)~~ (hecho).
4. ~~Combinarlo con el motor exacto~~: **cierre exacto** (v5.3) y **carrera** marcos contra uniones (hecho).

**Estado (2 de octubre de 2026, v5.3).** Los detalles están en `sistema_uniones_v5\LEEME.md`.
- **Con cierre exacto,** los 7x7 y 8x8 generados salen en 0,4 a 10 s con 12 hilos. En los 8x8 generados va igual de rápido o más que la V4.6.
- **Harris 8x8 7:2:** la V4.6 los resuelve en 3 a 9 s; uniones solo resuelve algunos.
- **Desde 9x9 no sale.** Hay "casi-soluciones falsas": 74 de 81 piezas reales, pero solo 7 en su sitio correcto.
- **Con la mitad del marco correcto,** los dos sistemas resuelven el 9x9 al instante; con un cuarto, ninguno de los dos.

**Pendientes de la línea uniones:**
- Atacar las casi-soluciones falsas: otra función objetivo, o introducir las piezas en orden y congelarlas, como propuso Carlos.
- Probar la carrera en la PC de Carlos.
- Ver si uniones sirve para elegir marcos prometedores para la V4.x.

Es una línea aparte: la "versión 5".

## Publicación (pendiente, idea de Carlos, 2 de octubre de 2026)

- Organizar todo lo descubierto y publicarlo **en inglés, con todos los detalles**, con el nombre de Carlos como autor en todo.
- Incluir: método por marcos y firmas (V4.3 a V4.7), sistema de uniones (v5), cierre exacto, creencias, carrera, tablas de resultados y lo que se probó y no funcionó.
- Antes de publicar: decidir qué se hace público y verificar el sellado OpenTimestamps de los zips registrados.
- Autor: **Carlos Edison Guzman Marte** (República Dominicana). Usar ese nombre completo en todo.
- Declarar que el trabajo se hizo con ayuda de IA.
