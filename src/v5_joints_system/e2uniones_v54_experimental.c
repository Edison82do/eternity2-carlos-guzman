/*
 * e2uniones.c — Motor en C del SISTEMA DE UNIONES (idea de Carlos, versión 5), con TODOS LOS NÚCLEOS.
 * La lógica de cada tablero es la misma del prototipo e2uniones.py.
 *
 * Representación: se eligen los COLORES DE LAS UNIONES entre casillas; el tablero siempre está armado y
 * la proporción de colores siempre es la del tablero objetivo (los colores solo se intercambian).
 * Una casilla es "real" si su pieza existe en el juego (contando copias). Meta: todas reales = solución.
 * Movimientos: intercambio de dos uniones, e "introducir pieza" (traer los colores de una pieza que falta).
 *
 * Uso de los núcleos (cada hilo tiene SU PROPIO tablero):
 *   --modo independiente  cada hilo hace su propio recocido con otra semilla; gana el primero.
 *   --modo replicas       (por defecto con 4 hilos o más) cada hilo a una temperatura fija (de --tfria a --tcaliente); cada tanto los vecinos
 *                         intercambian temperaturas si conviene (técnica de réplicas / "parallel tempering"):
 *                         los calientes exploran, los fríos afinan.
 *
 * Uso solo texto (lo más rápido):
 *   e2uniones TABLERO.txt [-t hilos] [--modo replicas|independiente] [--tfria 0.15] [--tcaliente 0.8] [--limite S] [--semilla K]
 *                         [--introducir P] [-P F,C,K,G ...] [-o solucion.txt]
 *                         [--cada S]  (una línea de estado cada S segundos; por defecto 2)
 *                         [--parar ARCHIVO]  (si aparece ese archivo, termina y muestra el resultado)
 * Uso con la ventana (ventana_uniones.py lo lanza con --servidor y le da órdenes por la entrada estándar):
 *   N k  todos los hilos avanzan k pasos        A k  avanzar hasta que el hilo mostrado acepte k cambios
 *   I p  probabilidad de "introducir pieza"     Q    salir
 *   respuesta: F pasos aceptados reales mejor temperatura recalentadas hilo_mostrado | colores | tocadas | reales de cada hilo
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <pthread.h>
#include <stdatomic.h>
#ifdef _WIN32
#include <windows.h>
static double ahora(void) { static LARGE_INTEGER f; static int i = 0; LARGE_INTEGER c; if (!i) { QueryPerformanceFrequency(&f); i = 1; } QueryPerformanceCounter(&c); return (double)c.QuadPart / f.QuadPart; }
static int num_cpus(void) { SYSTEM_INFO si; GetSystemInfo(&si); return (int)si.dwNumberOfProcessors; }
#else
#include <time.h>
#include <unistd.h>
static double ahora(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec * 1e-9; }
static int num_cpus(void) { long n = sysconf(_SC_NPROCESSORS_ONLN); return n > 0 ? (int)n : 1; }
#endif

typedef uint32_t u32; typedef uint64_t u64;
static void *xm(size_t n) { void *p = calloc(1, n ? n : 1); if (!p) { fprintf(stderr, "Sin memoria\n"); exit(2); } return p; }

/* ------------------------------------------------------------------ piezas */
typedef struct { int c[4]; } Pz;   /* N E S O */
static Pz rot(Pz p, int k) { for (k &= 3; k > 0; k--) { Pz q = {{p.c[3], p.c[0], p.c[1], p.c[2]}}; p = q; } return p; }
static u32 clave(Pz p) { return (u32)p.c[0] << 24 | (u32)p.c[1] << 16 | (u32)p.c[2] << 8 | (u32)p.c[3]; }
static u32 canon(Pz p) { u32 m = 0xFFFFFFFFu; for (int k = 0; k < 4; k++) { u32 x = clave(rot(p, k)); if (x < m) m = x; } return m; }
static Pz de_clave(u32 x) { Pz p = {{(int)(x >> 24), (int)(x >> 16 & 255), (int)(x >> 8 & 255), (int)(x & 255)}}; return p; }

/* ------------------------------------------------------------------ datos compartidos (solo lectura) */
static int n, NN, NE;
static Pz *pz;
static int (*ec)[4];                   /* unión: casilla1, lado1, casilla2, lado2 */
static int (*lados)[4];                /* casilla: sus 4 uniones (N E S O), -1 = gris */
static int *clase, *fija, *col0;       /* clase (1 marco / 0 resto), fija, colores fijos (-1 = libre) */
static int *libres[2], nlib[2];
static int *celda_fija;
static int NT; static u32 *tclave; static int *tobj;
static int HT; static int *htab;
static int *bolsa0[2], nb0[2];         /* colores libres por clase, para el reparto inicial de cada hilo */
static double p_introducir = 0.3;
static Pz (*trot)[4]; static int *ntrot;
static int MAXCOL; static int *idx_ini, *idx_lst;
static int *par_ini, *par_lst;   /* índice por pares de lados vecinos (d, d+1): (d, c1, c2) -> lista de tipo*4+giro */     /* índice: (lado d, color c) -> lista de (tipo*4+giro) */   /* giros distintos de cada tipo de pieza (para el cierre exacto) */
static int cierre_k = -1;                  /* intentar el cierre cuando falten <= cierre_k piezas (-1 = automático) */
static long long cierre_nodos = 5000000;   /* nodos por intento de cierre */
static int cierre_rmax = 2;                /* radio máximo de la zona que se rehace */
static int ventana_cada = 0;               /* cada cuántos tramos de 16384 pasos se prueba una ventana exacta (0 = nunca) */
static int ventana_r = 1;                  /* radio de la ventana (1 = 3x3, 2 = 5x5) */
static long long ventana_nodos = 20000;
static int ventana_libre = 0;              /* 1 = no exigir que se conserven los colores internos */
static double cierre_frac = 1.0;
static int usar_orden_fijo = 0;
static int mrv_total = 0;
static int usar_fc = 0;
static double cierre_azar = 0.0;
static double precio_eta = 0.0;            /* Lagrange: cuánto suben los precios de tipos que sobran/faltan en cada atasco */
static double gls_alfa = 0.0;              /* búsqueda guiada: castigo por repetir (casilla, tipo) de atascos anteriores */
static int atasco_pasos = 100000;
static float *prob_orden = NULL;           /* [NN][NT*4]: probabilidad de cada pieza con giro (creencias), para el orden de prueba */
static int solo_exacto = 0;                /* 1 = búsqueda exacta de todo el tablero, sin recocido (para medir órdenes) */          /* pasos sin mejorar que cuentan como atasco */           /* además, liberar al azar esta fracción de las demás casillas */                    /* 1 = tras poner una pieza, comprobar que sus vecinas libres aún tienen candidatas */                  /* 1 = contar candidatas en todas las casillas de la zona, no solo las más conocidas */            /* búsqueda exacta con orden fijo (rápida) o eligiendo casilla en cada nodo */           /* tope: nodos de búsqueda exacta <= cierre_frac * pasos del recocido */

static int buscar_tipo(u32 k) { u32 h = (k * 2654435761u) & (HT - 1); while (htab[h] >= 0) { if (tclave[htab[h]] == k) return htab[h]; h = (h + 1) & (HT - 1); } return -1; }

