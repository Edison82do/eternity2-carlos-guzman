/*
 * e2uniones.c — Motor en C del SISTEMA DE UNIONES (idea de Carlos, versión 5).
 * Misma lógica que el prototipo e2uniones.py, pero mucho más rápido.
 *
 * Representación: se eligen los COLORES DE LAS UNIONES entre casillas; el tablero siempre está armado y
 * la proporción de colores siempre es la del tablero objetivo (los colores solo se intercambian).
 * Una casilla es "real" si su pieza existe en el juego (contando copias). Meta: todas reales = solución.
 * Movimientos: intercambio de dos uniones, e "introducir pieza" (traer los colores de una pieza que falta).
 * Búsqueda: recocido simulado con recalentamiento.
 *
 * Uso solo texto (lo más rápido):
 *   e2uniones TABLERO.txt [--limite S] [--semilla K] [--introducir P] [-P F,C,K,G ...] [-o solucion.txt]
 * Uso con la ventana (ventana_uniones.py lo lanza así y le da órdenes por la entrada estándar):
 *   e2uniones TABLERO.txt --servidor ...
 *   órdenes:  N k   avanzar k pasos         A k   avanzar hasta k cambios aceptados
 *             I p   probabilidad de "introducir pieza"     Q   salir
 *   respuesta a cada orden, una línea:
 *   F pasos aceptados reales mejor temperatura recalentadas | color de cada unión | casillas tocadas
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#ifdef _WIN32
#include <windows.h>
static double ahora(void) { static LARGE_INTEGER f; static int i = 0; LARGE_INTEGER c; if (!i) { QueryPerformanceFrequency(&f); i = 1; } QueryPerformanceCounter(&c); return (double)c.QuadPart / f.QuadPart; }
#else
#include <time.h>
static double ahora(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec * 1e-9; }
#endif

typedef uint32_t u32; typedef uint64_t u64;
static void *xm(size_t n) { void *p = calloc(1, n ? n : 1); if (!p) { fprintf(stderr, "Sin memoria\n"); exit(2); } return p; }

/* ------------------------------------------------------------------ azar */
static u64 rs = 0x1234567ULL;
static inline u64 azar(void) { u64 z = (rs += 0x9E3779B97F4A7C15ULL); z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL; z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL; return z ^ (z >> 31); }
static inline int azar_n(int n) { return (int)(azar() % (u64)n); }
static inline double azar01(void) { return (azar() >> 11) * (1.0 / 9007199254740992.0); }

/* ------------------------------------------------------------------ piezas */
typedef struct { int c[4]; } Pz;   /* N E S O */
static Pz rot(Pz p, int k) { for (k &= 3; k > 0; k--) { Pz q = {{p.c[3], p.c[0], p.c[1], p.c[2]}}; p = q; } return p; }
static u32 clave(Pz p) { return (u32)p.c[0] << 24 | (u32)p.c[1] << 16 | (u32)p.c[2] << 8 | (u32)p.c[3]; }
static u32 canon(Pz p) { u32 m = 0xFFFFFFFFu; for (int k = 0; k < 4; k++) { u32 x = clave(rot(p, k)); if (x < m) m = x; } return m; }
static Pz de_clave(u32 x) { Pz p = {{(int)(x >> 24), (int)(x >> 16 & 255), (int)(x >> 8 & 255), (int)(x & 255)}}; return p; }

/* ------------------------------------------------------------------ datos */
static int n, NN, NE;
static Pz *pz;                         /* piezas del archivo */
static int (*ec)[4];                   /* cada unión: casilla1, lado1, casilla2, lado2 */
static int (*lados)[4];                /* cada casilla: sus 4 uniones (N E S O), -1 = gris */
static int *clase, *col, *fija;        /* unión: 1 marco / 0 resto; color; fija */
static int *libres[2], nlib[2];
static int *celda_fija;
/* tipos objetivo */
static int NT; static u32 *tclave; static int *tobj, *tcnt;
static int HT; static int *htab;       /* tabla hash clave -> índice de tipo */
static int *tipo;                      /* tipo de cada casilla (-1 = no existe en el juego) */
static int reales, mejor; static long long pasos, aceptados;
static double p_introducir = 0.3;

