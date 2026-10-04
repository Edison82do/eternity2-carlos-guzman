# Registro privado del método por marcos (autor: Carlos)

Creado el 29 de septiembre de 2026, a las 19:44, hora de Calgary. **Nada de esto es público.**

## Qué hay en esta carpeta

| Archivo | Para qué sirve |
|---|---|
| `metodo_marcos_carlos_2026-09-29.zip` | El paquete que se registra: versiones 1 y 2, resultados, la descripción formal del método y un manifiesto con la huella de cada archivo |
| `REGISTRO_LEEME.md` | Estas instrucciones |

**Huella digital (SHA-256) del paquete:**

```
0519e2b238a3b99d405cb485129d4699c5a3a5dca52e64872b520eefacf1c42a
```

La huella identifica este archivo exacto: cambiar un solo byte del zip produce otra completamente distinta. **No modifiques ni vuelvas a comprimir el zip**; guárdalo tal cual, con copia en otro disco o en la laptop.

Para comprobar la huella en Windows (PowerShell):

```
Get-FileHash .\metodo_marcos_carlos_2026-09-29.zip -Algorithm SHA256
```

## Paso pendiente (1 minuto, en tu navegador): sello de tiempo en la blockchain de Bitcoin

Desde esta sesión no pude hacerlo, porque la red de la sesión bloquea los servidores de OpenTimestamps. Es gratis y privado: **solo se envía la huella, nunca el contenido**.

1. Abre https://opentimestamps.org en tu navegador.
2. Arrastra `metodo_marcos_carlos_2026-09-29.zip` al recuadro "Stamp". La huella se calcula en tu navegador.
3. Descarga el archivo `metodo_marcos_carlos_2026-09-29.zip.ots` y guárdalo aquí, junto al zip.
4. Unas horas después, cuando Bitcoin lo confirme, vuelve a la página, arrastra el `.ots` y descárgalo de nuevo ya completo ("upgrade").

Con el `.zip` y el `.ots`, cualquiera puede comprobar en el futuro que este paquete **existía en esa fecha**, sin que tengas que revelarlo antes de tiempo.

## Opciones adicionales

- **Correo a ti mismo** con la huella (y el zip adjunto, si quieres). Deja una fecha registrada por los servidores de Google.
- **Registro de derechos de autor en Canadá (CIPO):** opcional; cuesta 63 CAD en línea. En Canadá los derechos de autor nacen solos al crear la obra; el registro solo da un certificado como prueba. **Protege el código escrito, no la idea ni el método.**

## Qué protege esto y qué no

- **Sí:** prueba que tú tenías este método y este código en esta fecha.
- **No:** impide que otros usen la idea. El reconocimiento de un método se obtiene al **publicarlo**. Cuando decidas hacerlo, publica este mismo zip junto con su sello: así la fecha de hoy queda demostrada como anterior a la publicación.

## Versión 4 (registrada el 30 de septiembre de 2026, 09:55 hora de Calgary)

| Archivo | Huella SHA-256 |
|---|---|
| `metodo_marcos_carlos_v4_2026-09-30.zip` | `c05bbda9be35e184f9ee9ddd606d8f9e82ed751d596800cc1581c0fdabc49540` |

- **Contenido:** el motor en C con la ruta unificada (mapas por lado, conteo temprano de piezas, orilla e interior juntos y piezas fijas), los ejecutables, la ventana, los resultados y el manifiesto con la huella de cada archivo.
- **Sello de tiempo:** se hace igual que con las versiones anteriores. Arrastra este zip a https://opentimestamps.org, guarda el `.ots` en esta carpeta y, horas después, haz el "upgrade".
- **Comprobación:** `Get-FileHash .\metodo_marcos_carlos_v4_2026-09-30.zip -Algorithm SHA256`

## Versión 4.1 (registrada el 30 de septiembre de 2026, 10:39 hora de Calgary)

| Archivo | Huella SHA-256 |
|---|---|
| `metodo_marcos_carlos_v41_2026-09-30.zip` | `c3ac611c91d8c386fbd17aa947ebbbf5859b98412ff777b785e16674536fe7f3` |

- **Contenido:** la V4 con el reparto dinámico del trabajo entre los hilos (cola de tareas y cesión de ramas), los ejecutables, la ventana, los resultados, la prueba de completitud (`cmp_grupos.txt`) y el manifiesto con la huella de cada archivo.
- **Sello de tiempo:** arrastra este zip a https://opentimestamps.org y guarda el `.ots` en esta carpeta.
- **Comprobación:** `Get-FileHash .\metodo_marcos_carlos_v41_2026-09-30.zip -Algorithm SHA256`

