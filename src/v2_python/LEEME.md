# Solucionador por marcos — VERSIÓN 2

La versión 1 (`..\solucionador_marcos`) queda intacta. Esta carpeta es independiente.

## Qué cambia respecto a la versión 1

0. **Grupo a grupo (idea de Carlos; es la opción por defecto).** Así se trabaja:
   - Los grupos se ordenan del más barato al más caro. Contar las cadenas por lado cuesta milisegundos.
   - Se resuelve **un grupo completo cada vez**: sus marcos, sus firmas y su interior, con todos los núcleos en ese grupo.
   - Si el grupo no tiene solución, se pasa al siguiente, **quitándole las firmas que ya se probaron sin éxito** en los grupos anteriores. Esto es válido porque el interior solo depende de la firma.
   - Con `--estrategia fundir` se vuelve al comportamiento de antes (todos los grupos a la vez). En la ventana es la casilla **"Grupo a grupo"**.
1. **Firmas por lado.** Cada uno de los 4 lados del marco se trata como una cadena de piezas de orilla que va de una esquina a la siguiente. Hay muchísimas menos cadenas que marcos completos. En un 7×7 donde la v1 generó 2 700 000 marcos (casi 1 minuto), la v2 genera unas mil cadenas por lado en 0,03 s.
2. **Piezas sin repetir entre lados.** Es lo único que une a los lados, y se comprueba así:
   - **Al preparar:** se descarta una cadena si, para algún otro lado, no existe ninguna cadena compatible con ella. También se descarta el grupo entero si no existe ningún marco completo.
   - **Durante el interior:** cada vez que la firma de un lado queda decidida, se comprueba que los lados decididos se puedan armar juntos sin repetir piezas. Si no se puede, se retrocede.
3. **Método automático (`--metodo auto`, por defecto).** Si los marcos completos salen pocos (150 000 o menos, ajustable con `--tope-marcos`), usa el filtro de la v1, que es más fuerte. Si salen más, cambia a firmas por lado. También se puede forzar con `--metodo marcos` o `--metodo lados`.
4. **Varios núcleos (`--procesos N`).** Hay dos modos:
   - `--modo dividir` (por defecto): cada proceso explora una parte distinta de la búsqueda del interior, sin repetir trabajo.
   - `--modo portafolio`: cada proceso explora todo, pero probando las piezas en distinto orden. Gana el primero que termina.

   La generación de marcos o cadenas no se reparte porque ya es muy rápida; lo que se reparte es la búsqueda del interior. La ventana gráfica usa siempre 1 proceso.

## Uso

```
python resolver.py --generar 7 --colores 6 --semilla 3 --limite 60
python resolver.py --generar 7 --colores 6 --semilla 3 --limite 60 --procesos 4
python resolver.py --generar 7 --colores 6 --semilla 3 --estrategia fundir
python resolver.py --modelo modelos_editor\model_yannick_0706.txt --metodo lados
python grafico.py
python comparar.py --tamanos 5 6 7 --semillas 10 --limite 60 --procesos 1 4
```

- En la ventana hay un selector de **Método** (auto / marcos / lados).
- Durante el interior con firmas por lado, el panel muestra cuántas firmas siguen vivas en cada lado (arriba / derecha / abajo / izquierda).
- `comparar.py --sin-java` mide solo la v2, sin el Editor.

Los resultados de las pruebas están en `RESULTADOS_v2_2026-09-29.md` y `resultados_comparacion_v2.csv`.

En Windows, usar varios procesos tarda un poco en arrancar (menos de 1 s). En tableros fáciles puede salir más lento que 1 proceso.

Los archivos `nucleo.py`, `control.py` y `marcos.py` son los mismos de la v1. `interior_marcos.py` es el buscador de la v1 con el reparto entre procesos añadido.