/* ------------------------------------------------------------------ estado de cada hilo (su propio tablero) */
#define MAXC 16
typedef struct {
    int id; u64 rs;
    int *col, *tipo, *tcnt;
    int reales, mejor; long long pasos, aceptados; int *mcol;   /* mcol: colores del mejor tablero que tuvo este hilo */
    double T, T0, Tmin, enfriar; long long paciencia, sin_mejora; int ult_mejor, recal;
    int fija_T;                        /* réplicas: la temperatura no baja sola */
    int cu[MAXC], cn[MAXC], cv[MAXC], ncamb, toc[2 * MAXC], ntoc;
    int ult_toc[2 * MAXC], nult;
    int *inv, *falt, *opi, *cand; Pz *opq;
    long long objetivo_pasos, objetivo_acept;
    /* cierre exacto (híbrido con búsqueda exacta) */
    int *enR, *puesto, *avail, *celR, nR, *ordR; Pz *pq; long long nodos, limite_nodos; int abortado;
    u64 ult_firma; long long cierres_intentos, cierres_exitos, cierres_nodos;
    double *lam, *mu, dw; unsigned short *pen; long long atascos, ult_mejora_paso; int mejor_local;
    int en_ventana, *cnt_int, *cnt_new, *ecol_bk; long long vent_intentos, vent_exitos, vent_mejoras;
} Est;

static inline u64 azar(Est *s) { u64 z = (s->rs += 0x9E3779B97F4A7C15ULL); z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL; z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL; return z ^ (z >> 31); }
static inline int azar_n(Est *s, int m) { return (int)(azar(s) % (u64)m); }
static inline double azar01(Est *s) { return (azar(s) >> 11) * (1.0 / 9007199254740992.0); }

static inline Pz pieza(const Est *s, int rc) { Pz p; for (int d = 0; d < 4; d++) { int e = lados[rc][d]; p.c[d] = e < 0 ? 0 : s->col[e]; } return p; }
static inline int es_real(const Est *s, int rc) { int t = s->tipo[rc]; return t >= 0 && s->tcnt[t] <= tobj[t]; }
static inline int quitar(Est *s, int t) { if (t < 0) return 0; int d = s->tcnt[t] <= tobj[t] ? -1 : 0;
    s->dw += d ? -(1.0 + s->mu[t]) : s->lam[t]; s->tcnt[t]--; return d; }
static inline int poner(Est *s, int t) { if (t < 0) return 0; int d = s->tcnt[t] < tobj[t] ? 1 : 0;
    s->dw += d ? (1.0 + s->mu[t]) : -s->lam[t]; s->tcnt[t]++; return d; }
static inline double castigo(Est *s, int rc) { int t = s->tipo[rc]; return (t >= 0 && s->pen) ? gls_alfa * s->pen[(size_t)rc * NT + t] : 0.0; }

/* ------------------------------------------------------------------ lectura y preparación */
static void leer(const char *ruta) {
    FILE *f = fopen(ruta, "r"); if (!f) { fprintf(stderr, "No puedo abrir %s\n", ruta); exit(1); }
    int cap = 4096, m = 0; int *v = xm(cap * sizeof(int)); char lin[16384];
    while (fgets(lin, sizeof lin, f)) {
        char *p = lin; while (*p == ' ' || *p == '\t') p++; if (*p == '#') continue;
        char *fin; long x; while ((x = strtol(p, &fin, 10)), fin != p) { if (m == cap) { cap *= 2; v = realloc(v, cap * sizeof(int)); } v[m++] = (int)x; p = fin; }
    }
    fclose(f);
    int np = m / 4; n = 0; while ((n + 1) * (n + 1) <= np) n++;
    if (n * n != np || m % 4) { fprintf(stderr, "El archivo no tiene un tablero cuadrado\n"); exit(1); }
    NN = n * n; pz = xm(NN * sizeof(Pz));
    for (int i = 0; i < NN; i++) for (int d = 0; d < 4; d++) pz[i].c[d] = v[4 * i + d];
    free(v);
}

typedef struct { int r, c, k, g; } Pista;

static void preparar(Pista *ps, int nps) {
    NE = 2 * n * (n - 1);
    ec = xm(NE * sizeof(*ec)); lados = xm(NN * sizeof(*lados)); clase = xm(NE * sizeof(int)); fija = xm(NE * sizeof(int)); col0 = xm(NE * sizeof(int));
    int e = 0; int *H = xm(NN * sizeof(int)), *V = xm(NN * sizeof(int));
    for (int r = 0; r < n; r++) for (int c = 0; c < n - 1; c++) { H[r * n + c] = e; ec[e][0] = r * n + c; ec[e][1] = 1; ec[e][2] = r * n + c + 1; ec[e][3] = 3; e++; }
    for (int r = 0; r < n - 1; r++) for (int c = 0; c < n; c++) { V[r * n + c] = e; ec[e][0] = r * n + c; ec[e][1] = 2; ec[e][2] = (r + 1) * n + c; ec[e][3] = 0; e++; }
    for (int r = 0; r < n; r++) for (int c = 0; c < n; c++) {
        int k = r * n + c;
        lados[k][0] = r > 0 ? V[(r - 1) * n + c] : -1; lados[k][1] = c < n - 1 ? H[k] : -1;
        lados[k][2] = r < n - 1 ? V[k] : -1;           lados[k][3] = c > 0 ? H[k - 1] : -1;
    }
    for (int i = 0; i < NE; i++) {
        int a = ec[i][0], b = ec[i][2], ra = a / n, ca = a % n, rb = b / n, cb = b % n;
        clase[i] = (ra == rb && (ra == 0 || ra == n - 1)) || (ca == cb && (ca == 0 || ca == n - 1));
    }
    tclave = xm(NN * sizeof(u32)); tobj = xm(NN * sizeof(int)); NT = 0;
    HT = 1; while (HT < 4 * NN) HT <<= 1; htab = xm(HT * sizeof(int)); for (int i = 0; i < HT; i++) htab[i] = -1;
    for (int i = 0; i < NN; i++) {
        u32 k = canon(pz[i]); int t = buscar_tipo(k);
        if (t < 0) { t = NT++; tclave[t] = k; u32 h = (k * 2654435761u) & (HT - 1); while (htab[h] >= 0) h = (h + 1) & (HT - 1); htab[h] = t; }
        tobj[t]++;
    }
    trot = xm(NT * sizeof(*trot)); ntrot = xm(NT * sizeof(int));
    for (int t = 0; t < NT; t++) {
        Pz b = de_clave(tclave[t]);
        for (int g = 0; g < 4; g++) { Pz q = rot(b, g); int dup = 0; for (int j = 0; j < ntrot[t]; j++) if (clave(trot[t][j]) == clave(q)) dup = 1; if (!dup) trot[t][ntrot[t]++] = q; }
    }
    int maxc = 0; for (int i = 0; i < NN; i++) for (int d = 0; d < 4; d++) if (pz[i].c[d] > maxc) maxc = pz[i].c[d];
    MAXCOL = maxc + 1;
    idx_ini = xm((4 * MAXCOL + 1) * sizeof(int)); idx_lst = xm((4 * NT * 4 + 1) * sizeof(int));
    for (int d = 0, m = 0; d < 4; d++) for (int c = 0; c < MAXCOL; c++) {
        idx_ini[d * MAXCOL + c] = m;
        for (int t = 0; t < NT; t++) for (int j = 0; j < ntrot[t]; j++) if (trot[t][j].c[d] == c) idx_lst[m++] = t * 4 + j;
        idx_ini[d * MAXCOL + c + 1] = m;
    }
    par_ini = xm((4 * MAXCOL * MAXCOL + 1) * sizeof(int)); par_lst = xm((4 * NT * 4 + 1) * sizeof(int));
    for (int d = 0, m = 0; d < 4; d++) for (int a = 0; a < MAXCOL; a++) for (int b = 0; b < MAXCOL; b++) {
        par_ini[(d * MAXCOL + a) * MAXCOL + b] = m;
        for (int t = 0; t < NT; t++) for (int j = 0; j < ntrot[t]; j++) if (trot[t][j].c[d] == a && trot[t][j].c[(d + 1) & 3] == b) par_lst[m++] = t * 4 + j;
        par_ini[(d * MAXCOL + a) * MAXCOL + b + 1] = m;
    }
    int *med[2]; med[0] = xm((maxc + 1) * sizeof(int)); med[1] = xm((maxc + 1) * sizeof(int));
    for (int i = 0; i < NN; i++) {
        int g = 0; for (int d = 0; d < 4; d++) g += pz[i].c[d] == 0;
        for (int d = 0; d < 4; d++) {
            if (pz[i].c[d] == 0) continue;
            int k = (g >= 1 && (pz[i].c[(d + 1) & 3] == 0 || pz[i].c[(d + 3) & 3] == 0)) ? 1 : 0;
            med[k][pz[i].c[d]]++;
        }
    }
    bolsa0[0] = xm(NE * sizeof(int)); bolsa0[1] = xm(NE * sizeof(int)); nb0[0] = nb0[1] = 0;
    int cuantas[2] = {0, 0}; for (int i = 0; i < NE; i++) cuantas[clase[i]]++;
    for (int k = 0; k < 2; k++) for (int c = 1; c <= maxc; c++) {
        if (med[k][c] % 2) { fprintf(stderr, "El color %d aparece un número impar de veces\n", c); exit(1); }
        for (int j = 0; j < med[k][c] / 2; j++) { if (nb0[k] >= NE) { fprintf(stderr, "Demasiados colores\n"); exit(1); } bolsa0[k][nb0[k]++] = c; }
    }
    if (nb0[0] != cuantas[0] || nb0[1] != cuantas[1]) { fprintf(stderr, "Los colores no alcanzan para las uniones (%d/%d, %d/%d)\n", nb0[0], cuantas[0], nb0[1], cuantas[1]); exit(1); }
    for (int i = 0; i < NE; i++) col0[i] = -1;
    celda_fija = xm(NN * sizeof(int));
    Pista esq;
    if (nps == 0) {   /* regla de Carlos: sin pistas, se fija UNA esquina arriba a la izquierda */
        for (int k = 0; k < NN && !nps; k++) {
            int g = 0; for (int d = 0; d < 4; d++) g += pz[k].c[d] == 0;
            if (g != 2) continue;
            for (int r = 0; r < 4; r++) { Pz q = rot(pz[k], r); if (q.c[0] == 0 && q.c[3] == 0) { esq.r = 0; esq.c = 0; esq.k = k; esq.g = r; ps = &esq; nps = 1; break; } }
        }
    }
    for (int i = 0; i < nps; i++) {
        int rc = ps[i].r * n + ps[i].c; Pz q = rot(pz[ps[i].k], ps[i].g); celda_fija[rc] = 1;
        for (int d = 0; d < 4; d++) {
            int u = lados[rc][d];
            if (u < 0) { if (q.c[d] != 0) { fprintf(stderr, "La pieza fija en (%d,%d) tiene color hacia afuera\n", ps[i].r + 1, ps[i].c + 1); exit(1); } continue; }
            if (col0[u] >= 0) { if (col0[u] != q.c[d]) { fprintf(stderr, "Dos piezas fijas no encajan entre sí\n"); exit(1); } continue; }
            col0[u] = q.c[d]; fija[u] = 1;
            int k = clase[u], j; for (j = 0; j < nb0[k]; j++) if (bolsa0[k][j] == q.c[d]) break;
            if (j == nb0[k]) { fprintf(stderr, "La pieza fija usa un color que no está disponible\n"); exit(1); }
            bolsa0[k][j] = bolsa0[k][--nb0[k]];
        }
    }
    for (int k = 0; k < 2; k++) { libres[k] = xm(NE * sizeof(int)); nlib[k] = 0; for (int i = 0; i < NE; i++) if (clase[i] == k && !fija[i]) libres[k][nlib[k]++] = i; }
    free(H); free(V); free(med[0]); free(med[1]);
}