static int buscar_tipo(u32 k) { u32 h = (k * 2654435761u) & (HT - 1); while (htab[h] >= 0) { if (tclave[htab[h]] == k) return htab[h]; h = (h + 1) & (HT - 1); } return -1; }
static inline Pz pieza(int rc) { Pz p; for (int d = 0; d < 4; d++) { int e = lados[rc][d]; p.c[d] = e < 0 ? 0 : col[e]; } return p; }
static inline int es_real(int rc) { int t = tipo[rc]; return t >= 0 && tcnt[t] <= tobj[t]; }
static inline int quitar(int t) { if (t < 0) return 0; int d = tcnt[t] <= tobj[t] ? -1 : 0; tcnt[t]--; return d; }
static inline int poner(int t) { if (t < 0) return 0; int d = tcnt[t] < tobj[t] ? 1 : 0; tcnt[t]++; return d; }

/* ------------------------------------------------------------------ lectura y armado inicial */
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
    ec = xm(NE * sizeof(*ec)); lados = xm(NN * sizeof(*lados)); clase = xm(NE * sizeof(int)); col = xm(NE * sizeof(int)); fija = xm(NE * sizeof(int));
    int e = 0;
    int *H = xm(NN * sizeof(int)), *V = xm(NN * sizeof(int));
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
    /* tipos objetivo */
    u32 *ks = xm(NN * sizeof(u32)); for (int i = 0; i < NN; i++) ks[i] = canon(pz[i]);
    tclave = xm(NN * sizeof(u32)); tobj = xm(NN * sizeof(int)); NT = 0;
    HT = 1; while (HT < 4 * NN) HT <<= 1; htab = xm(HT * sizeof(int)); for (int i = 0; i < HT; i++) htab[i] = -1;
    for (int i = 0; i < NN; i++) {
        int t = buscar_tipo(ks[i]);
        if (t < 0) { t = NT++; tclave[t] = ks[i]; u32 h = (ks[i] * 2654435761u) & (HT - 1); while (htab[h] >= 0) h = (h + 1) & (HT - 1); htab[h] = t; }
        tobj[t]++;
    }
    tcnt = xm(NT * sizeof(int)); tipo = xm(NN * sizeof(int));
    /* bolsas de colores por clase */
    int maxc = 0; for (int i = 0; i < NN; i++) for (int d = 0; d < 4; d++) if (pz[i].c[d] > maxc) maxc = pz[i].c[d];
    int *med[2]; med[0] = xm((maxc + 1) * sizeof(int)); med[1] = xm((maxc + 1) * sizeof(int));
    for (int i = 0; i < NN; i++) {
        int g = 0; for (int d = 0; d < 4; d++) g += pz[i].c[d] == 0;
        for (int d = 0; d < 4; d++) {
            if (pz[i].c[d] == 0) continue;
            int k = (g >= 1 && (pz[i].c[(d + 1) & 3] == 0 || pz[i].c[(d + 3) & 3] == 0)) ? 1 : 0;
            med[k][pz[i].c[d]]++;
        }
    }
    int *bolsa[2], nb[2] = {0, 0}; bolsa[0] = xm(NE * sizeof(int)); bolsa[1] = xm(NE * sizeof(int));
    int cuantas[2] = {0, 0}; for (int i = 0; i < NE; i++) cuantas[clase[i]]++;
    for (int k = 0; k < 2; k++) for (int c = 1; c <= maxc; c++) {
        if (med[k][c] % 2) { fprintf(stderr, "El color %d aparece un número impar de veces\n", c); exit(1); }
        for (int j = 0; j < med[k][c] / 2; j++) { if (nb[k] >= NE) { fprintf(stderr, "Demasiados colores\n"); exit(1); } bolsa[k][nb[k]++] = c; }
    }
    if (nb[0] != cuantas[0] || nb[1] != cuantas[1]) { fprintf(stderr, "Los colores no alcanzan para las uniones (%d/%d, %d/%d)\n", nb[0], cuantas[0], nb[1], cuantas[1]); exit(1); }
    /* uniones fijas: pistas, o una esquina arriba a la izquierda si no hay pistas */
    for (int i = 0; i < NE; i++) col[i] = -1;
    celda_fija = xm(NN * sizeof(int));
    Pista esq;
    if (nps == 0) {
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
            if (col[u] >= 0) { if (col[u] != q.c[d]) { fprintf(stderr, "Dos piezas fijas no encajan entre sí\n"); exit(1); } continue; }
            col[u] = q.c[d]; fija[u] = 1;
            int k = clase[u], j; for (j = 0; j < nb[k]; j++) if (bolsa[k][j] == q.c[d]) break;
            if (j == nb[k]) { fprintf(stderr, "La pieza fija usa un color que no está disponible\n"); exit(1); }
            bolsa[k][j] = bolsa[k][--nb[k]];
        }
    }
    /* reparto al azar */
    for (int k = 0; k < 2; k++) {
        for (int j = nb[k] - 1; j > 0; j--) { int r = azar_n(j + 1); int t = bolsa[k][j]; bolsa[k][j] = bolsa[k][r]; bolsa[k][r] = t; }
        int j = 0; for (int i = 0; i < NE; i++) if (clase[i] == k && col[i] < 0) col[i] = bolsa[k][j++];
    }
    for (int k = 0; k < 2; k++) { libres[k] = xm(NE * sizeof(int)); nlib[k] = 0; for (int i = 0; i < NE; i++) if (clase[i] == k && !fija[i]) libres[k][nlib[k]++] = i; }
    for (int rc = 0; rc < NN; rc++) { tipo[rc] = buscar_tipo(canon(pieza(rc))); if (tipo[rc] >= 0) tcnt[tipo[rc]]++; }
    reales = 0; for (int t = 0; t < NT; t++) reales += tcnt[t] < tobj[t] ? tcnt[t] : tobj[t];
    mejor = reales;
    free(H); free(V); free(ks); free(med[0]); free(med[1]); free(bolsa[0]); free(bolsa[1]);
}

