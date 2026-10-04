# Resultados de la versión 3 (motor en C), 30 de septiembre de 2026

Son los mismos 40 tableros que en V1, V2 y MkIV, con 60 s por corrida (MkIV mezcla las piezas a su manera, pero los tableros de fondo son los mismos). Todas las soluciones pasaron dos verificadores independientes (C y Python).

Máquina de pruebas: servidor Linux de 2 núcleos. En tu Ryzen 5 3600, con 6 núcleos y 12 hilos, debería ir más rápido.

## Comparación

Éxitos, con la mediana del tiempo entre paréntesis. Las corridas no resueltas cuentan como 60 s.

| Tablero | MkIV (Editor, Java) | V1 (Python) | V2 grupo a grupo, 1 núcleo | **V3 en C, 1 hilo** | **V3 en C, 2 hilos** |
|---|---|---|---|---|---|
| 5×5 (10) | 10/10 (0,05 s) | 10/10 | 10/10 | 10/10 (0,000 s) | 10/10 (0,002 s) |
| 6×6 (10) | 10/10 (0,15 s) | 10/10 | 10/10 | 10/10 (0,001 s) | 10/10 (0,001 s) |
| 7×7, 6 colores (10) | 5/10 (52 s) | 7/10 (5,2 s) | 7/10 (10 s) | **10/10 (0,36 s)** | **10/10 (0,24 s)** |
| 8×8, 8 colores (5) | 0/5 | 0/5 | 3/5 (58 s) | **4/5 (0,26 s)** | **4/5 (0,14 s)** |
| Modelo 0706 (5) | 4/5 (21 s) | 0/5 | 5/5 (17 s) | **5/5 (0,93 s)** | **5/5 (0,55 s)** |

### Los tableros que antes nadie resolvía

- **7×7, semilla 2** (2,7 millones de marcos): V3 lo resuelve en 18,9 s con 1 hilo y 9,5 s con 2.
- **7×7, semilla 9:** 9,0 s con 1 hilo y 5,4 s con 2.
- **8×8, semilla 2:** 6,3 s con 1 hilo y 3,7 s con 2.
- **8×8, semilla 5:** sigue sin resolverse en 60 s.

### Tableros más grandes del Editor (2 hilos)

| Modelo | Tamaño | Resultado |
|---|---|---|
| model_yannick_1015 | 10×10, 15 colores | resuelto en 0,004 s |
| model_yannick_1218 | 12×12, 18 colores | resuelto en 0,8 s |
| model_yannick_1116 | 11×11, 16 colores | no resuelto en 90 s: el primer grupo tiene 11 millones de marcos |
| model_yannick_1623 | 16×16, 23 colores (tamaño Eternity II) | no es práctico: el primer grupo supera 20 millones de marcos |

El archivo del modelo 1116 guarda una corrida incompleta del Editor (197 de 220 bordes). Contiene el juego de piezas, pero no está comprobado que tenga solución.

## Lectura

- **Velocidad:** el paso a C multiplica la velocidad entre unas 18 y 60 veces respecto a la V2 en los mismos tableros (salvo los casi instantáneos). Además, se añadió el salto de zonas sin firmas vivas.
- **Hilos:** 2 hilos dan entre 1,5 y 2 veces más velocidad en los casos difíciles.
- **Contra MkIV:** en el modelo 0706, V3 tarda 0,55 s y MkIV 21 s (mediana). En 7×7 y 8×8, V3 resuelve tableros que MkIV no resolvió en 60 s.
- **Límite actual:** tableros de 11×11 en adelante con pocos colores, donde un solo grupo tiene decenas de millones de marcos. Ahí hace falta la variante "firmas por lado" dentro del motor en C.
