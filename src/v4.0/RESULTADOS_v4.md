# Resultados de la versión 4 — 30 de septiembre de 2026

**Condiciones de prueba:**

- Máquina: la nube, con **2 núcleos**. En tu Ryzen con 12 hilos debería ir bastante más rápido.
- Límite: 30 s por tablero.
- Verificación: cada solución se comprobó dos veces, dentro del motor y con el verificador independiente de Python.
- **No hubo ninguna solución falsa.**

## 1. Los 40 tableros de siempre (corrección y velocidad)

Mediana en segundos:

| Tableros | V4 unificado (por defecto) | V4 `--auto` (= comportamiento V3) |
|---|---|---|
| 5x5 (10) | 10/10 — 0.018 s | 10/10 — 0.008 s |
| 6x6 (10) | 10/10 — 0.019 s | 10/10 — 0.019 s |
| 7x7 (10) | 10/10 — 0.089 s | 10/10 — 0.184 s |
| 8x8, 8 colores (5) | 4/5 — 0.93 s | 4/5 — 2.52 s |
| modelo 0706 (5) | 5/5 — 0.29 s | 5/5 — 0.57 s |

El 8x8 s5 no salió en 30 s con 2 núcleos, igual que en la versión 3. En tu PC, la versión 3 lo resolvió en 80 s con 12 hilos.

## 2. Tableros tipo Harris (subconjunto ligero: semillas 1 a 3)

| Tipo | V3 | V4 `--auto` | V4 unificado | Referencia ZLA (Harris 2018, mediana) |
|---|---|---|---|---|
| 6x6 6:2 | 3/3 — 5.5 s | 3/3 — 2.3 s | **3/3 — 0.075 s** | 0.026 s |
| 7x7 6:4 | 2/3 — 10 s | **3/3** — 14 s | **3/3** — 18 s | 0.017 s |
| 8x8 7:4 | 0/3 | 0/3 | 0/3 (llega a 60 de 64 piezas) | 0.80 s |

**Lectura honesta:**

- **Lo que mejoró:**
  - En 6x6 6:2, la V4 es unas **70 veces más rápida** que la V3.
  - En 7x7 6:4 resuelve el tablero que la V3 no sacaba.
  - Ya no existe el tope de 10 millones de cadenas por lado.
- **Lo que falta:**
  - En los tableros de pocos colores de orilla, el ZLA de Harris sigue siendo cientos de veces más rápido.
  - Los 8x8 7:4 no salen en 30 s con 2 núcleos.
  - Las pruebas grandes y con los 12 hilos quedan para ti, como pediste.

## 3. Eternity II real, con la pieza 139 fija en I8

Comando: `e2marcos4 eternity2_real.txt -P 8,9,139 -t 2 -l 60`

- **Carga:** carga bien y respeta la pieza fija. La versión 3 ni siquiera podía empezar, porque un lado superaba el tope de cadenas.
- **Resultado en 60 s:** unos 26 millones de nodos. El máximo fue **212 de 256 piezas** colocadas, todas con sus bordes coincidiendo, sin ningún error.
- **Cómo compararlo con el récord:** el récord de 470/480 mide otra cosa, bordes que coinciden permitiendo algunos errores, así que las dos cifras no se comparan directamente.
- **Conclusión:** ni esta versión ni ninguna otra conocida puede resolverlo por completo.

## Cambio de opción por defecto

Por los resultados de arriba, **la ruta unificada es ahora la opción por defecto**. Para comparar, las rutas anteriores siguen disponibles:

- `--auto`: el comportamiento de la versión 3;
- `--marcos`: solo marcos completos;
- `--lados`: la ruta por lados de la V3.

Archivos con los datos: `res_unif.csv` y `res_auto.csv` (los 40 tableros); `log_h3.txt`, `log_h4a.txt` y `log_h4.txt` (Harris: V3, V4 `--auto` y V4 unificado).