/* cada hilo arma su propio tablero inicial al azar */
static void iniciar_estado(Est *s, int id, long long semilla) {
    memset(s, 0, sizeof(*s)); s->id = id;
    s->rs = 0x9E3779B97F4A7C15ULL * (u64)(semilla + 1) + 0xD1B54A32D192ED03ULL * (u64)(id + 1);
    s->mcol = xm(NE * sizeof(int)); s->col = xm(NE * sizeof(int)); s->tipo = xm(NN * sizeof(int)); s->tcnt = xm(NT * sizeof(int));
    s->inv = xm(NN * sizeof(int)); s->falt = xm(NN * sizeof(int)); s->opq = xm(4 * NN * sizeof(Pz)); s->opi = xm(4 * NN * sizeof(int)); s->cand = xm(NE * sizeof(int));
    s->enR = xm(NN * sizeof(int)); s->puesto = xm(NN * sizeof(int)); s->avail = xm(NT * sizeof(int)); s->celR = xm(NN * sizeof(int)); s->ordR = xm(NN * sizeof(int)); s->pq = xm(NN * sizeof(Pz)); s->lam = xm(NT * sizeof(double)); s->mu = xm(NT * sizeof(double));
    if (gls_alfa > 0) s->pen = xm((size_t)NN * NT * sizeof(unsigned short)); s->cnt_int = xm(256 * sizeof(int)); s->cnt_new = xm(256 * sizeof(int)); s->ecol_bk = xm(NE * sizeof(int));
    memcpy(s->col, col0, NE * sizeof(int));
    for (int k = 0; k < 2; k++) {
        int *b = xm(NE * sizeof(int)); memcpy(b, bolsa0[k], nb0[k] * sizeof(int));
        for (int j = nb0[k] - 1; j > 0; j--) { int r = azar_n(s, j + 1); int t = b[j]; b[j] = b[r]; b[r] = t; }
        int j = 0; for (int i = 0; i < NE; i++) if (clase[i] == k && s->col[i] < 0) s->col[i] = b[j++];
        free(b);
    }
    for (int rc = 0; rc < NN; rc++) { s->tipo[rc] = buscar_tipo(canon(pieza(s, rc))); if (s->tipo[rc] >= 0) s->tcnt[s->tipo[rc]]++; }
    s->reales = 0; for (int t = 0; t < NT; t++) s->reales += s->tcnt[t] < tobj[t] ? s->tcnt[t] : tobj[t];
    s->mejor = s->reales; s->ult_mejor = -1; memcpy(s->mcol, s->col, NE * sizeof(int));
    s->T = s->T0 = 2.0; s->Tmin = 0.05; s->enfriar = 0.99995; s->paciencia = 200000;
}

/* ------------------------------------------------------------------ aplicar / deshacer */
static int aplicar(Est *s) {
    s->ntoc = 0;
    for (int i = 0; i < s->ncamb; i++) for (int q = 0; q < 3; q += 2) {
        int rc = ec[s->cu[i]][q], j; for (j = 0; j < s->ntoc; j++) if (s->toc[j] == rc) break;
        if (j == s->ntoc) s->toc[s->ntoc++] = rc;
    }
    int delta = 0; s->dw = 0;
    if (s->pen) for (int j = 0; j < s->ntoc; j++) s->dw += castigo(s, s->toc[j]);
    for (int j = 0; j < s->ntoc; j++) delta += quitar(s, s->tipo[s->toc[j]]);
    for (int i = 0; i < s->ncamb; i++) { s->cv[i] = s->col[s->cu[i]]; s->col[s->cu[i]] = s->cn[i]; }
    for (int j = 0; j < s->ntoc; j++) { int t = buscar_tipo(canon(pieza(s, s->toc[j]))); s->tipo[s->toc[j]] = t; delta += poner(s, t); }
    if (s->pen) for (int j = 0; j < s->ntoc; j++) s->dw -= castigo(s, s->toc[j]);
    return delta;
}
static void deshacer(Est *s) {
    for (int j = 0; j < s->ntoc; j++) quitar(s, s->tipo[s->toc[j]]);
    for (int i = s->ncamb - 1; i >= 0; i--) s->col[s->cu[i]] = s->cv[i];
    for (int j = 0; j < s->ntoc; j++) { int t = buscar_tipo(canon(pieza(s, s->toc[j]))); s->tipo[s->toc[j]] = t; poner(s, t); }
}