/* ------------------------------------------------------------------ aplicar / deshacer cambios */
#define MAXC 16
static int cu[MAXC], cn[MAXC], cv[MAXC], ncamb;     /* unión, color nuevo, color viejo */
static int toc[2 * MAXC], ntoc;

static int aplicar(void) {
    ntoc = 0;
    for (int i = 0; i < ncamb; i++) for (int s = 0; s < 3; s += 2) {
        int rc = ec[cu[i]][s], j; for (j = 0; j < ntoc; j++) if (toc[j] == rc) break; if (j == ntoc) toc[ntoc++] = rc;
    }
    int delta = 0;
    for (int j = 0; j < ntoc; j++) delta += quitar(tipo[toc[j]]);
    for (int i = 0; i < ncamb; i++) { cv[i] = col[cu[i]]; col[cu[i]] = cn[i]; }
    for (int j = 0; j < ntoc; j++) { int t = buscar_tipo(canon(pieza(toc[j]))); tipo[toc[j]] = t; delta += poner(t); }
    return delta;
}
static void deshacer(void) {
    for (int j = 0; j < ntoc; j++) quitar(tipo[toc[j]]);
    for (int i = ncamb - 1; i >= 0; i--) col[cu[i]] = cv[i];
    for (int j = 0; j < ntoc; j++) { int t = buscar_tipo(canon(pieza(toc[j]))); tipo[toc[j]] = t; poner(t); }
}

/* ------------------------------------------------------------------ movimientos (iguales al prototipo) */
static int mov_intercambio(void) {
    int k = (azar01() < 0.85 || nlib[1] == 0) ? 0 : 1; int *L = libres[k], nl = nlib[k];
    if (nl < 2) return 0;
    int e1 = L[azar_n(nl)];
    for (int i = 0; i < 6; i++) { if (!(es_real(ec[e1][0]) && es_real(ec[e1][2]))) break; e1 = L[azar_n(nl)]; }
    int e2 = L[azar_n(nl)];
    if (col[e1] == col[e2]) return 0;
    ncamb = 2; cu[0] = e1; cn[0] = col[e2]; cu[1] = e2; cn[1] = col[e1];
    return 1;
}