## Versión 4.2 (registrada el 30 de septiembre de 2026, 13:49 hora de Calgary)

| Archivo | Huella SHA-256 |
|---|---|
| `metodo_marcos_carlos_v42_2026-09-30.zip` | `70f9ca3c43d449edde1ca5d4ef0bd99566b57405066e29f883b9a3b47f756f21` |

- **Contenido:** la versión 4.1 más la idea del autor del "mapa del segundo anillo": el marco interior filtra al exterior, lado por lado. Incluye también la extensión a todas las filas y columnas interiores (`--anillo2`), los ejecutables, la ventana, los resultados, la prueba de completitud (`cmp_grupos.txt`) y el manifiesto con la huella SHA-256 de cada archivo.
- **Sello de tiempo:** arrastra este zip a https://opentimestamps.org y guarda el `.ots` en esta carpeta.
- **Comprobación:** `Get-FileHash .\metodo_marcos_carlos_v42_2026-09-30.zip -Algorithm SHA256`

## Versión 4.3 (registrada el 30 de septiembre de 2026, 23:41 hora de Calgary)

| Archivo | Huella SHA-256 |
|---|---|
| `metodo_marcos_carlos_v43_2026-09-30.zip` | `17121efd29ffd49e3d55536013db6dc8ed4f1ace50ec0b172b30ba2af81090c2` |

- **Contenido:** la versión 4.2 más el avance por grupo (% revisado), el estimado de Knuth del tamaño de cada grupo, el orden automático por grupo ("interior primero" cuando el estimado lo justifica) y el portafolio opcional de equipos de hilos. Incluye los ejecutables, la ventana, los resultados, los registros de las pruebas en la PC del autor, las pruebas de completitud y el manifiesto con la huella SHA-256 de cada archivo.
- **Sello de tiempo:** arrastra este zip a https://opentimestamps.org y guarda el `.ots` en esta carpeta.
- **Comprobación:** `Get-FileHash .\metodo_marcos_carlos_v43_2026-09-30.zip -Algorithm SHA256`

## Versión 4.4 (registrada el 1 de octubre de 2026, 15:29 hora de Calgary)

| Archivo | Huella SHA-256 |
|---|---|
| `metodo_marcos_carlos_v44_2026-10-01.zip` | `5203cea5c54b04613fe96cc86b250b655c1dea23b3b8917042567b44c50d7bd5` |

- **Contenido:** la versión 4.3 más el portafolio automático por grupo (búsqueda normal, pieza menos restrictiva y reinicios al azar en paralelo cuando el grupo es grande) y la corrección de las piezas fijas (las 4 esquinas arriba a la izquierda, observación del autor). Incluye los ejecutables, la ventana, el experimento SAT, los resultados, los registros de las pruebas en la PC del autor, las pruebas de completitud y el manifiesto con la huella SHA-256 de cada archivo.
- **Sello de tiempo:** arrastra este zip a https://opentimestamps.org y guarda el `.ots` en esta carpeta.
- **Comprobación:** `Get-FileHash .\metodo_marcos_carlos_v44_2026-10-01.zip -Algorithm SHA256`

## Versión 4.5 (registrada el 1 de octubre de 2026, 19:53 hora de Calgary)

| Archivo | Huella SHA-256 |
|---|---|
| `metodo_marcos_carlos_v45_2026-10-01.zip` | `3d2324d26deea266a66ac325e0b98162967ecb9ae0287b35e62b2630e7475a84` |

- **Contenido:** la versión 4.4 más las esquinas juntas: las 4 esquinas son parte de la búsqueda y el interior se busca una sola vez para todos los grupos. También incluye los tableros con solución conocida para investigar las piezas fijas, los ejecutables, la ventana, los resultados (con la tabla de evolución de todas las versiones), los registros de las pruebas en la PC del autor, las pruebas de completitud y el manifiesto con la huella SHA-256 de cada archivo.
- **Sello de tiempo:** arrastra este zip a https://opentimestamps.org y guarda el `.ots` en esta carpeta.
- **Comprobación:** `Get-FileHash .\metodo_marcos_carlos_v45_2026-10-01.zip -Algorithm SHA256`

## Versión 4.6 (registrada el 1 de octubre de 2026, 23:15 hora de Calgary)