/* ------------------------------------------------------------------ movimientos (iguales al prototipo) */
static int mov_intercambio(Est *s) {
    int k = (azar01(s) < 0.85 || nlib[1] == 0) ? 0 : 1; int *L = libres[k], nl = nlib[k];
    if (nl < 2) return 0;
    int e1 = L[azar_n(s, nl)];
    for (int i = 0; i < 6; i++) { if (!(es_real(s, ec[e1][0]) && es_real(s, ec[e1][2]))) break; e1 = L[azar_n(s, nl)]; }
    int e2 = L[azar_n(s, nl)];
    if (s->col[e1] == s->col[e2]) return 0;
    s->ncamb = 2; s->cu[0] = e1; s->cn[0] = s->col[e2]; s->cu[1] = e2; s->cn[1] = s->col[e1];
    return 1;
}

static int mov_introducir(Est *s) {
    int ni = 0; for (int rc = 0; rc < NN; rc++) if (!celda_fija[rc] && !es_real(s, rc)) s->inv[ni++] = rc;
    if (!ni) return 0;
    int rc = s->inv[azar_n(s, ni)];
    int nf = 0; for (int t = 0; t < NT; t++) if (s->tcnt[t] < tobj[t]) s->falt[nf++] = t;
    if (!nf) return 0;
    int no = 0, mx = -1;
    for (int i = 0; i < nf; i++) {
        Pz base = de_clave(tclave[s->falt[i]]);
        for (int g = 0; g < 4; g++) {
            Pz q = rot(base, g); int ok = 1, ig = 0;
            for (int d = 0; d < 4 && ok; d++) {
                int e = lados[rc][d];
                if ((e < 0) != (q.c[d] == 0)) { ok = 0; break; }
                if (e >= 0) { if (fija[e] && s->col[e] != q.c[d]) { ok = 0; break; } if (s->col[e] == q.c[d]) ig++; }
            }
            if (ok) { s->opq[no] = q; s->opi[no] = ig; no++; if (ig > mx) mx = ig; }
        }
    }
    if (!no) return 0;
    int nbuen = 0; for (int i = 0; i < no; i++) if (s->opi[i] >= mx - 1) s->opq[nbuen++] = s->opq[i];
    Pz q = s->opq[azar_n(s, nbuen)];
    s->ncamb = 0; int usados[8], nus = 0;
    for (int d = 0; d < 4; d++) if (lados[rc][d] >= 0) usados[nus++] = lados[rc][d];
    for (int d = 0; d < 4; d++) {
        int e = lados[rc][d]; if (e < 0) continue;
        int ce = s->col[e]; for (int i = 0; i < s->ncamb; i++) if (s->cu[i] == e) ce = s->cn[i];
        if (ce == q.c[d]) continue;
        int k = clase[e], nc = 0, mejor_r = 3;
        for (int j = 0; j < nlib[k]; j++) {
            int f = libres[k][j]; int cf = s->col[f]; for (int i = 0; i < s->ncamb; i++) if (s->cu[i] == f) cf = s->cn[i];
            if (cf != q.c[d]) continue;
            int u; for (u = 0; u < nus; u++) if (usados[u] == f) break;
            if (u < nus) continue;
            int r = es_real(s, ec[f][0]) + es_real(s, ec[f][2]);
            if (r < mejor_r) { mejor_r = r; nc = 0; }
            if (r == mejor_r) s->cand[nc++] = f;
        }
        if (!nc) return 0;
        int f = s->cand[azar_n(s, nc)]; usados[nus++] = f;
        int i;
        for (i = 0; i < s->ncamb; i++) if (s->cu[i] == e) break;
        if (i == s->ncamb) s->cu[s->ncamb++] = e;
        s->cn[i] = q.c[d];
        for (i = 0; i < s->ncamb; i++) if (s->cu[i] == f) break;
        if (i == s->ncamb) s->cu[s->ncamb++] = f;
        s->cn[i] = ce;
        if (s->ncamb > MAXC - 2) return 0;
    }
    return s->ncamb > 0;
}

static int paso(Est *s) {
    s->pasos++;
    int ok = azar01(s) < p_introducir ? mov_introducir(s) : mov_intercambio(s);
    int acept = 0;
    if (ok) {
        int d = aplicar(s); double dw = (precio_eta > 0 || gls_alfa > 0) ? s->dw : (double)d;
        if (dw >= 0 || azar01(s) < exp(dw / (s->T > 1e-9 ? s->T : 1e-9))) {
            s->reales += d; s->aceptados++; acept = 1; if (s->reales > s->mejor) { s->mejor = s->reales; memcpy(s->mcol, s->col, NE * sizeof(int)); }
            s->nult = s->ntoc; memcpy(s->ult_toc, s->toc, s->ntoc * sizeof(int));
        } else deshacer(s);
    }
    if (precio_eta > 0 || gls_alfa > 0) {          /* atasco: subir precios / castigar lo que hay */
        if (s->reales > s->mejor_local) { s->mejor_local = s->reales; s->ult_mejora_paso = s->pasos; }
        else if (s->pasos - s->ult_mejora_paso > atasco_pasos) {
            s->atascos++; s->ult_mejora_paso = s->pasos; s->mejor_local = s->reales;
            if (precio_eta > 0) for (int t = 0; t < NT; t++) {
                s->lam[t] *= 0.98; s->mu[t] *= 0.98;
                if (s->tcnt[t] > tobj[t]) s->lam[t] += precio_eta * (s->tcnt[t] - tobj[t]);
                if (s->tcnt[t] < tobj[t]) s->mu[t] += precio_eta * (tobj[t] - s->tcnt[t]);
            }
            if (s->pen) for (int rc = 0; rc < NN; rc++) if (es_real(s, rc)) { size_t k = (size_t)rc * NT + s->tipo[rc]; if (s->pen[k] < 60000) s->pen[k]++; }
        }
    }
    if (!s->fija_T) {
        if (s->mejor > s->ult_mejor) { s->ult_mejor = s->mejor; s->sin_mejora = 0; } else s->sin_mejora++;
        s->T *= s->enfriar; if (s->T < s->Tmin) s->T = s->Tmin;
        if (s->sin_mejora > s->paciencia) { s->T = s->T0 * 0.6; s->sin_mejora = 0; s->recal++; }
    }
    return acept;
}

/* ------------------------------------------------------------------ cierre exacto (mezcla de los dos sistemas)
 * Cuando el tablero está casi armado, se rehace EXACTAMENTE una zona: las casillas inventadas (o dudosas) y sus
 * vecinas hasta un radio r. Las casillas reales de afuera quedan quietas y sus colores son restricciones; la zona
 * se llena con las piezas que faltan, con una búsqueda por retroceso (como el método por marcos) y un límite de nodos.
 * Si la zona se llena entera, el tablero queda resuelto. */
