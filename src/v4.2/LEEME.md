# Versión 4.2: mapa del segundo anillo (idea de Carlos)

Es la versión 4.1 más una idea de Carlos: **el marco interior (segundo anillo) también filtra al marco exterior**. Las versiones anteriores no se tocaron.

## Cómo se aplicó la idea

No se enumeran todos los marcos interiores, porque serían muchísimos más que los exteriores. En cambio, cada lado del segundo anillo se trata como un mapa, igual que los lados de la orilla desde la V4:

1. **Revisión de la fila completa:** el programa recorre la fila (o columna) del segundo anillo de ida y de vuelta. Borra las piezas y giros que no forman ninguna fila completa compatible.
2. **Filtro hacia la orilla:** con los colores que ese lado del segundo anillo todavía puede mostrar hacia afuera, se restringen las piezas de orilla vecinas.
3. **Vuelta al marco exterior:** si la orilla cambió, se vuelve a pasar el mapa de ese lado del marco exterior.

La opción `--anillo2 2` extiende lo mismo a **todas** las filas y columnas interiores.

## Opción nueva

| Opción | Qué hace |
|---|---|
| `--anillo2 0` | Apagado (igual que la V4.1) |
| `--anillo2 1` | Solo el segundo anillo |
| `--anillo2 2` | Todas las filas y columnas interiores |
| (sin la opción) | Automático: 0 hasta 8x8, 2 desde 9x9 |

En la ventana (`python ventana_v42.py`) está como "Mapa del 2.º anillo".

## Pruebas que te tocan

```
e2marcos42.exe bateria_investigacion\harris_8x8_7-2_s01.txt -t 12 --anillo2 0
e2marcos42.exe bateria_investigacion\harris_8x8_7-2_s01.txt -t 12 --anillo2 1
e2marcos42.exe bateria_investigacion\harris_8x8_7-2_s01.txt -t 12 --anillo2 2
e2marcos42.exe bateria_investigacion\editor_1116_s01.txt -t 12
```

- **Harris 8x8 7:2:** con la V4.1 tardó 230 s. Es el tipo de tablero, con pocos colores de orilla, para el que se pensó esta idea, así que es la prueba más importante.
- **11x11 (editor_1116):** con la V4.1 tardó 61,6 s.
