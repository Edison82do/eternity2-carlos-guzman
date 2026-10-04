# Análisis del informe "IA local para acelerar Eternity II" (1 de octubre de 2026)

Guardado para tratarlo con calma más adelante. El informe está en `informe_IA_para_Eternity_II.txt`.

## En qué coincide con lo que ya medimos

- **La idea central (IA = guía, solucionador exacto = garantía) es sana.** No pierde soluciones, porque la IA solo cambia el orden.
- **La sección 5, que la IA ordene los candidatos, ya la probamos a mano:** es `--valor 1`, que prueba primero la pieza menos restrictiva. Resolvió 3 tableros que no salían y bajó 7:3 s05 de 130 s a 22 s. Una IA sería una versión aprendida de esa misma regla, así que hay base para esperar ganancias.
- **La sección 6, qué casilla llenar primero, también tiene base.** "Interior primero" cambió hasta 32 veces los tiempos en los Harris 7:2, y el orden fila por fila cambió el estimado 2,3 veces en el 9x9.
- **Las secciones 16 a 18, un modelo chico y en la CPU, son correctas.** Nuestro motor revisa unos 70 000 nodos por segundo y por hilo, unos 14 µs por nodo. El modelo tendría que costar pocos microsegundos por candidato. Mandar cada decisión a la GPU sería más lento que no usarla.
- **Las secciones 20 a 22, medir nodos y comparar contra nuestras heurísticas, coinciden con nuestro método.**

## Cuidados, según nuestros datos

1. **Ordenar candidatos solo ayuda en grupos que tienen solución.** En un grupo sin solución hay que revisarlo todo, y el orden de los valores no cambia el total de nodos. Ahí solo ayuda podar más o cambiar el orden de las casillas (sección 6).
2. **La sección 7, "abandonar ramas malas", rompe la completitud** si se abandona de verdad. Hay que usarla para ordenar o retrasar, nunca para descartar.
3. **Mucho ruido:** cada estrategia resuelve unos 9 de 16 tableros difíciles, pero no los mismos (cola pesada). Para saber si la IA aprende algo hacen falta muchas corridas, como dice la sección 21.
4. **Las métricas de la sección 20 incluyen "470 coincidencias".** Carlos descartó perseguir puntuaciones parciales (avance ficticio). Para nosotros la métrica es resolver de verdad, con nodos y tiempo.
5. **La sección 24 tiene razón:** el 9x9 9:3 parece tener una sola solución y un árbol de unos 10¹³ nodos por grupo. Una IA podría acelerarlo, pero no es magia.

## Lo que más vale del informe para nosotros

- **El solucionador como maestro (secciones 9 y 13):** ya tenemos el generador de tableros, la batería de prueba, el registro de nodos y **el estimado de Knuth**. Este último puede dar, sin resolver nada, la etiqueta "tamaño del subárbol de cada candidato" en tableros medianos. Es el dato que la IA necesita aprender (sección 26: "qué decisión reduce más el trabajo restante").
- **Fragmentos del tablero real (sección 12):** encaja con la idea de la pieza fija.

## Primer experimento posible (cuando lo retomemos)

1. Registrar, en tableros 6x6 a 8x8, cada nodo con sus candidatos y el tamaño real del subárbol de cada uno.
2. Entrenar un modelo diminuto (lineal o MLP pequeño) con datos sencillos: colores de las vecinas, opciones que deja, piezas restantes de cada color.
3. Usarlo como `--valor 2` y comparar contra `--valor 1` y el orden normal, en los 16 Harris difíciles y en muchos tableros chicos con varias corridas.