static void recalcular(Est *s) {
    memset(s->tcnt, 0, NT * sizeof(int));
    for (int rc = 0; rc < NN; rc++) { s->tipo[rc] = buscar_tipo(canon(pieza(s, rc))); if (s->tipo[rc] >= 0) s->tcnt[s->tipo[rc]]++; }
    s->reales = 0; for (int t = 0; t < NT; t++) s->reales += s->tcnt[t] < tobj[t] ? s->tcnt[t] : tobj[t];
    if (s->reales > s->mejor) { s->mejor = s->reales; memcpy(s->mcol, s->col, NE * sizeof(int)); }
}
static inline int req_lado(Est *s, int rc, int d) {      /* color exigido en el lado d de rc, o -1 si está libre */
    int e = lados[rc][d]; if (e < 0) return 0;
    int o = ec[e][0] == rc ? ec[e][2] : ec[e][0];
    if (!s->enR[o]) return s->col[e];
    return s->puesto[o] ? s->pq[o].c[(d + 2) & 3] : -1;
}
/* lista más corta de candidatas según los lados conocidos (todos los lados de borde exigen 0, siempre hay alguno) */
static inline void lista_cand(const int *rq, const int **a, const int **b) {
    int best = -1, bl = 1 << 30;
    for (int d = 0; d < 4; d++) if (rq[d] >= 0 && rq[d] < MAXCOL) { int l = idx_ini[d * MAXCOL + rq[d] + 1] - idx_ini[d * MAXCOL + rq[d]]; if (l < bl) { bl = l; best = d; } }
    if (best < 0) { *a = idx_lst; *b = idx_lst + idx_ini[MAXCOL]; return; }     /* sin lados conocidos: lado N, todos los colores */
    *a = idx_lst + idx_ini[best * MAXCOL + rq[best]]; *b = idx_lst + idx_ini[best * MAXCOL + rq[best] + 1];
}
static inline int encaja(Pz q, const int *rq) {
    return (rq[0] < 0 || q.c[0] == rq[0]) && (rq[1] < 0 || q.c[1] == rq[1]) && (rq[2] < 0 || q.c[2] == rq[2]) && (rq[3] < 0 || q.c[3] == rq[3]);
}
static int contar_cand(Est *s, int rc, const int *rq, int tope) {
    (void)rc; const int *a, *b; int c = 0; lista_cand(rq, &a, &b);
    if (rq[0] < 0 && rq[1] < 0 && rq[2] < 0 && rq[3] < 0) { for (int t = 0; t < NT; t++) if (s->avail[t] > 0) c += ntrot[t]; return c; }
    for (; a < b; a++) { int t = *a >> 2; if (s->avail[t] > 0 && encaja(trot[t][*a & 3], rq)) if (++c >= tope) return c; }
    return c;
}
static int hoja_ventana(Est *s) {   /* las uniones internas de la ventana deben conservar los mismos colores (proporción) */
    memset(s->cnt_new, 0, MAXCOL * sizeof(int));
    for (int i = 0; i < s->nR; i++) { int rc = s->celR[i];
        for (int d = 1; d <= 2; d++) { int e = lados[rc][d]; if (e < 0) continue; int o = ec[e][2]; if (s->enR[o]) s->cnt_new[s->pq[rc].c[d]]++; } }
    return !memcmp(s->cnt_new, s->cnt_int, MAXCOL * sizeof(int));
}
static int dfs_cierre(Est *s, int quedan) {
    if (quedan == 0) return (s->en_ventana && !ventana_libre) ? hoja_ventana(s) : 1;
    if (++s->nodos > s->limite_nodos) { s->abortado = 1; return 0; }
    /* casilla con más lados conocidos; entre ellas, la de menos candidatas */
    int mejor = -1, mk = -1, mc = 1 << 30, rq[4], brq[4];
    for (int i = 0; i < s->nR; i++) {
        int rc = s->celR[i]; if (s->puesto[rc]) continue;
        int k = 0; for (int d = 0; d < 4; d++) { rq[d] = req_lado(s, rc, d); k += rq[d] >= 0; }
        if (!mrv_total && k < mk) continue;
        if (mrv_total && k == 0) continue;
        int c = contar_cand(s, rc, rq, (mrv_total || k == mk) ? mc : 1 << 30);
        if (mrv_total ? (c < mc || (c == mc && k > mk)) : (k > mk || c < mc)) { mk = k; mc = c; mejor = rc; memcpy(brq, rq, sizeof rq); }
        if (mc == 0) return 0;
    }
    if (mejor < 0) { for (int i = 0; i < s->nR; i++) if (!s->puesto[s->celR[i]]) { mejor = s->celR[i]; for (int d = 0; d < 4; d++) brq[d] = req_lado(s, mejor, d); break; } }
    int rc = mejor;
    const int *a, *b;
    lista_cand(brq, &a, &b);
    int lst[1024]; float pr[1024]; int nl = 0;
    for (; a < b && nl < 1024; a++) { int t = *a >> 2; if (s->avail[t] <= 0) continue; if (!encaja(trot[t][*a & 3], brq)) continue;
        lst[nl] = *a; pr[nl] = prob_orden ? prob_orden[(size_t)rc * NT * 4 + *a] : 0; nl++; }
    if (prob_orden) for (int i = 1; i < nl; i++) { int x = lst[i]; float y = pr[i]; int j = i - 1; while (j >= 0 && pr[j] < y) { lst[j + 1] = lst[j]; pr[j + 1] = pr[j]; j--; } lst[j + 1] = x; pr[j + 1] = y; }
    for (int ii = 0; ii < nl; ii++) {
        int t = lst[ii] >> 2;
        Pz q = trot[t][lst[ii] & 3];
        s->puesto[rc] = 1; s->pq[rc] = q; s->avail[t]--;
        if (usar_fc) {
            int vivo = 1;
            for (int d = 0; d < 4 && vivo; d++) { int e = lados[rc][d]; if (e < 0) continue; int o = ec[e][0] == rc ? ec[e][2] : ec[e][0];
                if (!s->enR[o] || s->puesto[o]) continue; int r2[4]; for (int d2 = 0; d2 < 4; d2++) r2[d2] = req_lado(s, o, d2);
                if (contar_cand(s, o, r2, 1) == 0) vivo = 0; }
            if (!vivo) { s->puesto[rc] = 0; s->avail[t]++; continue; }
        }
        if (dfs_cierre(s, quedan - 1)) return 1;
        s->puesto[rc] = 0; s->avail[t]++;
        if (s->abortado) return 0;
    }
    return 0;
}
/* orden fijo para la zona: cada vez, la casilla con más lados ya conocidos (borde, afuera o ya ordenada) */
static void ordenar_zona(Est *s) {
    for (int i = 0; i < s->nR; i++) s->puesto[s->celR[i]] = 0;
    for (int k = 0; k < s->nR; k++) {
        int best = -1, bk = -1;
        for (int i = 0; i < s->nR; i++) {
            int rc = s->celR[i]; if (s->puesto[rc]) continue;
            int kn = 0;
            for (int d = 0; d < 4; d++) { int e = lados[rc][d]; if (e < 0) { kn++; continue; } int o = ec[e][0] == rc ? ec[e][2] : ec[e][0]; if (!s->enR[o] || s->puesto[o]) kn++; }
            if (kn > bk) { bk = kn; best = rc; }
        }
        s->ordR[k] = best; s->puesto[best] = 1;
    }
    for (int i = 0; i < s->nR; i++) s->puesto[s->celR[i]] = 0;
}
static int dfs_orden(Est *s, int k) {
    if (k == s->nR) return (s->en_ventana && !ventana_libre) ? hoja_ventana(s) : 1;
    if (++s->nodos > s->limite_nodos) { s->abortado = 1; return 0; }
    int rc = s->ordR[k], rq[4];
    for (int d = 0; d < 4; d++) rq[d] = req_lado(s, rc, d);
    /* lista más corta: un par de lados vecinos conocidos, o si no, un lado */
    const int *a = NULL, *b = NULL; int bl = 1 << 30;
    for (int d = 0; d < 4; d++) { int d2 = (d + 1) & 3;
        if (rq[d] >= 0 && rq[d2] >= 0 && rq[d] < MAXCOL && rq[d2] < MAXCOL) { int x = (d * MAXCOL + rq[d]) * MAXCOL + rq[d2]; int l = par_ini[x + 1] - par_ini[x]; if (l < bl) { bl = l; a = par_lst + par_ini[x]; b = par_lst + par_ini[x + 1]; } } }
    if (!a) lista_cand(rq, &a, &b);
    for (; a < b; a++) {
        int t = *a >> 2; if (s->avail[t] <= 0) continue;
        Pz q = trot[t][*a & 3]; if (!encaja(q, rq)) continue;
        s->puesto[rc] = 1; s->pq[rc] = q; s->avail[t]--;
        if (dfs_orden(s, k + 1)) return 1;
        s->puesto[rc] = 0; s->avail[t]++;
        if (s->abortado) return 0;
    }
    return 0;
}
static int buscar_zona(Est *s) { if (usar_orden_fijo) { ordenar_zona(s); return dfs_orden(s, 0); } return dfs_cierre(s, s->nR); }