static int *inv, *falt; static Pz *opq; static int *opi, *cand;
static int mov_introducir(void) {
    int ni = 0; for (int rc = 0; rc < NN; rc++) if (!celda_fija[rc] && !es_real(rc)) inv[ni++] = rc;
    if (!ni) return 0;
    int rc = inv[azar_n(ni)];
    int nf = 0; for (int t = 0; t < NT; t++) if (tcnt[t] < tobj[t]) falt[nf++] = t;
    if (!nf) return 0;
    int no = 0, mx = -1;
    for (int i = 0; i < nf; i++) {
        Pz base = de_clave(tclave[falt[i]]);
        for (int g = 0; g < 4; g++) {
            Pz q = rot(base, g); int ok = 1, ig = 0;
            for (int d = 0; d < 4 && ok; d++) {
                int e = lados[rc][d];
                if ((e < 0) != (q.c[d] == 0)) { ok = 0; break; }
                if (e >= 0) { if (fija[e] && col[e] != q.c[d]) { ok = 0; break; } if (col[e] == q.c[d]) ig++; }
            }
            if (ok) { opq[no] = q; opi[no] = ig; no++; if (ig > mx) mx = ig; }
        }
    }
    if (!no) return 0;
    int nbuen = 0; for (int i = 0; i < no; i++) if (opi[i] >= mx - 1) opq[nbuen++] = opq[i];
    Pz q = opq[azar_n(nbuen)];
    ncamb = 0; int usados[8], nus = 0;
    for (int d = 0; d < 4; d++) if (lados[rc][d] >= 0) usados[nus++] = lados[rc][d];
    for (int d = 0; d < 4; d++) {
        int e = lados[rc][d]; if (e < 0) continue;
        /* el color actual de e puede haber cambiado ya por un intercambio anterior de este mismo movimiento */
        int ce = col[e]; for (int i = 0; i < ncamb; i++) if (cu[i] == e) ce = cn[i];
        if (ce == q.c[d]) continue;
        int k = clase[e], nc = 0, mejor_r = 3;
        for (int j = 0; j < nlib[k]; j++) {
            int f = libres[k][j]; int cf = col[f]; for (int i = 0; i < ncamb; i++) if (cu[i] == f) cf = cn[i];
            if (cf != q.c[d]) continue;
            int u; for (u = 0; u < nus; u++) if (usados[u] == f) break; if (u < nus) continue;
            int r = es_real(ec[f][0]) + es_real(ec[f][2]);
            if (r < mejor_r) { mejor_r = r; nc = 0; }
            if (r == mejor_r) cand[nc++] = f;
        }
        if (!nc) return 0;
        int f = cand[azar_n(nc)]; usados[nus++] = f;
        /* intercambiar e <-> f (registrando el último valor de cada unión) */
        int i;
        for (i = 0; i < ncamb; i++) if (cu[i] == e) break;
        if (i == ncamb) cu[ncamb++] = e;
        cn[i] = q.c[d];
        for (i = 0; i < ncamb; i++) if (cu[i] == f) break;
        if (i == ncamb) cu[ncamb++] = f;
        cn[i] = ce;
        if (ncamb > MAXC - 2) return 0;
    }
    return ncamb > 0;
}

/* ------------------------------------------------------------------ recocido */
static double T = 2.0, T0 = 2.0, Tmin = 0.05, enfriar = 0.99995; static long long paciencia = 200000, sin_mejora = 0; static int ult_mejor = -1, recal = 0;
static int ult_toc[2 * MAXC], nult = 0;

static int paso(void) {
    pasos++;
    int ok = azar01() < p_introducir ? mov_introducir() : mov_intercambio();
    int acept = 0;
    if (ok) {
        int d = aplicar();
        if (d >= 0 || azar01() < exp(d / (T > 1e-9 ? T : 1e-9))) {
            reales += d; aceptados++; acept = 1; if (reales > mejor) mejor = reales;
            nult = ntoc; memcpy(ult_toc, toc, ntoc * sizeof(int));
        } else deshacer();
    }
    if (mejor > ult_mejor) { ult_mejor = mejor; sin_mejora = 0; } else sin_mejora++;
    T *= enfriar; if (T < Tmin) T = Tmin;
    if (sin_mejora > paciencia) { T = T0 * 0.6; sin_mejora = 0; recal++; }
    return acept;
}

/* ------------------------------------------------------------------ verificación y salida */
static int verificar(void) {
    if (reales != NN) return 0;
    for (int t = 0; t < NT; t++) if (tcnt[t] != tobj[t]) return 0;
    for (int rc = 0; rc < NN; rc++) if (tipo[rc] < 0) return 0;
    return 1;
}
static void guardar(const char *ruta) {
    FILE *f = fopen(ruta, "w"); if (!f) return;
    for (int r = 0; r < n; r++) { for (int c = 0; c < n; c++) { Pz p = pieza(r * n + c); fprintf(f, "%s%d %d %d %d", c ? "   " : "", p.c[0], p.c[1], p.c[2], p.c[3]); } fprintf(f, "\n"); }
    fclose(f);
}
static void foto(void) {
    printf("F %lld %lld %d %d %.4f %d |", pasos, aceptados, reales, mejor, T, recal);
    for (int i = 0; i < NE; i++) printf(" %d", col[i]);
    printf(" |"); for (int i = 0; i < nult; i++) printf(" %d", ult_toc[i]);
    printf("\n"); fflush(stdout);
}

