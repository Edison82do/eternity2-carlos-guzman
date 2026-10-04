# Versión 4.3: avance, estimado de Knuth y orden automático

Es la versión 4.2 con la Fase A del plan y el primer hallazgo de la Fase B. El método por marcos y firmas no cambia, y las versiones anteriores no se tocaron.

## Qué trae

| Novedad | Qué hace |
|---|---|
| **Grupo actual** en cada línea de progreso | Por ejemplo, "grupo 2/6". |
| **% revisado del grupo** | Cada nodo reparte su peso por igual entre sus opciones. En los grupos sin solución llega exactamente a 100 %. |
| **Estimado de Knuth (1975)** | Antes de buscar, miles de sondeos al azar estiman cuántos nodos tiene cada grupo. En el 11x11, para un grupo sin solución, estimó unos 2 M nodos y fueron unos 1,9 M. |
| **Orden automático** | Para cada grupo, Knuth compara dos órdenes: "casilla con menos opciones" (el de siempre) e "interior primero, orilla al final". Elige "interior primero" solo si su árbol es al menos 3 veces más chico. |
| **`--anillo2 2` por defecto** | Tus pruebas con 12 hilos lo justifican. |
| **Portafolio (opcional)** | Varios equipos de hilos recorren el mismo grupo con órdenes distintos; gana el primero que encuentra una solución o que termina el grupo. |

## Cómo leer la línea de progreso

```
... 120.0 s | grupo 1/3 | 95 M nodos | revisado 5.3%, faltan ~36 min (Knuth: ~430 días) | máximo colocado 61 de 81 | hilos 12 de 12
```

- **"revisado" y "faltan" según el avance:** son optimistas cuando el árbol está muy desparejo. Al principio se descartan rápido muchas ramas cortas y el porcentaje sube, y luego se queda quieto en una rama enorme.
- **"Knuth":** es lo que falta según el tamaño estimado del árbol, a la velocidad actual. Es la cifra más confiable para el orden de magnitud.
- **El estimado vale para revisar el grupo entero.** Si el grupo tiene solución, puede aparecer mucho antes.

## Opciones nuevas

| Opción | Qué hace |
|---|---|
| `--estimar S` | Segundos en total de sondeo de Knuth. Por defecto: 3 s desde 8x8 y 0 en tableros más chicos. `0` lo apaga. |
| `--orden N` | `0` menos opciones, `1` interior primero. Por defecto, automático. |
| `--orden-knuth` | Recorre los grupos del más chico al más grande según Knuth. |
| `--solo-estimar` | Solo estima los grupos y sale. Sirve para medir una idea en minutos sin resolver el tablero. |
| `--portafolio N` | `0` no (por defecto); `2` un equipo por orden; `4` además dos equipos que prueban las opciones en orden al azar. |
| `--paridad 1`, `--conteo 1`, `--forzar 1`, `--punto-fijo 1` | Ideas de la Fase B que se midieron y casi no ayudan. Quedan como opciones para seguir probándolas. |

En la ventana (`python ventana_v43.py`) están "Orden de casillas" y "Portafolio". La línea de estado muestra el grupo y el % revisado.

## Archivos

| Archivo | Qué es |
|---|---|
| `e2marcos43.exe` | Motor para tu Ryzen (Zen 2). |
| `e2marcos43_compatible.exe` | Mismo motor, para cualquier PC de 64 bits. |
| `e2marcos43.c` | Código fuente. |
| `ventana_v43.py`, `bench_v43.py` | Ventana y prueba en lote. |
| `RESULTADOS_v43.md` | Mediciones. |

**Sobre Windows:** el Control de aplicaciones inteligente (Smart App Control) bloquea a veces un ejecutable nuevo. Si pasa, hay que compilar otra vez con otras opciones; ya ocurrió con una de las versiones de prueba.