/* rehace la zona: dudosas + vecinas hasta radio r. Devuelve 1 si el tablero quedó resuelto. */
static int intentar_cierre_r(Est *s, int r) {
    memset(s->enR, 0, NN * sizeof(int)); s->nR = 0;
    for (int rc = 0; rc < NN; rc++) {
        if (es_real(s, rc)) continue;
        int r0 = rc / n, c0 = rc % n;
        for (int dr = -r; dr <= r; dr++) for (int dc = -r; dc <= r; dc++) {
            int rr = r0 + dr, cc = c0 + dc; if (rr < 0 || rr >= n || cc < 0 || cc >= n) continue;
            int x = rr * n + cc; if (celda_fija[x] || s->enR[x]) continue;
            s->enR[x] = 1; s->celR[s->nR++] = x;
        }
    }
    if (s->nR == 0) return 0;
    if (cierre_azar > 0) for (int rc = 0; rc < NN; rc++) if (!s->enR[rc] && !celda_fija[rc] && azar01(s) < cierre_azar) { s->enR[rc] = 1; s->celR[s->nR++] = rc; }
    memcpy(s->avail, tobj, NT * sizeof(int));
    for (int rc = 0; rc < NN; rc++) if (!s->enR[rc]) {
        int t = s->tipo[rc]; if (t < 0 || --s->avail[t] < 0) return 0;     /* afuera hay algo que no es pieza: no se puede */
    }
    memset(s->puesto, 0, NN * sizeof(int));
    s->nodos = 0; s->abortado = 0; s->limite_nodos = cierre_nodos;
    int ok = buscar_zona(s);
    s->cierres_intentos++; s->cierres_nodos += s->nodos;
    if (!ok) return 0;
    for (int i = 0; i < s->nR; i++) {
        int rc = s->celR[i];
        for (int d = 0; d < 4; d++) { int e = lados[rc][d]; if (e >= 0) s->col[e] = s->pq[rc].c[d]; }
    }
    recalcular(s);
    if (s->reales == NN) { s->cierres_exitos++; s->nult = 0; for (int i = 0; i < s->nR && s->nult < 2 * MAXC; i++) s->ult_toc[s->nult++] = s->celR[i]; return 1; }
    return 0;
}
static void intentar_cierre(Est *s) {
    int faltan = NN - s->reales, k = cierre_k >= 0 ? cierre_k : (NN / 4 > 3 ? NN / 4 : 3);
    if (faltan > k || faltan <= 0) return;
    if (s->cierres_nodos > cierre_frac * (double)s->pasos) return;   /* no dejar que la búsqueda exacta se coma todo el tiempo */
    u64 f = 1469598103934665603ULL;     /* no repetir el intento sobre el mismo tablero */
    for (int i = 0; i < NE; i++) f = (f ^ (u64)s->col[i]) * 1099511628211ULL;
    if (f == s->ult_firma) return;
    s->ult_firma = f;
    for (int r = 1; r <= cierre_rmax; r++) if (intentar_cierre_r(s, r)) return;
}

/* ventana exacta: rehace un bloque alrededor de una casilla inventada con piezas reales que falten, sin tocar lo de afuera
 * y conservando los colores de las uniones internas. Se acepta si no baja el número de piezas reales. */
static void probar_ventana(Est *s) {
    int ni = 0; for (int rc = 0; rc < NN; rc++) if (!celda_fija[rc] && !es_real(s, rc)) s->inv[ni++] = rc;
    if (!ni) return;
    int c0 = s->inv[azar_n(s, ni)], r = ventana_r;
    int rr0 = c0 / n - r, cc0 = c0 % n - r;      /* ventana desplazada al azar para no centrarla siempre */
    int sh = azar_n(s, 3) - 1, sv = azar_n(s, 3) - 1; rr0 += sh; cc0 += sv;
    if (rr0 < 0) rr0 = 0;
    if (cc0 < 0) cc0 = 0;
    if (rr0 + 2 * r >= n) rr0 = n - 1 - 2 * r;
    if (cc0 + 2 * r >= n) cc0 = n - 1 - 2 * r;
    if (rr0 < 0 || cc0 < 0) return;
    memset(s->enR, 0, NN * sizeof(int)); s->nR = 0;
    for (int a = rr0; a <= rr0 + 2 * r; a++) for (int b = cc0; b <= cc0 + 2 * r; b++) { int x = a * n + b; if (!celda_fija[x]) { s->enR[x] = 1; s->celR[s->nR++] = x; } }
    /* piezas disponibles: las que no están (como reales) afuera */
    memset(s->avail, 0, NT * sizeof(int));
    for (int rc = 0; rc < NN; rc++) if (!s->enR[rc] && s->tipo[rc] >= 0) s->avail[s->tipo[rc]]++;
    for (int t = 0; t < NT; t++) { int fuera = s->avail[t] < tobj[t] ? s->avail[t] : tobj[t]; s->avail[t] = tobj[t] - fuera; }
    memset(s->cnt_int, 0, MAXCOL * sizeof(int));
    for (int i = 0; i < s->nR; i++) { int rc = s->celR[i];
        for (int d = 1; d <= 2; d++) { int e = lados[rc][d]; if (e < 0) continue; if (s->enR[ec[e][2]]) { if (s->col[e] >= MAXCOL) return; s->cnt_int[s->col[e]]++; } } }
    memset(s->puesto, 0, NN * sizeof(int));
    s->nodos = 0; s->abortado = 0; s->limite_nodos = ventana_nodos; s->en_ventana = 1;
    int ok = dfs_cierre(s, s->nR);
    s->en_ventana = 0; s->vent_intentos++; s->cierres_nodos += s->nodos;
    if (!ok) return;
    memcpy(s->ecol_bk, s->col, NE * sizeof(int)); int antes = s->reales;
    for (int i = 0; i < s->nR; i++) { int rc = s->celR[i]; for (int d = 0; d < 4; d++) { int e = lados[rc][d]; if (e >= 0) s->col[e] = s->pq[rc].c[d]; } }
    recalcular(s);
    if (s->reales < antes) { memcpy(s->col, s->ecol_bk, NE * sizeof(int)); recalcular(s); return; }
    s->vent_exitos++; if (s->reales > antes) s->vent_mejoras++;
    s->nult = 0; for (int i = 0; i < s->nR && s->nult < 2 * MAXC; i++) s->ult_toc[s->nult++] = s->celR[i];
}

static int verificar(const Est *s) {
    if (s->reales != NN) return 0;
    for (int t = 0; t < NT; t++) if (s->tcnt[t] != tobj[t]) return 0;
    for (int rc = 0; rc < NN; rc++) if (s->tipo[rc] < 0) return 0;
    return 1;
}
static void guardar(const Est *s, const char *ruta) {
    FILE *f = fopen(ruta, "w"); if (!f) return;
    for (int r = 0; r < n; r++) { for (int c = 0; c < n; c++) { Pz p = pieza(s, r * n + c); fprintf(f, "%s%d %d %d %d", c ? "   " : "", p.c[0], p.c[1], p.c[2], p.c[3]); } fprintf(f, "\n"); }
    fclose(f);
}

/* ------------------------------------------------------------------ hilos */
static int H = 1, modo_replicas = -1;   /* -1: automático (réplicas con 4 hilos o más) */
static Est *E;
static atomic_int resuelto_por;        /* -1 nadie; si no, el hilo que llegó a todas reales */

