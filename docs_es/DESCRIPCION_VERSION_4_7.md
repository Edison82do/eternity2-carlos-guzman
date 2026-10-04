# Método por marcos y firmas (Carlos Edison Guzman Marte) — versión 4.7

**Fecha:** 2 de octubre de 2026, Calgary.

Es la versión 4.6 más las **creencias** (propagación de creencias) como orden de búsqueda.

- **Qué cambia:** `creencias.py` estima qué tan probable es cada pieza, con su giro, en cada casilla. La búsqueda prueba primero las opciones más probables.
  - `--valor 3` usa solo creencias.
  - `--portafolio 6` agrega un equipo de creencias a los otros tres.
  - `--portafolio 7` combina creencias con reinicios guiados por creencias.
- **Resultados en la PC del autor, 9x9 9:3 con 5 pistas, 12 hilos:**
  - suma de los 5 tableros: 150 s con la búsqueda automática (como la V4.6), 33 s con solo creencias;
  - el s1 pasó de 101,6 s a 3,3 s.
- **10x10 con 5 pistas:** sigue sin resolverse en 1 hora.
- **También incluye:**
  - el generador de tableros "tipo oficial" (colores equilibrados, sin piezas repetidas ni simétricas);
  - el análisis de la huella del diseñador (el tablero oficial no muestra otra regularidad aprovechable);
  - la cantidad de marcos y firmas en 9x9 y 10x10;
  - las creencias por bloques de 2x2;
  - las pruebas de creencias recalculadas dentro de la búsqueda;
  - los registros de las pruebas en la PC del autor y el estado del proyecto.

Trabajo hecho con ayuda de herramientas de IA. Las ideas, los métodos y las decisiones son del autor.
