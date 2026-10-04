# El Eternity II real (el que nadie ha resuelto)

## Qué es

- **Tablero:** 16×16 = **256 piezas**. Lo lanzó Tomy en 2007, con un premio de 2 millones de dólares que expiró sin ganador el 31 de diciembre de 2010.
- **Piezas:** 4 esquinas, 56 de orilla y 196 interiores.
- **Colores:** 22, más el gris del borde.
  - **5 de orilla** (en el archivo: 1, 5, 9, 13 y 17). Solo aparecen entre piezas del borde, 24 veces cada uno.
  - **17 interiores**, que aparecen 48 o 50 veces cada uno.
- **Pista obligatoria del concurso:** la pieza de inicio (la n.º 139 del archivo) debía ir en la casilla **I8** (columna I, fila 8), con una orientación fija. Había además 4 pistas opcionales en otros puzzles del concurso.
- **Estado:** **nunca se ha resuelto por completo.** El mejor resultado es 470 de 480 bordes (Blackwood 2021, igualado por Bucas en 2024).

## El archivo

`eternity2_real.txt`, en esta carpeta, tiene las 256 piezas en el formato del programa: 16 filas de 16 piezas, 4 números por pieza (N E S O).

- **Qué es:** es la lista de piezas, **no una solución**. Las piezas están puestas en el orden de la lista, así que el tablero está "desarmado".
- **Fuente:** la lista pública `e2pieces.txt` que circula en la comunidad. La tomé del repositorio github.com/lumy/EternityII.
- **Conversión:** ese archivo guarda cada pieza como (N, S, O, E); aquí se reordenó a (N, E, S, O).
- **Comprobación:** con este orden, los 5 colores de orilla quedan solo junto al gris, como debe ser.

## Cómo cargarlo

```
e2marcos.exe eternity2_real.txt -l 300
```

o, en la ventana (`python ventana_v3.py`), con **Elegir archivo…** y seleccionando `eternity2_real.txt`.

## Qué va a pasar (probado)

El programa lo carga bien, pero **el método actual no avanza con este tablero**:

- **Los marcos:** con 56 piezas de orilla y solo 5 colores de orilla, las formas de armar cada lado son tantas que el programa ni siquiera puede contarlas.
- **Las firmas por lado:** un solo lado supera el tope de 10 millones de cadenas. El programa avisa y termina a los pocos minutos, en vez de agotar la memoria.
- **La pista de la pieza 139 en I8** todavía no está implementada: el motor no permite fijar piezas.

**En resumen:** para el Eternity II real, el método necesita otra idea para el marco, o empezar desde la pieza fija del centro. Es el siguiente reto de investigación, no algo que tu PC pueda resolver con la versión actual.