static void *trabajar(void *arg) {     /* avanzar este hilo hasta su objetivo de pasos o de aceptados */
    Est *s = arg;
    while (s->pasos < s->objetivo_pasos && s->aceptados < s->objetivo_acept) {
        paso(s);
        if ((s->pasos & 16383) == 0) {
            if (cierre_k != 0) intentar_cierre(s);
            if (ventana_cada > 0 && s->reales < NN && ((s->pasos >> 14) % ventana_cada) == 0) probar_ventana(s);
        }
        if (s->reales == NN) { int no = -1; atomic_compare_exchange_strong(&resuelto_por, &no, s->id); break; }
        if ((s->pasos & 1023) == 0 && atomic_load_explicit(&resuelto_por, memory_order_relaxed) >= 0) break;
    }
    return NULL;
}
/* todos los hilos avanzan k pasos a la vez (o hasta que alguno resuelva) */
static void avanzar_todos(long long k, int hilo_acept, long long acept) {
    pthread_t th[256];
    for (int i = 0; i < H; i++) {
        E[i].objetivo_pasos = E[i].pasos + k;
        E[i].objetivo_acept = (i == hilo_acept) ? E[i].aceptados + acept : (long long)9e18;
    }
    if (hilo_acept >= 0) for (int i = 0; i < H; i++) if (i != hilo_acept) E[i].objetivo_pasos = E[i].pasos + 2000;
    for (int i = 0; i < H; i++) pthread_create(&th[i], NULL, trabajar, &E[i]);
    for (int i = 0; i < H; i++) pthread_join(th[i], NULL);
}
/* réplicas: los vecinos en la escalera de temperaturas intercambian temperaturas si conviene */
static long long intercambios = 0, intentos = 0;
static void intercambiar_replicas(Est *s) {
    if (!modo_replicas || H < 2) return;
    int ord[256]; for (int i = 0; i < H; i++) ord[i] = i;
    for (int i = 1; i < H; i++) { int x = ord[i], j = i - 1; while (j >= 0 && E[ord[j]].T > E[x].T) { ord[j + 1] = ord[j]; j--; } ord[j + 1] = x; }
    for (int i = 0; i + 1 < H; i++) {
        Est *a = &E[ord[i]], *b = &E[ord[i + 1]];
        /* energía = -reales; aceptar con min(1, exp((1/Ta - 1/Tb) * (Ea - Eb))) */
        double x = (1.0 / a->T - 1.0 / b->T) * ((double)(-a->reales) - (double)(-b->reales));
        intentos++;
        if (x >= 0 || azar01(s) < exp(x)) { double t = a->T; a->T = b->T; b->T = t; intercambios++; int o = ord[i]; ord[i] = ord[i + 1]; ord[i + 1] = o; }
    }
}
static int hilo_mejor(void) { int b = 0; for (int i = 1; i < H; i++) if (E[i].reales > E[b].reales) b = i; return b; }

static void foto(int m) {
    long long p = 0, a = 0; int mj = 0; for (int i = 0; i < H; i++) { p += E[i].pasos; a += E[i].aceptados; if (E[i].mejor > mj) mj = E[i].mejor; }
    Est *s = &E[m];
    printf("F %lld %lld %d %d %.4f %d %d |", p, a, s->reales, mj, s->T, s->recal, m);
    for (int i = 0; i < NE; i++) printf(" %d", s->col[i]);
    printf(" |"); for (int i = 0; i < s->nult; i++) printf(" %d", s->ult_toc[i]);
    printf(" |"); for (int i = 0; i < H; i++) printf(" %d", E[i].reales);
    printf("\n"); fflush(stdout);
}

int main(int argc, char **argv) {
    double t_fria = 0.15, t_caliente = 0.8, cada = 2.0; const char *parar = NULL;
    const char *ruta_prob = NULL;
    const char *ruta = NULL, *salida = NULL; double limite = 60; int servidor = 0; long long semilla = 1;
    Pista ps[64]; int nps = 0; H = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--limite") && i + 1 < argc) limite = atof(argv[++i]);
        else if (!strcmp(argv[i], "--semilla") && i + 1 < argc) semilla = atoll(argv[++i]);
        else if (!strcmp(argv[i], "--introducir") && i + 1 < argc) p_introducir = atof(argv[++i]);
        else if (!strcmp(argv[i], "-t") && i + 1 < argc) H = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--modo") && i + 1 < argc) { i++; modo_replicas = !strcmp(argv[i], "replicas") || !strcmp(argv[i], "réplicas"); }
        else if (!strcmp(argv[i], "--tfria") && i + 1 < argc) t_fria = atof(argv[++i]);
        else if (!strcmp(argv[i], "--tcaliente") && i + 1 < argc) t_caliente = atof(argv[++i]);
        else if (!strcmp(argv[i], "--servidor")) servidor = 1;
        else if (!strcmp(argv[i], "--cierre") && i + 1 < argc) cierre_k = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--cierre-nodos") && i + 1 < argc) cierre_nodos = atoll(argv[++i]);
        else if (!strcmp(argv[i], "--cierre-radio") && i + 1 < argc) cierre_rmax = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--ventana-libre")) ventana_libre = 1;
        else if (!strcmp(argv[i], "--orden-fijo")) usar_orden_fijo = 1;
        else if (!strcmp(argv[i], "--mrv-total")) mrv_total = 1;
        else if (!strcmp(argv[i], "--fc")) usar_fc = 1;
        else if (!strcmp(argv[i], "--solo-exacto")) solo_exacto = 1;
        else if (!strcmp(argv[i], "--orden-creencias") && i + 1 < argc) ruta_prob = argv[++i];
        else if (!strcmp(argv[i], "--precios") && i + 1 < argc) precio_eta = atof(argv[++i]);
        else if (!strcmp(argv[i], "--gls") && i + 1 < argc) gls_alfa = atof(argv[++i]);
        else if (!strcmp(argv[i], "--atasco") && i + 1 < argc) atasco_pasos = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--cierre-azar") && i + 1 < argc) cierre_azar = atof(argv[++i]);
        else if (!strcmp(argv[i], "--cierre-frac") && i + 1 < argc) cierre_frac = atof(argv[++i]);
        else if (!strcmp(argv[i], "--ventana") && i + 1 < argc) ventana_cada = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--ventana-radio") && i + 1 < argc) ventana_r = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--ventana-nodos") && i + 1 < argc) ventana_nodos = atoll(argv[++i]);
        else if (!strcmp(argv[i], "--cada") && i + 1 < argc) cada = atof(argv[++i]);
        else if (!strcmp(argv[i], "--parar") && i + 1 < argc) parar = argv[++i];
        else if (!strcmp(argv[i], "-o") && i + 1 < argc) salida = argv[++i];
        else if (!strcmp(argv[i], "-P") && i + 1 < argc && nps < 64) {
            int f, c, k, g = 0; if (sscanf(argv[++i], "%d,%d,%d,%d", &f, &c, &k, &g) < 3) { fprintf(stderr, "Pieza fija inválida\n"); return 1; }
            ps[nps].r = f - 1; ps[nps].c = c - 1; ps[nps].k = k - 1; ps[nps].g = g & 3; nps++;
        }
        else if (argv[i][0] != '-') ruta = argv[i];
        else { fprintf(stderr, "Opción desconocida %s\n", argv[i]); return 1; }
    }
    if (!ruta) { fprintf(stderr, "Uso: e2uniones TABLERO.txt [-t hilos] [--modo replicas|independiente] [--tfria 0.15] [--tcaliente 0.8] [--limite S] [--semilla K] [--introducir P] [-P F,C,K,G] [-o sol.txt] [--servidor]\n"); return 1; }
#ifdef _WIN32
    SetConsoleOutputCP(65001);
