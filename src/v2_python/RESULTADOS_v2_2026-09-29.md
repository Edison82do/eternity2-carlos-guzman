# Resultados de la versión 2 (29 de septiembre de 2026)

Se usaron los mismos tableros y las mismas mezclas que en la versión 1, con un límite de 60 s por corrida. MkIV (Iterative Path MkIV | Human, del Editor) usa 1 núcleo. Esta máquina de pruebas tiene 2 núcleos.

## Éxitos (mediana de tiempo entre paréntesis; una corrida no resuelta cuenta como 60 s)

| Tablero | MkIV | V1 | V2 todos los grupos juntos, 1 núcleo | V2 todos los grupos juntos, 2 núcleos | **V2 grupo a grupo, 1 núcleo** | **V2 grupo a grupo, 2 núcleos** |
|---|---|---|---|---|---|---|
| 5×5 (10) | 10/10 | 10/10 | 10/10 | 10/10 | 10/10 | 10/10 |
| 6×6 (10) | 10/10 | 10/10 | 10/10 | 10/10 | 10/10 | 10/10 |
| 7×7 (10) | 5/10 (52 s) | 7/10 (5.2 s) | 7/10 (5.5 s) | 7/10 (3.6 s) | 7/10 (10 s) | **8/10 (5.4 s)** |
| 8×8, 8 colores (5) | 0/5 | 0/5 | 0/5 | 0/5 | **3/5 (58 s)** | **4/5 (30 s)** |
| modelo 0706 (5) | 4/5 (21 s) | 0/5 | 0/5 | 5/5 (43 s) | **5/5 (17 s)** | **5/5 (8.8 s)** |

## Qué aportó cada cambio

- **Grupo a grupo (idea de Carlos).** Fue la mejora más grande:
  - En el modelo 0706 pasó de 0/5 a 5/5, y con 1 núcleo gana a MkIV en éxitos y en tiempo.
  - En 8×8 pasó de 0/5 a 3/5 con 1 núcleo, y a 4/5 con 2. MkIV no resolvió ninguno.
  - El motivo: cada grupo por separado tiene muchas menos firmas. El filtro es más fuerte y cada paso es más barato. Además, si el grupo correcto sale pronto en el orden, no se toca el resto.
  - En 7×7 no cambia el número de éxitos con 1 núcleo, y en algunos tableros tarda más, cuando el grupo con solución sale tarde en el orden.
- **Descartar en el siguiente grupo las firmas de un grupo ya revisado sin solución.** Es correcto: el interior solo depende de la firma, y se comprobó en 150 tableros sin ningún error. En estos tableros quitó pocas firmas (del 2 al 4 %, 771 en total en la prueba de 150 tableros), porque los grupos casi no comparten firmas. Puede ayudar más en tableros donde los grupos compartan más firmas.
- **Firmas por lado.** Resuelve el problema de tiempo al generar marcos: en el 7×7 con 2,7 millones de marcos, preparar el marco pasa de casi 1 minuto a unos 3 s, contando la poda entre lados. Pero como filtro es más débil que el marco completo, así que ese tablero sigue sin resolverse. Por eso el modo `auto` solo lo usa cuando hay más de 150 000 marcos.
- **2 núcleos (modo dividir).** Con 2 núcleos va aproximadamente al doble de rápido. En los tableros difíciles convierte algunos casi-éxitos en éxitos: el 8×8 de 3/5 a 4/5 y el 7×7 de 7/10 a 8/10. Tu PC seguramente tiene más núcleos.

## Sin resolver todavía (en 60 s)

- 7×7, semillas 2 y 9.
- 8×8, semilla 5.

El detalle de cada corrida está en `resultados_comparacion_v2.csv`: método usado, marcos, firmas y nodos. Las filas de MkIV y de la V1 están en `..\solucionador_marcos\resultados_comparacion.csv`.