int main(int argc, char **argv) {
    const char *ruta = NULL, *salida = NULL; double limite = 60; int servidor = 0; long long semilla = 1;
    Pista ps[64]; int nps = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--limite") && i + 1 < argc) limite = atof(argv[++i]);
        else if (!strcmp(argv[i], "--semilla") && i + 1 < argc) semilla = atoll(argv[++i]);
        else if (!strcmp(argv[i], "--introducir") && i + 1 < argc) p_introducir = atof(argv[++i]);
        else if (!strcmp(argv[i], "--servidor")) servidor = 1;
        else if (!strcmp(argv[i], "-o") && i + 1 < argc) salida = argv[++i];
        else if (!strcmp(argv[i], "-P") && i + 1 < argc && nps < 64) {
            int f, c, k, g = 0; if (sscanf(argv[++i], "%d,%d,%d,%d", &f, &c, &k, &g) < 3) { fprintf(stderr, "Pieza fija inválida\n"); return 1; }
            ps[nps].r = f - 1; ps[nps].c = c - 1; ps[nps].k = k - 1; ps[nps].g = g & 3; nps++;
        }
        else if (argv[i][0] != '-') ruta = argv[i];
        else { fprintf(stderr, "Opción desconocida %s\n", argv[i]); return 1; }
    }
    if (!ruta) { fprintf(stderr, "Uso: e2uniones TABLERO.txt [--limite S] [--semilla K] [--introducir P] [-P F,C,K,G] [-o sol.txt] [--servidor]\n"); return 1; }
#ifdef _WIN32
    SetConsoleOutputCP(65001);
#endif
    rs = 0x9E3779B97F4A7C15ULL * (u64)(semilla + 1);
    leer(ruta);
    for (int i = 0; i < nps; i++) if (ps[i].r < 0 || ps[i].r >= n || ps[i].c < 0 || ps[i].c >= n || ps[i].k < 0 || ps[i].k >= NN) { fprintf(stderr, "Pieza fija fuera del tablero\n"); return 1; }
    preparar(ps, nps);
    inv = xm(NN * sizeof(int)); falt = xm(NN * sizeof(int)); opq = xm(4 * NN * sizeof(Pz)); opi = xm(4 * NN * sizeof(int)); cand = xm(NE * sizeof(int));

    if (servidor) {   /* la ventana manda órdenes y recibe una "foto" por cada una */
        int guardada = 0;
        printf("INICIO %d %d\n", n, NE); foto();
        char lin[256];
        while (fgets(lin, sizeof lin, stdin)) {
            char o = lin[0]; double x = atof(lin + 1);
            if (o == 'Q') break;
            if (o == 'I') { p_introducir = x; foto(); continue; }
            if (o == 'N') { long long k = (long long)x; for (long long i = 0; i < k && reales < NN; i++) paso(); }
            if (o == 'A') { long long k = (long long)x, a0 = aceptados; for (long long i = 0; i < 20000 && aceptados - a0 < k && reales < NN; i++) paso(); }
            if (reales == NN && salida && !guardada && verificar()) { guardar(salida); guardada = 1; }
            foto();
        }
        if (!guardada && verificar() && salida) guardar(salida);
        return 0;
    }

    /* modo texto: el motor corre libre; mira el reloj cada 4096 pasos y escribe una línea cada 2 s */
    printf("Tablero %dx%d: %d casillas, %d uniones. Reales al empezar: %d/%d\n", n, n, NN, NE, reales, NN);
    double t0 = ahora(), ult = t0;
    while (reales < NN) {
        paso();
        if ((pasos & 4095) == 0) {
            double t = ahora();
            if (t - t0 > limite) break;
            if (t - ult >= 2.0) { ult = t; printf("  %7.1f s | pasos %lld | reales %d/%d (mejor %d) | temperatura %.3f | recalentadas %d\n", t - t0, pasos, reales, NN, mejor, T, recal); fflush(stdout); }
        }
    }
    double dt = ahora() - t0; int ok = verificar();
    printf("RESULTADO resuelto=%d s=%.2f pasos=%lld mejor=%d/%d pasos_por_s=%.0f\n", ok, dt, pasos, mejor, NN, pasos / (dt > 1e-9 ? dt : 1e-9));
    if (ok && salida) { guardar(salida); printf("Solución guardada en %s\n", salida); }
    return ok ? 0 : 3;
}