#endif
    if (H <= 0) H = num_cpus();
    if (H > 256) H = 256;
    if (modo_replicas < 0) modo_replicas = H >= 4;
    leer(ruta);
    for (int i = 0; i < nps; i++) if (ps[i].r < 0 || ps[i].r >= n || ps[i].c < 0 || ps[i].c >= n || ps[i].k < 0 || ps[i].k >= NN) { fprintf(stderr, "Pieza fija fuera del tablero\n"); return 1; }
    preparar(ps, nps);
    if (ruta_prob) {   /* líneas: casilla N E S O probabilidad */
        prob_orden = xm((size_t)NN * NT * 4 * sizeof(float));
        FILE *f = fopen(ruta_prob, "r"); if (!f) { fprintf(stderr, "No puedo abrir %s\n", ruta_prob); return 1; }
        int rc, c0, c1, c2, c3; float p;
        while (fscanf(f, "%d %d %d %d %d %f", &rc, &c0, &c1, &c2, &c3, &p) == 6) {
            Pz q = {{c0, c1, c2, c3}}; int t = buscar_tipo(canon(q)); if (t < 0 || rc < 0 || rc >= NN) continue;
            for (int j = 0; j < ntrot[t]; j++) if (clave(trot[t][j]) == clave(q)) prob_orden[(size_t)rc * NT * 4 + t * 4 + j] = p;
        }
        fclose(f);
    }
    if (solo_exacto) {   /* toda la búsqueda exacta, desde la esquina (o pistas), sin recocido */
        Est s0; iniciar_estado(&s0, 0, semilla);
        memset(s0.enR, 0, NN * sizeof(int)); s0.nR = 0;
        for (int rc = 0; rc < NN; rc++) if (!celda_fija[rc]) { s0.enR[rc] = 1; s0.celR[s0.nR++] = rc; }
        memcpy(s0.avail, tobj, NT * sizeof(int));
        for (int rc = 0; rc < NN; rc++) if (celda_fija[rc]) s0.avail[s0.tipo[rc]]--;
        memset(s0.puesto, 0, NN * sizeof(int)); s0.nodos = 0; s0.abortado = 0; s0.limite_nodos = cierre_nodos;
        double t0 = ahora(); int ok = dfs_cierre(&s0, s0.nR);
        printf("EXACTO resuelto=%d nodos=%lld s=%.2f orden=%s\n", ok, s0.nodos, ahora() - t0, ruta_prob ? "creencias" : "normal");
        return ok ? 0 : 3;
    }
    E = xm(H * sizeof(Est));
    for (int i = 0; i < H; i++) iniciar_estado(&E[i], i, semilla);
    if (modo_replicas) {   /* escalera de temperaturas fijas, de --tfria a --tcaliente */
        for (int i = 0; i < H; i++) { E[i].fija_T = 1; E[i].T = H > 1 ? t_fria * pow(t_caliente / t_fria, (double)i / (H - 1)) : t_fria; }
    }
    atomic_store(&resuelto_por, -1);
    Est aux; memset(&aux, 0, sizeof aux); aux.rs = 0xABCDEF12345ULL + (u64)semilla;

    if (servidor) {
        int guardada = 0, mostrado = 0;
        printf("INICIO %d %d %d\n", n, NE, H); foto(0);
        char lin[256];
        while (fgets(lin, sizeof lin, stdin)) {
            char o = lin[0]; double x = atof(lin + 1);
            if (o == 'Q') break;
            if (o == 'I') { p_introducir = x; foto(mostrado); continue; }
            if (atomic_load(&resuelto_por) < 0) {
                if (o == 'N') {
                    long long k = (long long)x, hecho = 0;
                    while (hecho < k && atomic_load(&resuelto_por) < 0) { long long tramo = k - hecho < 20000 ? k - hecho : 20000; avanzar_todos(tramo, -1, 0); intercambiar_replicas(&aux); hecho += tramo; }
                    mostrado = hilo_mejor();
                }
                if (o == 'A') { avanzar_todos(20000, mostrado, (long long)x); intercambiar_replicas(&aux); }
            }
            int r = atomic_load(&resuelto_por); if (r >= 0) mostrado = r;
            if (r >= 0 && salida && !guardada && verificar(&E[r])) { guardar(&E[r], salida); guardada = 1; }
            foto(mostrado);
        }
        return 0;
    }

    /* modo texto: los hilos avanzan en tramos de 20 000 pasos; entre tramos se mira el reloj y, en réplicas, se intercambia */
    printf("Tablero %dx%d: %d casillas, %d uniones, %d hilos, modo %s.\n", n, n, NN, NE, H, modo_replicas ? "réplicas" : "independiente");
    fflush(stdout);
    double t0 = ahora(), ult = t0, ult_parar = t0;
    while (atomic_load(&resuelto_por) < 0) {
        avanzar_todos(20000, -1, 0);
        intercambiar_replicas(&aux);
        double t = ahora();
        if (t - t0 > limite) break;
        if (parar && t - ult_parar >= 0.3) {   /* la ventana pide parar creando este archivo */
            ult_parar = t; FILE *fp = fopen(parar, "r"); if (fp) { fclose(fp); remove(parar); break; }
        }
        if (t - ult >= cada) {
            ult = t; long long p = 0; int mj = 0, b = hilo_mejor(); for (int i = 0; i < H; i++) { p += E[i].pasos; if (E[i].mejor > mj) mj = E[i].mejor; }
            printf("  %7.1f s | pasos %lld (%.1f M/s) | mejor hilo ahora: %d/%d | mejor de todos: %d | reales por hilo:", t - t0, p, p / (t - t0) / 1e6, E[b].reales, NN, mj);
            for (int i = 0; i < H; i++) printf(" %d", E[i].reales);
            if (modo_replicas) printf(" | intercambios aceptados %lld de %lld", intercambios, intentos);
            { long long ci = 0; for (int i = 0; i < H; i++) ci += E[i].cierres_intentos; printf(" | cierres probados %lld", ci); }
            printf("\n"); fflush(stdout);
        }
    }
    double dt = ahora() - t0; int r = atomic_load(&resuelto_por);
    long long p = 0; int mj = 0; for (int i = 0; i < H; i++) { p += E[i].pasos; if (E[i].mejor > mj) mj = E[i].mejor; }
    int ok = r >= 0 && verificar(&E[r]);
    printf("RESULTADO resuelto=%d s=%.2f pasos=%lld mejor=%d/%d hilos=%d modo=%s pasos_por_s=%.0f\n", ok, dt, p, mj, NN, H, modo_replicas ? "replicas" : "independiente", p / (dt > 1e-9 ? dt : 1e-9));
    if (ok && salida) { guardar(&E[r], salida); printf("Solución guardada en %s (la encontró el hilo %d)\n", salida, r); }
    { long long ci = 0, ce = 0, cn = 0; for (int i = 0; i < H; i++) { ci += E[i].cierres_intentos; ce += E[i].cierres_exitos; cn += E[i].cierres_nodos; }
      long long vi = 0, ve = 0, vm = 0; for (int i = 0; i < H; i++) { vi += E[i].vent_intentos; ve += E[i].vent_exitos; vm += E[i].vent_mejoras; }
      printf("VENTANA intentos=%lld exitos=%lld mejoras=%lld\n", vi, ve, vm);
      printf("CIERRE intentos=%lld exitos=%lld nodos=%lld%s\n", ci, ce, cn, ok && E[r].cierres_exitos ? " (la solución salió del cierre exacto)" : ""); }
    /* el mejor tablero que se llegó a tener (en cualquier hilo), para que la ventana lo dibuje */
    int bm = 0; for (int i = 1; i < H; i++) if (E[i].mejor > E[bm].mejor) bm = i;
    if (ok) bm = r;
    printf("FINAL %d %d |", bm, ok ? NN : E[bm].mejor);
    for (int i = 0; i < NE; i++) printf(" %d", ok ? E[bm].col[i] : E[bm].mcol[i]);
    printf("\n"); fflush(stdout);
    return ok ? 0 : 3;
}