| Archivo | Huella SHA-256 |
|---|---|
| `metodo_marcos_carlos_v46_2026-10-01.zip` | `4f6eb41935c948e1d1dd45866b408fa27b4e7fe19f9824cc097cebbf41aee940` |

- **Contenido:** la versión 4.5 más los mapas de filas "perezosos" (el doble de nodos por segundo, con el mismo árbol) y las correcciones de la línea de progreso. Incluye los ejecutables, la ventana, los resultados, los registros de las pruebas en la PC del autor, las pruebas de completitud y el manifiesto con la huella SHA-256 de cada archivo.
- **Sello de tiempo:** arrastra este zip a https://opentimestamps.org y guarda el `.ots` en esta carpeta.
- **Comprobación:** `Get-FileHash .\metodo_marcos_carlos_v46_2026-10-01.zip -Algorithm SHA256`

## Versión 4.7 (registrada el 2 de octubre de 2026, 22:18 hora de Calgary)

| Archivo | Huella SHA-256 |
|---|---|
| `metodo_marcos_carlos_v47_2026-10-02.zip` | `2369ee793939fc84e3b9bafda219fae82f9e79c119b6375d131b8a0a12cc877d` |

- **Autor:** Carlos Edison Guzman Marte.
- **Contenido:** la versión 4.6 más las creencias (propagación de creencias) como orden de búsqueda, el generador "tipo oficial", el análisis de la huella del diseñador, la cantidad de marcos y firmas, las creencias por bloques de 2x2, los registros de las pruebas en la PC del autor, el estado del proyecto y el manifiesto con la huella SHA-256 de cada archivo. Ver `DESCRIPCION_VERSION_4_7.md`.
- **Sello de tiempo:** arrastra este zip a https://opentimestamps.org y guarda el `.ots` en esta carpeta.
- **Comprobación:** `Get-FileHash .\metodo_marcos_carlos_v47_2026-10-02.zip -Algorithm SHA256`

## Sistema de uniones, versión 5 (registrado el 2 de octubre de 2026, 22:18 hora de Calgary)

| Archivo | Huella SHA-256 |
|---|---|
| `sistema_uniones_carlos_v5_2026-10-02.zip` | `0f2b9911e7ebf55b6ae95f7c2c38a8ea0148a59dcd012d3e37e5f7f29212f252` |

- **Autor:** Carlos Edison Guzman Marte. Es una idea suya: armar desde un tablero sintético e ir introduciendo las piezas reales.
- **Contenido:** el prototipo en Python, el motor en C (todas las versiones, de la 5.0 a la 5.4), el panel, el cierre exacto, la carrera, el puntaje normal, `creencias.py`, los ejemplos, los registros de las pruebas en la PC del autor y el manifiesto con la huella SHA-256 de cada archivo. Ver `DESCRIPCION_SISTEMA_UNIONES_V5.md`.
- **Sello de tiempo:** arrastra este zip a https://opentimestamps.org y guarda el `.ots` en esta carpeta.
- **Comprobación:** `Get-FileHash .\sistema_uniones_carlos_v5_2026-10-02.zip -Algorithm SHA256`

## Versión 6: la tarjeta de video (registrada el 3 de octubre de 2026, 12:40 hora de Calgary)

| Archivo | Huella SHA-256 |
|---|---|
| `version6_tarjeta_carlos_2026-10-03.zip` | `87b5be96acc87e4d79d01f25a479654113779b252c0695c16aae7fae4b10c20f` |

- **Autor:** Carlos Edison Guzman Marte.
- **Contenido:** el recocido masivo en la tarjeta con cierre exacto en el procesador, la búsqueda exacta repartida en miles de hilos (con prueba de que no hay solución si se agota), la prueba de la poda de la V4.6 en la tarjeta, las creencias vectorizadas, el panel, los registros de las pruebas en la PC del autor y el manifiesto con la huella SHA-256 de cada archivo. Ver `DESCRIPCION_VERSION_6.md`.
- **Sello de tiempo:** arrastra este zip a https://opentimestamps.org y guarda el `.ots` en esta carpeta.
- **Comprobación:** `Get-FileHash .\version6_tarjeta_carlos_2026-10-03.zip -Algorithm SHA256`

**Nota:** los sellos de las versiones 4.4, 4.5 y 4.6 siguen pendientes de completarse en opentimestamps.org.

Todo el trabajo se hizo con ayuda de herramientas de IA. Las ideas, los métodos y las decisiones son del autor.
