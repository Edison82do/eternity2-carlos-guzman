# Solucionador por marcos (método de Carlos) — Eternity II, tableros cuadrados

## Idea del método

1. **Una esquina fija.** La primera esquina va siempre arriba a la izquierda. Esto elimina las 4 copias giradas de cada solución sin perder ninguna.
2. **Grupos.** Las otras 3 esquinas se permutan: 6 grupos, o menos si hay esquinas idénticas (3 o 1).
3. **Marcos.** Para cada grupo se generan todos los marcos completos. Se recorre por *tipo* de pieza, así las piezas iguales nunca producen marcos repetidos. Un grupo sin marcos se descarta.
4. **Firmas.** Al interior solo le importa la secuencia de colores que el marco muestra hacia adentro (la "firma"). Muchos marcos comparten firma y cuentan como uno solo.
5. **Fusión condicional.** Los grupos se funden en uno solo si las firmas se repiten entre grupos o si el total es manejable (200 000 o menos). Si no, se analizan por separado.
6. **Interior sin orden lineal.**
   - Siempre se llena la casilla con menos opciones.
   - Tras cada pieza se comprueba que ninguna casilla ni pieza se quede sin lugar.
   - Las casillas pegadas al marco solo aceptan colores de alguna firma todavía viva. Todas las restricciones de todos los marcos se aplican a la vez con una operación AND sobre bits.
7. **Verificador independiente.** Revisa la solución final: bordes, colores y que las piezas sean exactamente las del juego.

## Archivos

| Archivo | Qué hace |
|---|---|
| `resolver.py` | Programa principal (consola o ventana) con cronómetro por fase |
| `grafico.py` | Ventana: pausa, paso a paso, cámara lenta, modo rápido |
| `marcos.py` | Esquinas, grupos, marcos, firmas, fusión |
| `interior.py` | Búsqueda del interior |
| `nucleo.py` | Piezas, lectura de modelos, generador, verificador |
| `control.py` | Pausa, cámara lenta, detener, límite de tiempo |
| `comparar.py` | Compara contra Iterative Path MkIV del Editor en los mismos tableros |
| `comparar_java/Cronometro.java` | Cronometra el solver del Editor con precisión de milisegundos |
| `modelos_editor/` | Los 12 tableros que trae el Editor. Vienen resueltos; el programa los mezcla antes de resolver |

> **Nota sobre los nombres de los modelos:** `model_yannick_0706` significa 7×7 con 6 colores, no 7×6. `1015` es 10×10 con 15 colores, `1623` es 16×16 con 23 colores, y así los demás.

## Uso (en Windows, desde esta carpeta)

```
python resolver.py --modelo modelos_editor\model_yannick_0606.txt
python resolver.py --generar 7 --colores 6 --semilla 3 --limite 60
python resolver.py --modelo modelos_editor\model_yannick_0505.txt --grafico
python grafico.py
```

Opciones de `resolver.py`:

- `--semilla N`: cambia la mezcla de las piezas.
- `--limite S`: corta la búsqueda a los S segundos.
- `--guardar archivo.txt`: guarda la solución.
- `--limite-marcos N`: corta si un grupo tiene más de N marcos.

### Ventana gráfica

- **Iniciar / Pausa / Continuar / Paso / Detener.**
- **Cámara lenta:** milisegundos de espera en cada paso (0 = sin espera). Se puede mover mientras corre.
- **Modo rápido:** corre sin animación. Usa este modo cuando quieras medir el tiempo de verdad.
- **Durante la fase de marcos** se ve el collar formándose. **Durante el interior** se ve un marco de ejemplo compatible con las firmas que siguen vivas, más las piezas del interior.
- **Abrir modelo… / Generar:** cargan o crean otro tablero (N, colores y semilla).

## Comparar con el Editor

Requiere Java y el jar del Editor en la carpeta de arriba (`..\EternityEditor-1.6.0.jar`).

```
python comparar.py --tamanos 5 6 7 --colores 6 --semillas 10 --limite 60
python comparar.py --modelos modelos_editor\model_yannick_0706.txt --semillas 5 --limite 60
```

Guarda el detalle en `resultados_comparacion.csv` e imprime un resumen con éxitos, mediana y media.

Las corridas no resueltas cuentan como el límite de tiempo.

**Importante:** este programa está en Python y el Editor en Java. Java ejecuta del orden de 50 a 100 veces más pasos por segundo. Por eso, además del tiempo, conviene mirar los **nodos** (pasos de búsqueda) de cada método, que aparecen en el CSV.
