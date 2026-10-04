/*
 * e2marcos44 — Motor (versión 4.4: en grupos grandes, tres estrategias a la vez (normal, pieza menos
 *              restrictiva y reinicios al azar); piezas fijas con las 4 esquinas arriba a la izquierda)
 * Basado en e2marcos43 — (versión 4.3: avance del grupo, estimado de Knuth, grupo en cada línea,
 *              mapa de todas las filas (--anillo2 2) por defecto)
 * Basado en e2marcos42 — (versión 4.2: mapa del segundo anillo, idea de Carlos)
 * Basado en e2marcos41 (versión 4.1: reparto dinámico del trabajo entre hilos)
 * Basado en e2marcos4 — Motor (versión 4) del MÉTODO POR MARCOS Y FIRMAS de Carlos.
 *   Versión 4: ruta unificada (mapas por lado + orilla e interior en la misma búsqueda),
 *   conteo temprano de piezas y piezas fijas (pistas). Ver unificado.c.
 *
 *   1. Esquina fija.            2. Grupos (permutaciones de las otras 3 esquinas).
 *   3. Marcos completos por grupo, sin repetir piezas idénticas.
 *   4. Firmas: colores que el marco muestra hacia adentro.
 *   5. Interior sin orden lineal (casilla más restringida), filtrado por firmas vivas.
 *   6. Grupo a grupo, con todos los núcleos en el mismo grupo.
 *   7. Firmas de grupos agotados sin solución se descartan en los siguientes.
 *
 * Uso:  e2marcos4 TABLERO.txt [-t hilos] [-l segundos] [-s semilla] [-o solucion.txt] [--unificado] [-P F,C,K[,G]] [-q]
 * Formato del tablero: el del Eternity II Editor (4 números N E S O por pieza, una fila por línea).
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <stdatomic.h>
#include <pthread.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#include <unistd.h>
#endif

typedef uint64_t u64;
typedef uint8_t u8;

/* ------------------------------------------------------------------ utilidades */
static double ahora(void) {
#ifdef _WIN32
    static LARGE_INTEGER f; static int ini = 0; LARGE_INTEGER c;
    if (!ini) { QueryPerformanceFrequency(&f); ini = 1; }
    QueryPerformanceCounter(&c); return (double)c.QuadPart / (double)f.QuadPart;
#else
    struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec * 1e-9;
#endif
}
static int num_cpus(void) {
#ifdef _WIN32
    SYSTEM_INFO si; GetSystemInfo(&si); return (int)si.dwNumberOfProcessors;
#else
    long n = sysconf(_SC_NPROCESSORS_ONLN); return n > 0 ? (int)n : 1;
#endif
}
static void *xmalloc(size_t n) { void *p = calloc(1, n ? n : 1); if (!p) { fprintf(stderr, "Sin memoria (%zu bytes)\n", n); exit(2); } return p; }
static int popcount64(u64 x) { return __builtin_popcountll(x); }
static int ctz64(u64 x) { return __builtin_ctzll(x); }

typedef struct { u8 c[4]; } Pieza;      /* N E S O */
static Pieza rotar(Pieza p, int k) { for (k &= 3; k > 0; k--) { Pieza q = {{p.c[3], p.c[0], p.c[1], p.c[2]}}; p = q; } return p; }
static int cmp4(const u8 *a, const u8 *b) { return memcmp(a, b, 4); }
static Pieza canonica(Pieza p) { Pieza best = p; for (int k = 1; k < 4; k++) { Pieza r = rotar(p, k); if (cmp4(r.c, best.c) < 0) best = r; } return best; }
static int grises(Pieza p) { int g = 0; for (int i = 0; i < 4; i++) g += p.c[i] == 0; return g; }

static u64 sm_estado;
static u64 splitmix(void) { u64 z = (sm_estado += 0x9E3779B97F4A7C15ULL); z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL; z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL; return z ^ (z >> 31); }

/* ------------------------------------------------------------------ datos globales */
static int N, M, NP;                  /* lado, interior (N-2), número de piezas */
static Pieza *piezas;                 /* piezas tal como se leyeron (posiblemente mezcladas) */
static Pieza *piezas_orig;            /* piezas en el orden del archivo (para las pistas) */
static int NC;                        /* colores: 0..NC-1 (renumerados) */
static int color_original[256];       /* para escribir la solución con los números originales */
static int silencioso = 0;

/* esquinas normalizadas (a,b) y tipos de orilla (a,d,b) con cantidad */
typedef struct { u8 a, b; } Esq;
typedef struct { u8 a, d, b; int cant; } TOri;
static Esq esq[4];
static TOri *tori; static int ntori;
static int **ori_por_a; static int *nori_por_a;   /* tipos de orilla que empiezan con color a */

/* tipos de pieza interior y orientaciones */
static Pieza *tint; static int *cant_int; static int ntint;
typedef struct { int tipo; u8 c[4]; } Orient;
static Orient *orient; static int norient, OW;    /* OW = palabras de 64 bits para conjuntos de orientaciones */
static u64 *lado_color;   /* [4][NC][OW] */
static u64 *mascara_tipo; /* [ntint][OW] */
#define LC(s, c) (lado_color + ((size_t)(s) * NC + (c)) * OW)
#define MT(t) (mascara_tipo + (size_t)(t) * OW)

static Esq normalizar_esquina(Pieza p) { while (!(p.c[0] == 0 && p.c[3] == 0)) p = rotar(p, 1); Esq e = {p.c[2], p.c[1]}; return e; }
static void normalizar_orilla(Pieza p, u8 *a, u8 *d, u8 *b) { while (p.c[0] != 0) p = rotar(p, 1); *a = p.c[3]; *d = p.c[2]; *b = p.c[1]; }

/* ------------------------------------------------------------------ lectura */
static int leer_tablero(const char *ruta) {
    FILE *f = fopen(ruta, "r"); if (!f) { fprintf(stderr, "No puedo abrir %s\n", ruta); return 0; }
    int cap = 1024, n = 0; int *v = xmalloc(cap * sizeof(int)); char linea[8192];
    while (fgets(linea, sizeof linea, f)) {
        if (linea[0] == '#') continue;                 /* comentarios del Editor */
        char *p = linea, *fin; long x;
        while ((x = strtol(p, &fin, 10)), fin != p) { if (n == cap) { cap *= 2; v = realloc(v, cap * sizeof(int)); } v[n++] = (int)x; p = fin; }
    }
    fclose(f);
    if (n % 4) { fprintf(stderr, "El archivo no tiene grupos de 4 números\n"); return 0; }
    NP = n / 4; N = 0; while ((N + 1) * (N + 1) <= NP) N++;
    if (N * N != NP || N < 3) { fprintf(stderr, "%d piezas: el tablero debe ser cuadrado de 3x3 o más\n", NP); return 0; }
    M = N - 2; piezas = xmalloc(NP * sizeof(Pieza));
    /* los colores se renumeran 1..K (0 = gris) para no reservar memoria para colores que no existen */
    int mapa[256]; for (int c = 0; c < 256; c++) mapa[c] = -1; mapa[0] = 0; NC = 1;
    for (int i = 0; i < n; i++) {
        if (v[i] < 0 || v[i] > 255) { fprintf(stderr, "Color fuera de rango: %d\n", v[i]); return 0; }
        if (mapa[v[i]] < 0) mapa[v[i]] = NC++;
    }
    for (int i = 0; i < NP; i++) for (int k = 0; k < 4; k++) piezas[i].c[k] = (u8)mapa[v[4 * i + k]];
    for (int c = 0; c < 256; c++) if (mapa[c] > 0) color_original[mapa[c]] = c;
    piezas_orig = xmalloc(NP * sizeof(Pieza)); memcpy(piezas_orig, piezas, NP * sizeof(Pieza));
    free(v); return 1;
}

static void mezclar(u64 semilla) {
    sm_estado = semilla;
    for (int i = NP - 1; i > 0; i--) { int j = (int)(splitmix() % (u64)(i + 1)); Pieza t = piezas[i]; piezas[i] = piezas[j]; piezas[j] = t; }
    for (int i = 0; i < NP; i++) piezas[i] = rotar(piezas[i], (int)(splitmix() & 3));
}

/* ------------------------------------------------------------------ clasificación */
static int clasificar(void) {
    int ne = 0; tori = xmalloc(NP * sizeof(TOri)); ntori = 0;
    Pieza *ints = xmalloc(NP * sizeof(Pieza)); int nint = 0;
    for (int i = 0; i < NP; i++) {
        int g = grises(piezas[i]);
        if (g == 2) { if (ne < 4) esq[ne] = normalizar_esquina(piezas[i]); ne++; }
        else if (g == 1) {
            u8 a, d, b; normalizar_orilla(piezas[i], &a, &d, &b); int j;
            for (j = 0; j < ntori; j++) if (tori[j].a == a && tori[j].d == d && tori[j].b == b) { tori[j].cant++; break; }
            if (j == ntori) { tori[ntori].a = a; tori[ntori].d = d; tori[ntori].b = b; tori[ntori].cant = 1; ntori++; }
        } else if (g == 0) ints[nint++] = piezas[i];
        else { fprintf(stderr, "Pieza inválida\n"); return 0; }
    }
    if (ne != 4 || nint != M * M) { fprintf(stderr, "Esquinas=%d interiores=%d: tablero inválido\n", ne, nint); return 0; }
    ori_por_a = xmalloc(NC * sizeof(int *)); nori_por_a = xmalloc(NC * sizeof(int));
    for (int c = 0; c < NC; c++) ori_por_a[c] = xmalloc((ntori + 1) * sizeof(int));
    for (int j = 0; j < ntori; j++) { int a = tori[j].a; ori_por_a[a][nori_por_a[a]++] = j; }
    /* tipos interiores */
    tint = xmalloc(nint * sizeof(Pieza)); cant_int = xmalloc(nint * sizeof(int)); ntint = 0;
    for (int i = 0; i < nint; i++) {
        Pieza c = canonica(ints[i]); int j;
        for (j = 0; j < ntint; j++) if (!cmp4(tint[j].c, c.c)) { cant_int[j]++; break; }
        if (j == ntint) { tint[ntint] = c; cant_int[ntint] = 1; ntint++; }
    }
    orient = xmalloc(4 * ntint * sizeof(Orient)); norient = 0;
    for (int t = 0; t < ntint; t++) {
        Pieza vistas[4]; int nv = 0;
        for (int k = 0; k < 4; k++) {
            Pieza r = rotar(tint[t], k); int dup = 0;
            for (int q = 0; q < nv; q++) if (!cmp4(vistas[q].c, r.c)) dup = 1;
            if (!dup) { vistas[nv++] = r; orient[norient].tipo = t; memcpy(orient[norient].c, r.c, 4); norient++; }
        }
    }
    OW = (norient + 63) / 64; if (OW == 0) OW = 1;
    lado_color = xmalloc((size_t)4 * NC * OW * sizeof(u64));
    mascara_tipo = xmalloc((size_t)(ntint + 1) * OW * sizeof(u64));
    for (int o = 0; o < norient; o++) {
        for (int s = 0; s < 4; s++) LC(s, orient[o].c[s])[o >> 6] |= 1ULL << (o & 63);
        MT(orient[o].tipo)[o >> 6] |= 1ULL << (o & 63);
    }
    free(ints); return 1;
}

/* ------------------------------------------------------------------ conjunto de firmas (tabla hash) */
typedef struct {
    int L;              /* bytes por firma (4M) */
    int n, cap;         /* firmas guardadas */
    u8 *datos;          /* n*L */
    u8 *ejemplo;        /* n*(4M) índices de tipo de orilla del marco de ejemplo (puede ser NULL) */
    int *tabla; int tcap;
} SetFirmas;

static u64 hash_bytes(const u8 *p, int L) { u64 h = 1469598103934665603ULL; for (int i = 0; i < L; i++) { h ^= p[i]; h *= 1099511628211ULL; } return h ^ (h >> 29); }
static void set_init(SetFirmas *s, int L, int con_ejemplo) {
    memset(s, 0, sizeof(*s)); s->L = L; s->cap = 1024; s->datos = xmalloc((size_t)s->cap * (L ? L : 1));
    if (con_ejemplo) s->ejemplo = xmalloc((size_t)s->cap * (L ? L : 1));
    s->tcap = 2048; s->tabla = xmalloc(s->tcap * sizeof(int)); for (int i = 0; i < s->tcap; i++) s->tabla[i] = -1;
}
static void set_free(SetFirmas *s) { free(s->datos); free(s->ejemplo); free(s->tabla); memset(s, 0, sizeof(*s)); }
static int set_buscar(const SetFirmas *s, const u8 *f) {
    if (!s->tabla) return -1;
    u64 h = hash_bytes(f, s->L); int i = (int)(h & (u64)(s->tcap - 1));
    while (s->tabla[i] >= 0) { if (!memcmp(s->datos + (size_t)s->tabla[i] * s->L, f, s->L)) return s->tabla[i]; i = (i + 1) & (s->tcap - 1); }
    return -1;
}
static void set_rehash(SetFirmas *s) {
    free(s->tabla); s->tcap *= 2; s->tabla = xmalloc(s->tcap * sizeof(int)); for (int i = 0; i < s->tcap; i++) s->tabla[i] = -1;
    for (int k = 0; k < s->n; k++) { u64 h = hash_bytes(s->datos + (size_t)k * s->L, s->L); int i = (int)(h & (u64)(s->tcap - 1)); while (s->tabla[i] >= 0) i = (i + 1) & (s->tcap - 1); s->tabla[i] = k; }
}
/* inserta; devuelve 1 si era nueva */
static int set_insertar(SetFirmas *s, const u8 *f, const u8 *ej) {
    if (set_buscar(s, f) >= 0) return 0;
    if (s->n == s->cap) {
        s->cap *= 2; s->datos = realloc(s->datos, (size_t)s->cap * s->L);
        if (s->ejemplo) s->ejemplo = realloc(s->ejemplo, (size_t)s->cap * s->L);
        if (!s->datos || (s->ejemplo == NULL && ej)) { fprintf(stderr, "Sin memoria para firmas\n"); exit(2); }
    }
    memcpy(s->datos + (size_t)s->n * s->L, f, s->L);
    if (s->ejemplo && ej) memcpy(s->ejemplo + (size_t)s->n * s->L, ej, s->L);
    s->n++;
    if (s->n * 2 > s->tcap) set_rehash(s);
    else { u64 h = hash_bytes(f, s->L); int i = (int)(h & (u64)(s->tcap - 1)); while (s->tabla[i] >= 0) i = (i + 1) & (s->tcap - 1); s->tabla[i] = s->n - 1; }
    return 1;
}

/* ------------------------------------------------------------------ marcos de un grupo */
static Esq g4[4];                     /* grupo actual: TL, TR, BR, BL */
static int *cnt_ori;                  /* cantidades restantes durante la búsqueda */
static u8 *col_d, *col_t;             /* firma y tipos del collar en construcción (sin esquinas) */
static long long marcos_grupo, tope_marcos = 50000000LL;
static SetFirmas *set_actual;
static int demasiados = 0;

static void dfs_marco(int lado, int pos, u8 prev_b) {
    if (demasiados) return;
    if (pos == M) {                            /* fin de un lado: debe encajar con la siguiente esquina */
        Esq e = g4[(lado + 1) & 3];
        if (e.a != prev_b) return;
        if (lado == 3) {                       /* marco completo */
            marcos_grupo++;
            if (marcos_grupo > tope_marcos) { demasiados = 1; return; }
            set_insertar(set_actual, col_d, col_t);
            return;
        }
        dfs_marco(lado + 1, 0, e.b);
        return;
    }
    int *lista = ori_por_a[prev_b]; int nl = nori_por_a[prev_b];
    for (int q = 0; q < nl; q++) {
        int j = lista[q];
        if (cnt_ori[j] <= 0) continue;
        cnt_ori[j]--;
        col_d[lado * M + pos] = tori[j].d; col_t[lado * M + pos] = (u8)j;
        dfs_marco(lado, pos + 1, tori[j].b);
        cnt_ori[j]++;
    }
}

/* número de cadenas de M piezas entre dos colores (para ordenar grupos) */
static long long pasos_conteo;          /* para no quedarse contando para siempre en tableros enormes */
static double contar_cadenas(int pos, u8 prev_b, u8 fin) {
    if (++pasos_conteo > 200000000LL) return 1e30;   /* demasiadas: se da por "incontable" */
    if (pos == M) return prev_b == fin ? 1.0 : 0.0;
    double t = 0; int *lista = ori_por_a[prev_b]; int nl = nori_por_a[prev_b];
    for (int q = 0; q < nl; q++) { int j = lista[q]; if (cnt_ori[j] <= 0) continue; cnt_ori[j]--; t += contar_cadenas(pos + 1, tori[j].b, fin); cnt_ori[j]++; }
    return t;
}


/* ------------------------------------------------------------------ cadenas por lado (versión 3.1) */
typedef struct {
    int n, cap;             /* cadenas únicas (por firma + conjunto de piezas) */
    u8 *firma;              /* n*M colores hacia adentro */
    u8 *tipos;              /* n*M tipos de orilla en orden */
    SetFirmas clave;        /* clave = firma + tipos ordenados (2M bytes) para no repetir */
} Cadenas;
static Cadenas *cad_actual;
static u8 *cad_d, *cad_t;
static long long tope_cadenas = 10000000LL, cad_contadas;
static int cad_excede;

static int cmp_u8(const void *a, const void *b) { return (int)*(const u8 *)a - (int)*(const u8 *)b; }

static void dfs_cadena(int pos, u8 prev_b, u8 fin) {
    if (cad_excede) return;
    if (pos == M) {
        if (prev_b != fin) return;
        u8 clave[512]; memcpy(clave, cad_d, M); memcpy(clave + M, cad_t, M); qsort(clave + M, M, 1, cmp_u8);
        if (set_buscar(&cad_actual->clave, clave) >= 0) return;
        set_insertar(&cad_actual->clave, clave, NULL);
        Cadenas *c = cad_actual;
        if (c->n == c->cap) { c->cap = c->cap ? c->cap * 2 : 1024; c->firma = realloc(c->firma, (size_t)c->cap * M); c->tipos = realloc(c->tipos, (size_t)c->cap * M);
                              if (!c->firma || !c->tipos) { fprintf(stderr, "Sin memoria para cadenas\n"); exit(2); } }
        memcpy(c->firma + (size_t)c->n * M, cad_d, M); memcpy(c->tipos + (size_t)c->n * M, cad_t, M); c->n++;
        if (++cad_contadas > tope_cadenas) cad_excede = 1;
        return;
    }
    int *lista = ori_por_a[prev_b]; int nl = nori_por_a[prev_b];
    for (int q = 0; q < nl; q++) {
        int j = lista[q]; if (cnt_ori[j] <= 0) continue;
        cnt_ori[j]--; cad_d[pos] = tori[j].d; cad_t[pos] = (u8)j;
        dfs_cadena(pos + 1, tori[j].b, fin);
        cnt_ori[j]++;
    }
}

/* ------------------------------------------------------------------ problema del interior
 * Un "canal" es un conjunto de firmas con su filtro de bits:
 *   ruta MARCOS: 1 canal, firmas de marco completo (4M posiciones).
 *   ruta LADOS : 4 canales, uno por lado (M posiciones cada uno). */
typedef struct {
    int nch, npos[4], W[4], off[4], Wtot, nsig[4];
    u64 *bits[4];              /* [npos][NC][W] */
    int (*toca)[4][3];         /* casilla -> (lado de la casilla, canal, posición) */
    int *ntoca;
    int (*vec)[4][3]; int *nvec;
    /* ruta lados: cadenas agrupadas por firma */
    int *cini[4], *cnum[4];    /* por firma del lado: primera cadena y cuántas */
    u8 *ctipos[4];             /* cadenas ordenadas por firma: M tipos cada una */
    u8 *cfirma[4];             /* firma (M colores) de cada firma del lado */
} Problema;
#define BITS(ch, p, c) (PR.bits[ch] + (((size_t)(p) * NC + (c)) * PR.W[ch]))

static Problema PR;
static atomic_int resuelto;
static atomic_int detener;
static atomic_int terminados;          /* hilos que ya terminaron su parte */
static void dormir_ms(int ms) {
#ifdef _WIN32
    Sleep(ms);
#else
    struct timespec t = {ms / 1000, (ms % 1000) * 1000000L}; nanosleep(&t, NULL);
#endif
}
static double t_limite = 0, t_inicio = 0;

#define CACHE_BITS 16
typedef struct { int d[4]; int res; } EntradaCache;

typedef struct {
    int id, nhilos, prof_reparto;
    long long nodos, contador_ramas, comprobaciones;
    int *celdas, *cuenta;
    u64 *cand_niveles;         /* [MM+1][MM][OW] */
    u64 *vivas;                /* [MM+1][Wtot] */
    int *idx;                  /* [MM+1][Wtot] palabras no nulas */
    int *nidx, *src, *decid;   /* [MM+1][4] */
    u64 *perm;                 /* [OW] */
    int *resto;                /* [ntori] piezas de orilla restantes (factibilidad) */
    EntradaCache *cache;
    int encontrado, *solucion, firma_idx, eleccion[4];
} Hilo;

#define VIV(h, lv, ch) ((h)->vivas + (size_t)(lv) * PR.Wtot + PR.off[ch])
#define IDX(h, lv, ch) ((h)->idx + (size_t)(lv) * PR.Wtot + PR.off[ch])

/* ¿alguna firma viva (del canal ch) tiene el color c en la posición p? */
static int hay_color(const Hilo *h, int lv_src, int nid, int ch, int p, int c) {
    const u64 *b = BITS(ch, p, c), *v = VIV(h, lv_src, ch); const int *ix = IDX(h, lv_src, ch);
    for (int i = 0; i < nid; i++) { int w = ix[i]; if (b[w] & v[w]) return 1; }
    return 0;
}
static void permitido(const Hilo *h, int lv, int s, int ch, int p, u64 *out) {
    int ls = h->src[lv * 4 + ch], nid = h->nidx[lv * 4 + ch];
    memset(out, 0, OW * sizeof(u64));
    for (int c = 0; c < NC; c++)
        if (hay_color(h, ls, nid, ch, p, c)) { const u64 *m = LC(s, c); for (int w = 0; w < OW; w++) out[w] |= m[w]; }
}

/* ruta lados: ¿las cadenas de los lados ya decididos se pueden elegir sin repetir piezas? */
static int dfs_factible(Hilo *h, const int *orden, int no, int k, const int *dec, int *elec) {
    if (k == no) return 1;
    int s = orden[k], f = dec[s];
    for (int q = 0; q < PR.cnum[s][f]; q++) {
        int ci = PR.cini[s][f] + q; const u8 *t = PR.ctipos[s] + (size_t)ci * M; int ok = 1, i;
        for (i = 0; i < M; i++) { if (h->resto[t[i]] <= 0) { ok = 0; break; } h->resto[t[i]]--; }
        if (ok && dfs_factible(h, orden, no, k + 1, dec, elec)) { if (elec) elec[s] = ci; for (int j = 0; j < M; j++) h->resto[t[j]]++; return 1; }
        for (int j = 0; j < i; j++) h->resto[t[j]]++;
    }
    return 0;
}
static int factible(Hilo *h, const int *dec, int *elec) {
    int orden[4], no = 0;
    for (int s = 0; s < 4; s++) if (dec[s] >= 0) orden[no++] = s;
    if (no == 0) return 1;
    u64 hs = 0; for (int s = 0; s < 4; s++) hs = hs * 1000003ULL + (u64)(dec[s] + 1);
    EntradaCache *e = &h->cache[(hs ^ (hs >> 21)) & ((1u << CACHE_BITS) - 1)];
    if (!elec && e->res != 0 && !memcmp(e->d, dec, sizeof(e->d))) return e->res > 0;
    for (int a = 0; a < no; a++) for (int b = a + 1; b < no; b++) if (PR.cnum[orden[b]][dec[orden[b]]] < PR.cnum[orden[a]][dec[orden[a]]]) { int t = orden[a]; orden[a] = orden[b]; orden[b] = t; }
    for (int j = 0; j < ntori; j++) h->resto[j] = tori[j].cant;
    h->comprobaciones++;
    int r = dfs_factible(h, orden, no, 0, dec, elec);
    memcpy(e->d, dec, sizeof(e->d)); e->res = r ? 1 : -1;
    return r;
}

static int buscar_rec(Hilo *h, int nivel, int vacias) {
    const int MM = M * M, nch = PR.nch;
    h->nodos++;
    if ((h->nodos & 1023) == 0) {
        if (atomic_load_explicit(&resuelto, memory_order_relaxed) || atomic_load_explicit(&detener, memory_order_relaxed)) return -1;
        if (t_limite > 0 && ahora() - t_inicio > t_limite) { atomic_store(&detener, 1); return -1; }
    }
    const int *src = h->src + nivel * 4, *nidx = h->nidx + nivel * 4, *decid = h->decid + nivel * 4;
    u64 *cand = h->cand_niveles + (size_t)nivel * MM * OW;
    if (vacias == 0) {
        if (nch == 1) { const int *ix = IDX(h, src[0], 0); const u64 *v = VIV(h, src[0], 0); h->firma_idx = ix[0] * 64 + ctz64(v[ix[0]]); }
        else if (!factible(h, decid, h->eleccion)) return 0;
        memcpy(h->solucion, h->celdas, MM * sizeof(int)); h->encontrado = 1;
        return 1;
    }
    /* casilla con menos opciones; ninguna vacía sin opciones; ningún tipo sin lugar */
    int mejor = -1, mejor_v = 1 << 30;
    u64 uni[64]; memset(uni, 0, OW * sizeof(u64));
    for (int k = 0; k < MM; k++) {
        if (h->celdas[k] >= 0) continue;
        const u64 *c = cand + (size_t)k * OW; int nb = 0;
        for (int w = 0; w < OW; w++) { nb += popcount64(c[w]); uni[w] |= c[w]; }
        if (nb == 0) return 0;
        int vec = 0; for (int q = 0; q < PR.nvec[k]; q++) vec += h->celdas[PR.vec[k][q][0]] >= 0;
        int v = nb * 8 - vec;
        if (v < mejor_v) { mejor_v = v; mejor = k; }
    }
    for (int t = 0; t < ntint; t++) {
        if (h->cuenta[t] <= 0) continue;
        const u64 *mt = MT(t); int ok = 0;
        for (int w = 0; w < OW; w++) if (uni[w] & mt[w]) { ok = 1; break; }
        if (!ok) return 0;
    }
    const int k = mejor, lv = nivel + 1;
    const u64 *ck = cand + (size_t)k * OW;
    int *src_h = h->src + lv * 4, *nidx_h = h->nidx + lv * 4, *dec_h = h->decid + lv * 4;
    u64 *cand_h = h->cand_niveles + (size_t)lv * MM * OW;
    for (int w0 = 0; w0 < OW; w0++) {
        u64 bitsw = ck[w0];
        while (bitsw) {
            int o = w0 * 64 + ctz64(bitsw); bitsw &= bitsw - 1;
            const Orient *or_ = &orient[o];
            for (int ch = 0; ch < 4; ch++) { src_h[ch] = src[ch]; nidx_h[ch] = nidx[ch]; dec_h[ch] = decid[ch]; }
            int cambiados = 0, vacio = 0, dec_cambia = 0;
            /* aplicar las restricciones del marco, canal por canal */
            for (int ch = 0; ch < nch && !vacio; ch++) {
                const u64 *bs[4]; int nb = 0;
                for (int q = 0; q < PR.ntoca[k]; q++) if (PR.toca[k][q][1] == ch) bs[nb++] = BITS(ch, PR.toca[k][q][2], or_->c[PR.toca[k][q][0]]);
                if (!nb) continue;
                const u64 *v = VIV(h, src[ch], ch); const int *ix = IDX(h, src[ch], ch);
                u64 *vh = VIV(h, lv, ch); int *ixh = IDX(h, lv, ch); int n = 0;
                for (int i = 0; i < nidx[ch]; i++) {
                    int w = ix[i]; u64 x = v[w] & bs[0][w];
                    for (int j = 1; j < nb; j++) x &= bs[j][w];
                    if (x) { vh[w] = x; ixh[n++] = w; }
                }
                if (!n) { vacio = 1; break; }
                src_h[ch] = lv; nidx_h[ch] = n; cambiados |= 1 << ch;
                if (nch == 4 && n == 1 && !(vh[ixh[0]] & (vh[ixh[0]] - 1))) {
                    int f = ixh[0] * 64 + ctz64(vh[ixh[0]]);
                    if (dec_h[ch] != f) { dec_h[ch] = f; dec_cambia = 1; }
                }
            }
            if (vacio) continue;
            if (dec_cambia && !factible(h, dec_h, NULL)) continue;
            if (h->nhilos > 1 && nivel == h->prof_reparto) {
                long long r = h->contador_ramas++;
                if (r % h->nhilos != h->id) continue;
            }
            int t = or_->tipo;
            h->celdas[k] = o; h->cuenta[t]--;
            memcpy(cand_h, cand, (size_t)MM * OW * sizeof(u64));
            if (h->cuenta[t] == 0) { const u64 *mt = MT(t); for (int kk = 0; kk < MM; kk++) { u64 *c = cand_h + (size_t)kk * OW; for (int w = 0; w < OW; w++) c[w] &= ~mt[w]; } }
            for (int q = 0; q < PR.nvec[k]; q++) {
                int k2 = PR.vec[k][q][0], s2 = PR.vec[k][q][1], s1 = PR.vec[k][q][2];
                if (h->celdas[k2] >= 0) continue;
                const u64 *m = LC(s2, or_->c[s1]); u64 *c = cand_h + (size_t)k2 * OW;
                for (int w = 0; w < OW; w++) c[w] &= m[w];
            }
            if (cambiados) {
                for (int kk = 0; kk < MM; kk++) {
                    if (h->celdas[kk] >= 0) continue;
                    u64 *c = cand_h + (size_t)kk * OW;
                    for (int q = 0; q < PR.ntoca[kk]; q++) {
                        int ch = PR.toca[kk][q][1]; if (!(cambiados & (1 << ch))) continue;
                        permitido(h, lv, PR.toca[kk][q][0], ch, PR.toca[kk][q][2], h->perm);
                        for (int w = 0; w < OW; w++) c[w] &= h->perm[w];
                    }
                }
            }
            int r = buscar_rec(h, lv, vacias - 1);
            h->celdas[k] = -1; h->cuenta[t]++;
            if (r != 0) return r;
        }
    }
    return 0;
}

static void *hilo_main(void *arg) {
    Hilo *h = arg;
    int r = buscar_rec(h, 0, M * M);
    if (r == 1) atomic_store(&resuelto, 1);
    atomic_fetch_add(&terminados, 1);
    return NULL;
}

static void preparar_vecinos(void) {
    const int MM = M * M;
    PR.toca = xmalloc(MM * sizeof(*PR.toca)); PR.ntoca = xmalloc(MM * sizeof(int));
    PR.vec = xmalloc(MM * sizeof(*PR.vec)); PR.nvec = xmalloc(MM * sizeof(int));
    for (int i = 0; i < M; i++) for (int j = 0; j < M; j++) {
        int k = i * M + j, v = 0;
        if (i > 0) { PR.vec[k][v][0] = k - M; PR.vec[k][v][1] = 2; PR.vec[k][v][2] = 0; v++; }
        if (i < M - 1) { PR.vec[k][v][0] = k + M; PR.vec[k][v][1] = 0; PR.vec[k][v][2] = 2; v++; }
        if (j > 0) { PR.vec[k][v][0] = k - 1; PR.vec[k][v][1] = 1; PR.vec[k][v][2] = 3; v++; }
        if (j < M - 1) { PR.vec[k][v][0] = k + 1; PR.vec[k][v][1] = 3; PR.vec[k][v][2] = 1; v++; }
        PR.nvec[k] = v;
    }
}

/* posiciones del marco que toca cada casilla; lados=1 -> canal por lado */
static void preparar_toca(int lados) {
    for (int i = 0; i < M; i++) for (int j = 0; j < M; j++) {
        int k = i * M + j, n = 0;
        int ls[4], qs[4];
        if (i == 0) { ls[n] = 0; qs[n] = j; n++; }
        if (j == M - 1) { ls[n] = 1; qs[n] = i; n++; }
        if (i == M - 1) { ls[n] = 2; qs[n] = M - 1 - j; n++; }
        if (j == 0) { ls[n] = 3; qs[n] = M - 1 - i; n++; }
        for (int q = 0; q < n; q++) {
            PR.toca[k][q][0] = ls[q];
            PR.toca[k][q][1] = lados ? ls[q] : 0;
            PR.toca[k][q][2] = lados ? qs[q] : ls[q] * M + qs[q];
        }
        PR.ntoca[k] = n;
    }
}

static void liberar_canales(void) {
    for (int ch = 0; ch < 4; ch++) { free(PR.bits[ch]); PR.bits[ch] = NULL; free(PR.cini[ch]); free(PR.cnum[ch]); free(PR.ctipos[ch]); free(PR.cfirma[ch]); PR.cini[ch] = PR.cnum[ch] = NULL; PR.ctipos[ch] = PR.cfirma[ch] = NULL; }
}

static void preparar_marcos(const SetFirmas *s) {
    liberar_canales(); preparar_toca(0);
    PR.nch = 1; PR.npos[0] = 4 * M; PR.nsig[0] = s->n; PR.W[0] = (s->n + 63) / 64; PR.off[0] = 0; PR.Wtot = PR.W[0];
    PR.bits[0] = xmalloc((size_t)PR.npos[0] * NC * PR.W[0] * sizeof(u64));
    for (int i = 0; i < s->n; i++) { const u8 *f = s->datos + (size_t)i * s->L; for (int p = 0; p < 4 * M; p++) BITS(0, p, f[p])[i >> 6] |= 1ULL << (i & 63); }
}

/* ordena las cadenas de un lado por firma y arma los conjuntos de bits */
static Cadenas *orden_cad; static int cmp_idx_firma(const void *a, const void *b) {
    int x = *(const int *)a, y = *(const int *)b;
    return memcmp(orden_cad->firma + (size_t)x * M, orden_cad->firma + (size_t)y * M, M);
}
static void preparar_lados(Cadenas cads[4]) {
    liberar_canales(); preparar_toca(1);
    PR.nch = 4; PR.Wtot = 0;
    for (int s = 0; s < 4; s++) {
        Cadenas *c = &cads[s];
        int *ord = xmalloc((c->n + 1) * sizeof(int)); for (int i = 0; i < c->n; i++) ord[i] = i;
        orden_cad = c; qsort(ord, c->n, sizeof(int), cmp_idx_firma);
        PR.ctipos[s] = xmalloc((size_t)(c->n + 1) * M);
        PR.cini[s] = xmalloc((c->n + 1) * sizeof(int)); PR.cnum[s] = xmalloc((c->n + 1) * sizeof(int));
        PR.cfirma[s] = xmalloc((size_t)(c->n + 1) * M);
        int nf = 0;
        for (int i = 0; i < c->n; i++) {
            const u8 *f = c->firma + (size_t)ord[i] * M;
            memcpy(PR.ctipos[s] + (size_t)i * M, c->tipos + (size_t)ord[i] * M, M);
            if (nf == 0 || memcmp(PR.cfirma[s] + (size_t)(nf - 1) * M, f, M)) { memcpy(PR.cfirma[s] + (size_t)nf * M, f, M); PR.cini[s][nf] = i; PR.cnum[s][nf] = 0; nf++; }
            PR.cnum[s][nf - 1]++;
        }
        free(ord);
        PR.npos[s] = M; PR.nsig[s] = nf; PR.W[s] = (nf + 63) / 64; if (!PR.W[s]) PR.W[s] = 1;
        PR.off[s] = PR.Wtot; PR.Wtot += PR.W[s];
        PR.bits[s] = xmalloc((size_t)M * NC * PR.W[s] * sizeof(u64));
        for (int i = 0; i < nf; i++) { const u8 *f = PR.cfirma[s] + (size_t)i * M; for (int p = 0; p < M; p++) BITS(s, p, f[p])[i >> 6] |= 1ULL << (i & 63); }
    }
}

/* ------------------------------------------------------------------ armado y verificación */
static Pieza pieza_esquina(Esq e) { Pieza p = {{0, e.b, e.a, 0}}; return p; }
static Pieza pieza_orilla(int j) { Pieza p = {{0, tori[j].b, tori[j].d, tori[j].a}}; return p; }

/* tipos_lado[s] = M tipos de orilla del lado s en orden de recorrido */
static void armar(const u8 *tipos_lado[4], const int *sol, Pieza *grid) {
    for (int lado = 0; lado < 4; lado++) {
        int r, c;
        if (lado == 0) { r = 0; c = 0; } else if (lado == 1) { r = 0; c = N - 1; } else if (lado == 2) { r = N - 1; c = N - 1; } else { r = N - 1; c = 0; }
        grid[r * N + c] = rotar(pieza_esquina(g4[lado]), lado);
        for (int q = 0; q < M; q++) {
            int j = tipos_lado[lado][q];
            if (lado == 0) { r = 0; c = 1 + q; } else if (lado == 1) { r = 1 + q; c = N - 1; }
            else if (lado == 2) { r = N - 1; c = N - 2 - q; } else { r = N - 2 - q; c = 0; }
            grid[r * N + c] = rotar(pieza_orilla(j), lado);
        }
    }
    for (int k = 0; k < M * M; k++) { int i = k / M, j = k % M; Pieza p; memcpy(p.c, orient[sol[k]].c, 4); grid[(i + 1) * N + (j + 1)] = p; }
}

static int cmp_pieza(const void *a, const void *b) { return memcmp(a, b, 4); }
static int verificar(const Pieza *grid) {
    for (int r = 0; r < N; r++) for (int c = 0; c < N; c++) {
        Pieza p = grid[r * N + c];
        if ((p.c[0] == 0) != (r == 0) || (p.c[2] == 0) != (r == N - 1) || (p.c[3] == 0) != (c == 0) || (p.c[1] == 0) != (c == N - 1)) return 0;
        if (c < N - 1 && p.c[1] != grid[r * N + c + 1].c[3]) return 0;
        if (r < N - 1 && p.c[2] != grid[(r + 1) * N + c].c[0]) return 0;
    }
    Pieza *a = xmalloc(NP * sizeof(Pieza)), *b = xmalloc(NP * sizeof(Pieza));
    for (int i = 0; i < NP; i++) { a[i] = canonica(grid[i]); b[i] = canonica(piezas[i]); }
    qsort(a, NP, sizeof(Pieza), cmp_pieza); qsort(b, NP, sizeof(Pieza), cmp_pieza);
    int ok = memcmp(a, b, NP * sizeof(Pieza)) == 0; free(a); free(b); return ok;
}

/* ------------------------------------------------------------------ búsqueda de un grupo con todos los hilos */
static int buscar_interior(int hilos, int prof, long long *nodos_out, long long *compr_out, Pieza *grid, const SetFirmas *sg) {
    const int MM = M * M;
    Hilo *hs = xmalloc(hilos * sizeof(Hilo)); pthread_t *th = xmalloc(hilos * sizeof(pthread_t));
    atomic_store(&resuelto, 0); atomic_store(&terminados, 0);
    for (int x = 0; x < hilos; x++) {
        Hilo *h = &hs[x]; h->id = x; h->nhilos = hilos; h->prof_reparto = prof;
        h->celdas = xmalloc(MM * sizeof(int)); for (int k = 0; k < MM; k++) h->celdas[k] = -1;
        h->cuenta = xmalloc(ntint * sizeof(int)); memcpy(h->cuenta, cant_int, ntint * sizeof(int));
        h->cand_niveles = xmalloc((size_t)(MM + 1) * MM * OW * sizeof(u64));
        h->vivas = xmalloc((size_t)(MM + 1) * PR.Wtot * sizeof(u64));
        h->idx = xmalloc((size_t)(MM + 1) * PR.Wtot * sizeof(int));
        h->nidx = xmalloc((MM + 1) * 4 * sizeof(int)); h->src = xmalloc((MM + 1) * 4 * sizeof(int)); h->decid = xmalloc((MM + 1) * 4 * sizeof(int));
        h->perm = xmalloc(OW * sizeof(u64)); h->resto = xmalloc((ntori + 1) * sizeof(int));
        h->cache = xmalloc(((size_t)1 << CACHE_BITS) * sizeof(EntradaCache));
        h->solucion = xmalloc(MM * sizeof(int));
        for (int ch = 0; ch < 4; ch++) { h->src[ch] = 0; h->nidx[ch] = 0; h->decid[ch] = -1; }
        for (int ch = 0; ch < PR.nch; ch++) {
            u64 *v = VIV(h, 0, ch); int *ix = IDX(h, 0, ch);
            for (int w = 0; w < PR.W[ch]; w++) { v[w] = ~0ULL; ix[w] = w; }
            if (PR.nsig[ch] % 64) v[PR.W[ch] - 1] = (1ULL << (PR.nsig[ch] % 64)) - 1;
            h->nidx[ch] = PR.nsig[ch] ? PR.W[ch] : 0;
            if (PR.nch == 4 && PR.nsig[ch] == 1) h->decid[ch] = 0;
        }
        for (int k = 0; k < MM; k++) {
            u64 *c = h->cand_niveles + (size_t)k * OW;
            for (int o = 0; o < norient; o++) c[o >> 6] |= 1ULL << (o & 63);
            for (int q = 0; q < PR.ntoca[k]; q++) {
                permitido(h, 0, PR.toca[k][q][0], PR.toca[k][q][1], PR.toca[k][q][2], h->perm);
                for (int w = 0; w < OW; w++) c[w] &= h->perm[w];
            }
        }
        pthread_create(&th[x], NULL, hilo_main, h);
    }
    /* el hilo principal solo vigila: informa el avance cada 2 s (no frena la búsqueda) */
    double ultimo = ahora();
    while (atomic_load(&terminados) < hilos) {
        dormir_ms(5);
        if (!silencioso && ahora() - ultimo >= 2.0) {
            long long nd = 0; for (int x = 0; x < hilos; x++) nd += hs[x].nodos;
            printf("    ... %.1f s, %lld nodos del interior\n", ahora() - t_inicio, nd);
            ultimo = ahora();
        }
    }
    int gan = -1;
    for (int x = 0; x < hilos; x++) { pthread_join(th[x], NULL); *nodos_out += hs[x].nodos; *compr_out += hs[x].comprobaciones; if (hs[x].encontrado && gan < 0) gan = x; }
    int res = 0;
    if (gan >= 0) {
        const u8 *tl[4];
        if (PR.nch == 1) { const u8 *ej = sg->ejemplo + (size_t)hs[gan].firma_idx * sg->L; for (int s = 0; s < 4; s++) tl[s] = ej + s * M; }
        else for (int s = 0; s < 4; s++) tl[s] = PR.ctipos[s] + (size_t)hs[gan].eleccion[s] * M;
        armar(tl, hs[gan].solucion, grid);
        res = verificar(grid) ? 1 : -1;
    }
    for (int x = 0; x < hilos; x++) { Hilo *h = &hs[x]; free(h->celdas); free(h->cuenta); free(h->cand_niveles); free(h->vivas); free(h->idx); free(h->nidx); free(h->src); free(h->decid); free(h->perm); free(h->resto); free(h->cache); free(h->solucion); }
    free(hs); free(th);
    return res;
}


/* ==================================================================================
 * VERSIÓN 4 — RUTA UNIFICADA
 *
 * Cuando un grupo tiene demasiados marcos, en vez de guardar listas de cadenas por lado:
 *  (a) cada lado del marco se representa como un MAPA de estados (posición, color de orilla
 *      con el que se llega). Las millones de cadenas son caminos en ese mapa y nunca se
 *      escriben. El mapa elimina, en cada paso, las piezas de orilla que ya no pueden formar
 *      ningún camino completo de esquina a esquina ("restricción regular").
 *  (b) las piezas de orilla se colocan en la MISMA búsqueda que las del interior, siempre la
 *      casilla con menos opciones primero. La regla de no repetir piezas se cumple al instante
 *      (cada pieza colocada se descuenta), en vez de revisarse tarde.
 *  (c) conteo temprano: si una pieza de orilla tiene más copias por colocar que casillas de
 *      orilla donde todavía cabe, se retrocede de inmediato.
 *  (d) piezas fijas (pistas), por ejemplo la pieza del centro del Eternity II.
 * ================================================================================== */

typedef struct { int r, c, giros; Pieza orig, canon; } Pista;   /* giros = -1: cualquier orientación */
static Pista pistas[64]; static int npistas = 0;

static int orden_peso = 0;            /* 0 = menos opciones; >0 = prioriza casillas con vecinas puestas */
static int NN, EW, DW;                /* celdas, palabras para orillas, palabras por dominio */
static u8 *colB;                      /* [4][ntori][4] colores de la orilla t puesta en el lado s */
static u64 *BMk;                      /* [4][4][NC][EW] orillas del lado s con color c en la dirección dir */
static int *tipoc, *ladoc, *posc;     /* por celda: 0 interior, 1 orilla, 2 esquina; lado; posición */
static int (*vec4)[4];                /* vecino en cada dirección (N,E,S,O) o -1 */
static int *celda_lado;               /* [4][M] índice de celda de cada posición de cada lado */
static int *var_celdas; static int nvar;  /* celdas que se deciden (todas menos las esquinas) */
#define COLB(s, t, d) colB[(((s) * ntori + (t)) * 4) + (d)]
#define BMASK(s, d, c) (BMk + ((((size_t)(s) * 4 + (d)) * NC + (c)) * EW))
static const int dr[4] = {-1, 0, 1, 0}, dc[4] = {0, 1, 0, -1};

static void preparar_unificado(void) {
    NN = N * N; EW = (ntori + 63) / 64; if (!EW) EW = 1; DW = OW > EW ? OW : EW;
    colB = xmalloc((size_t)4 * ntori * 4); BMk = xmalloc((size_t)16 * NC * EW * sizeof(u64));
    for (int s = 0; s < 4; s++) for (int t = 0; t < ntori; t++) {
        Pieza p = rotar(pieza_orilla(t), s);
        for (int d = 0; d < 4; d++) { COLB(s, t, d) = p.c[d]; BMASK(s, d, p.c[d])[t >> 6] |= 1ULL << (t & 63); }
    }
    tipoc = xmalloc(NN * sizeof(int)); ladoc = xmalloc(NN * sizeof(int)); posc = xmalloc(NN * sizeof(int));
    vec4 = xmalloc(NN * sizeof(*vec4)); celda_lado = xmalloc(4 * M * sizeof(int)); var_celdas = xmalloc(NN * sizeof(int));
    for (int r = 0; r < N; r++) for (int c = 0; c < N; c++) {
        int k = r * N + c, bord = (r == 0) + (r == N - 1) + (c == 0) + (c == N - 1);
        tipoc[k] = bord == 0 ? 0 : (bord == 1 ? 1 : 2); ladoc[k] = -1; posc[k] = -1;
        for (int d = 0; d < 4; d++) { int r2 = r + dr[d], c2 = c + dc[d]; vec4[k][d] = (r2 < 0 || r2 >= N || c2 < 0 || c2 >= N) ? -1 : r2 * N + c2; }
    }
    for (int q = 0; q < M; q++) {
        int ks[4] = {0 * N + (1 + q), (1 + q) * N + (N - 1), (N - 1) * N + (N - 2 - q), (N - 2 - q) * N + 0};
        for (int s = 0; s < 4; s++) { ladoc[ks[s]] = s; posc[ks[s]] = q; celda_lado[s * M + q] = ks[s]; }
    }
    nvar = 0; for (int k = 0; k < NN; k++) if (tipoc[k] != 2) var_celdas[nvar++] = k;
}

static inline const u64 *mascara_celda(int k, int dir, int color) { return tipoc[k] == 0 ? LC(dir, color) : BMASK(ladoc[k], dir, color); }
static inline int palabras(int k) { return tipoc[k] == 0 ? OW : EW; }
static inline int color_opcion(int k, int o, int dir) { return tipoc[k] == 0 ? orient[o].c[dir] : COLB(ladoc[k], o, dir); }

typedef struct {
    int id, nhilos, prof_reparto;
    long long nodos, contador_ramas; int maxprof;
    u64 *dom;               /* [nvar+1][NN][DW] */
    int *puesto;            /* opción colocada en cada celda o -1 */
    int *cint, *cori;       /* piezas restantes por tipo interior y de orilla */
    int encontrado, *solucion;
    int orden;              /* v4.3: 0 menos opciones, 1 interior primero (por hilo: portafolio) */
} HiloU;
#define DOM(h, lv, k) ((h)->dom + (((size_t)(lv) * NN + (k)) * DW))

/* mapa de un lado: elimina de las casillas del lado las piezas que no forman ningún camino
 * completo de esquina a esquina. Devuelve 0 si el lado queda imposible. Marca en *cambio las
 * posiciones cuyas opciones se redujeron. */
static int mapa_lado(HiloU *h, int lv, int s, unsigned *cambio) {
    u64 F[65]; u64 B[65];
    F[0] = 1ULL << g4[s].b;
    for (int q = 0; q < M; q++) {
        const u64 *D = DOM(h, lv, celda_lado[s * M + q]); u64 f = 0;
        for (int w = 0; w < EW; w++) { u64 x = D[w]; while (x) { int t = w * 64 + ctz64(x); x &= x - 1; if ((F[q] >> tori[t].a) & 1) f |= 1ULL << tori[t].b; } }
        F[q + 1] = f; if (!f) return 0;
    }
    B[M] = 1ULL << g4[(s + 1) & 3].a;
    if (!(F[M] & B[M])) return 0;
    for (int q = M - 1; q >= 0; q--) {
        u64 *D = DOM(h, lv, celda_lado[s * M + q]); u64 b = 0; int vacio = 1, cambia = 0;
        for (int w = 0; w < EW; w++) {
            u64 x = D[w], nuevo = 0;
            while (x) { int t = w * 64 + ctz64(x); x &= x - 1;
                if (((F[q] >> tori[t].a) & 1) && ((B[q + 1] >> tori[t].b) & 1)) { nuevo |= 1ULL << (t & 63); b |= 1ULL << tori[t].a; } }
            if (nuevo != D[w]) { cambia = 1; D[w] = nuevo; }
            if (nuevo) vacio = 0;
        }
        if (vacio) return 0;
        if (cambia) *cambio |= 1u << q;
        B[q] = b;
    }
    return 1;
}

/* después de reducir casillas de orilla: su vecina del interior solo acepta los colores posibles */
static int orilla_a_interior(HiloU *h, int lv, int s, unsigned cambio) {
    int din = (2 + s) & 3, dout = s;          /* dirección hacia adentro desde la orilla, y la opuesta */
    for (int q = 0; q < M; q++) {
        if (!((cambio >> q) & 1)) continue;
        int kb = celda_lado[s * M + q], ki = vec4[kb][din];
        if (ki < 0 || tipoc[ki] != 0 || h->puesto[ki] >= 0) continue;
        u64 cols = 0; const u64 *D = DOM(h, lv, kb);
        for (int w = 0; w < EW; w++) { u64 x = D[w]; while (x) { int t = w * 64 + ctz64(x); x &= x - 1; cols |= 1ULL << COLB(s, t, din); } }
        u64 m[64]; memset(m, 0, OW * sizeof(u64));
        while (cols) { int c = ctz64(cols); cols &= cols - 1; const u64 *lc = LC(dout, c); for (int w = 0; w < OW; w++) m[w] |= lc[w]; }
        u64 *Di = DOM(h, lv, ki); u64 any = 0;
        for (int w = 0; w < OW; w++) { Di[w] &= m[w]; any |= Di[w]; }
        if (!any) return 0;
    }
    return 1;
}

static int propagar_lados(HiloU *h, int lv, unsigned lados) {
    for (int s = 0; s < 4; s++) {
        if (!((lados >> s) & 1)) continue;
        unsigned cambio = 0;
        if (!mapa_lado(h, lv, s, &cambio)) return 0;
        if (cambio && !orilla_a_interior(h, lv, s, cambio)) return 0;
    }
    return 1;
}

/* ================================================================== versión 4.2: MAPA DEL SEGUNDO ANILLO
 * Idea de Carlos: el marco interior (segundo anillo) también filtra al marco exterior.
 * No se enumeran los marcos interiores (serían demasiados); en cambio, cada lado del segundo
 * anillo se trata como un mapa, igual que los lados de la orilla:
 *   - pasada de ida y de vuelta sobre la fila (o columna): se borran las orientaciones que no
 *     forman ninguna fila completa compatible;
 *   - los colores que el segundo anillo aún puede mostrar hacia la orilla restringen a las piezas
 *     de orilla vecinas; si cambian, se vuelve a pasar el mapa de ese lado de la orilla.
 * anillo2 = 0 apagado, 1 solo el segundo anillo, 2 todas las filas y columnas interiores;
 * por defecto (-1) se elige solo: 0 hasta 8x8 (ahí cuesta más de lo que ahorra), 2 desde 9x9. */
static int anillo2 = -1;   /* -1 = automático. v4.3: 2 siempre (desde 5x5); las pruebas de Carlos con 12 hilos lo justifican */
static int nlin = 0, lin_len[256], lin_dir[256], *lin_cel[256];

static void preparar_anillo2(void) {
    nlin = 0;
    if (anillo2 < 0) anillo2 = 2;
    if (anillo2 == 0 || N < 5) return;
    int m = N - 2;
    for (int r = 1; r <= N - 2; r++) {
        if (anillo2 == 1 && r != 1 && r != N - 2) continue;
        lin_len[nlin] = m; lin_dir[nlin] = 1; lin_cel[nlin] = xmalloc(m * sizeof(int));
        for (int j = 0; j < m; j++) lin_cel[nlin][j] = r * N + 1 + j;
        nlin++;
    }
    for (int c = 1; c <= N - 2; c++) {
        if (anillo2 == 1 && c != 1 && c != N - 2) continue;
        lin_len[nlin] = m; lin_dir[nlin] = 2; lin_cel[nlin] = xmalloc(m * sizeof(int));
        for (int j = 0; j < m; j++) lin_cel[nlin][j] = (1 + j) * N + c;
        nlin++;
    }
}

/* OR de las máscaras de orientación que muestran algún color de 'cols' en la dirección d */
static inline void union_lc(u64 *out, int d, u64 cols) {
    memset(out, 0, OW * sizeof(u64));
    while (cols) { int c = ctz64(cols); cols &= cols - 1; const u64 *lc = LC(d, c); for (int w = 0; w < OW; w++) out[w] |= lc[w]; }
}
static inline u64 colores_en(const u64 *m, int d) {
    int nb = 0; for (int w = 0; w < OW; w++) nb += popcount64(m[w]);
    u64 cols = 0;
    if (nb <= 2 * NC) {       /* pocas opciones: se leen una por una */
        for (int w = 0; w < OW; w++) { u64 x = m[w]; while (x) { int o = w * 64 + ctz64(x); x &= x - 1; cols |= 1ULL << orient[o].c[d]; } }
        return cols;
    }
    for (int c = 0; c < NC; c++) { const u64 *lc = LC(d, c); for (int w = 0; w < OW; w++) if (m[w] & lc[w]) { cols |= 1ULL << c; break; } }
    return cols;
}

static int punto_fijo = 0, forzar = 0;
static int a2_cambio;      /* el último pase cambió alguna casilla (por hilo no hace falta: se usa justo después) */
static int propagar_anillo2_(HiloU *h, int lv, int *cambio_out);
static int propagar_anillo2(HiloU *h, int lv) {
    int c = 0, r = propagar_anillo2_(h, lv, &c);
    for (int it = 0; r && c && punto_fijo && it < 50; it++) { c = 0; r = propagar_anillo2_(h, lv, &c); }
    (void)a2_cambio; return r;
}
static int propagar_anillo2_(HiloU *h, int lv, int *cambio_out) {
    if (!nlin) return 1;
    const u64 todos = NC >= 64 ? ~0ULL : ((1ULL << NC) - 1);
    unsigned lados = 0;
    u64 U[64];
    for (int L = 0; L < nlin; L++) {
        int len = lin_len[L], d = lin_dir[L], b = (d + 2) & 3; const int *cel = lin_cel[L];
        if (lv > 0) {   /* si ninguna casilla de la línea cambió desde el nivel anterior, ya está al día */
            int cambio = 0;
            for (int j = 0; j < len && !cambio; j++) if (memcmp(DOM(h, lv, cel[j]), DOM(h, lv - 1, cel[j]), OW * sizeof(u64))) cambio = 1;
            if (!cambio) continue;
        }
        u64 A[len * OW];
        u64 F = todos;
        for (int j = 0; j < len; j++) {
            const u64 *D = DOM(h, lv, cel[j]); u64 *a = A + (size_t)j * OW; u64 any = 0;
            if (F == todos) { for (int w = 0; w < OW; w++) { a[w] = D[w]; any |= a[w]; } }
            else { union_lc(U, b, F); for (int w = 0; w < OW; w++) { a[w] = D[w] & U[w]; any |= a[w]; } }
            if (!any) return 0;
            F = colores_en(a, d);
        }
        u64 B = todos;
        for (int j = len - 1; j >= 0; j--) {
            u64 *a = A + (size_t)j * OW; u64 any = 0;
            if (B != todos) { union_lc(U, d, B); for (int w = 0; w < OW; w++) a[w] &= U[w]; }
            for (int w = 0; w < OW; w++) any |= a[w];
            if (!any) return 0;
            u64 *D = DOM(h, lv, cel[j]);
            if (memcmp(D, a, OW * sizeof(u64))) { *cambio_out = 1; memcpy(D, a, OW * sizeof(u64)); }
            B = colores_en(a, b);
            /* vecinas de orilla (solo el segundo anillo las tiene) */
            for (int e = 0; e < 4; e++) {
                if (e == d || e == b) continue;
                int k2 = vec4[cel[j]][e]; if (k2 < 0 || tipoc[k2] != 1 || h->puesto[k2] >= 0) continue;
                u64 cols = colores_en(a, e), m[64]; int s = ladoc[k2], fe = (e + 2) & 3;
                memset(m, 0, EW * sizeof(u64));
                while (cols) { int c = ctz64(cols); cols &= cols - 1; const u64 *bm = BMASK(s, fe, c); for (int w = 0; w < EW; w++) m[w] |= bm[w]; }
                u64 *D2 = DOM(h, lv, k2); u64 any2 = 0; int cambia = 0;
                for (int w = 0; w < EW; w++) { u64 nv = D2[w] & m[w]; if (nv != D2[w]) cambia = 1; D2[w] = nv; any2 |= nv; }
                if (!any2) return 0;
                if (cambia) { lados |= 1u << s; *cambio_out = 1; }
            }
        }
    }
    if (lados) return propagar_lados(h, lv, lados);
    return 1;
}

static int rec_u(HiloU *h, int lv, int vacias) {
    h->nodos++;
    if (lv > h->maxprof) h->maxprof = lv;
    if ((h->nodos & 1023) == 0) {
        if (atomic_load_explicit(&resuelto, memory_order_relaxed) || atomic_load_explicit(&detener, memory_order_relaxed)) return -1;
        if (t_limite > 0 && ahora() - t_inicio > t_limite) { atomic_store(&detener, 1); return -1; }
    }
    if (vacias == 0) { memcpy(h->solucion, h->puesto, NN * sizeof(int)); h->encontrado = 1; return 1; }

    /* casilla con menos opciones (orilla o interior) */
    int mejor = -1, mejor_v = 1 << 30;
    u64 uni_i[64], uni_b[64]; memset(uni_i, 0, OW * sizeof(u64)); memset(uni_b, 0, EW * sizeof(u64));
    int *cabe = NULL; int cabe_local[512];
    if (ntori <= 512) { cabe = cabe_local; memset(cabe, 0, ntori * sizeof(int)); }
    for (int i = 0; i < nvar; i++) {
        int k = var_celdas[i]; if (h->puesto[k] >= 0) continue;
        const u64 *D = DOM(h, lv, k); int nw = palabras(k), nb = 0;
        if (tipoc[k] == 0) for (int w = 0; w < nw; w++) { nb += popcount64(D[w]); uni_i[w] |= D[w]; }
        else for (int w = 0; w < nw; w++) { nb += popcount64(D[w]); uni_b[w] |= D[w];
                 if (cabe) { u64 x = D[w]; while (x) { cabe[w * 64 + ctz64(x)]++; x &= x - 1; } } }
        if (nb == 0) return 0;
        int vec = 0; for (int d = 0; d < 4; d++) { int k2 = vec4[k][d]; if (k2 >= 0 && (tipoc[k2] == 2 || h->puesto[k2] >= 0)) vec++; }
        int v = orden_peso ? (nb < 2 ? nb : nb + (4 - vec) * orden_peso) : nb * 8 - vec;
        if (v < mejor_v) { mejor_v = v; mejor = k; }
    }
    for (int t = 0; t < ntint; t++) if (h->cint[t] > 0) { const u64 *mt = MT(t); int ok = 0; for (int w = 0; w < OW; w++) if (uni_i[w] & mt[w]) { ok = 1; break; } if (!ok) return 0; }
    for (int t = 0; t < ntori; t++) if (h->cori[t] > 0) {
        if (!((uni_b[t >> 6] >> (t & 63)) & 1)) return 0;
        if (cabe && cabe[t] < h->cori[t]) return 0;      /* (c) más copias que casillas donde cabe */
    }

    const int k = mejor; const int nw = palabras(k);
    const u64 *Dk = DOM(h, lv, k);
    for (int w0 = 0; w0 < nw; w0++) {
        u64 bits = Dk[w0];
        while (bits) {
            int o = w0 * 64 + ctz64(bits); bits &= bits - 1;
            if (h->nhilos > 1 && lv == h->prof_reparto) { long long r = h->contador_ramas++; if (r % h->nhilos != h->id) continue; }
            u64 *H = DOM(h, lv + 1, 0);
            memcpy(H, DOM(h, lv, 0), (size_t)NN * DW * sizeof(u64));
            u64 *Hk = DOM(h, lv + 1, k); memset(Hk, 0, DW * sizeof(u64)); Hk[o >> 6] = 1ULL << (o & 63);
            h->puesto[k] = o;
            int esint = tipoc[k] == 0, t = esint ? orient[o].tipo : o, ok = 1;
            unsigned lados = 0;
            if (esint) h->cint[t]--; else { h->cori[t]--; lados |= 1u << ladoc[k]; }
            /* pieza agotada: se quita de todas las casillas */
            if (esint && h->cint[t] == 0) {
                const u64 *mt = MT(t);
                for (int i = 0; i < nvar; i++) { int kk = var_celdas[i]; if (tipoc[kk] != 0 || h->puesto[kk] >= 0) continue; u64 *D = DOM(h, lv + 1, kk); for (int w = 0; w < OW; w++) D[w] &= ~mt[w]; }
            } else if (!esint && h->cori[t] == 0) {
                for (int i = 0; i < nvar; i++) { int kk = var_celdas[i]; if (tipoc[kk] != 1 || h->puesto[kk] >= 0) continue; DOM(h, lv + 1, kk)[t >> 6] &= ~(1ULL << (t & 63)); lados |= 1u << ladoc[kk]; }
            }
            /* vecinas: deben coincidir en el color del lado común */
            for (int d = 0; d < 4 && ok; d++) {
                int k2 = vec4[k][d]; if (k2 < 0 || tipoc[k2] == 2 || h->puesto[k2] >= 0) continue;
                const u64 *m = mascara_celda(k2, (d + 2) & 3, color_opcion(k, o, d));
                u64 *D2 = DOM(h, lv + 1, k2); u64 any = 0; int n2 = palabras(k2);
                for (int w = 0; w < n2; w++) { D2[w] &= m[w]; any |= D2[w]; }
                if (!any) ok = 0;
                if (tipoc[k2] == 1) lados |= 1u << ladoc[k2];
            }
            if (ok && lados) ok = propagar_lados(h, lv + 1, lados);
            int r = ok ? rec_u(h, lv + 1, vacias - 1) : 0;
            h->puesto[k] = -1;
            if (esint) h->cint[t]++; else h->cori[t]++;
            if (r != 0) return r;
        }
    }
    return 0;
}

static void *hilo_u(void *arg) {
    HiloU *h = arg; int vac = 0;
    for (int i = 0; i < nvar; i++) if (h->puesto[var_celdas[i]] < 0) vac++;
    int r = rec_u(h, 0, vac);
    if (r == 1) atomic_store(&resuelto, 1);
    atomic_fetch_add(&terminados, 1);
    return NULL;
}

/* prepara las opciones iniciales de un grupo (esquinas del grupo, pistas, mapas).
 * Devuelve 0 si el grupo es imposible desde el principio. */
static int iniciar_grupo_u(HiloU *h) {
    for (int k = 0; k < NN; k++) {
        u64 *D = DOM(h, 0, k); memset(D, 0, DW * sizeof(u64)); h->puesto[k] = -1;
        if (tipoc[k] == 0) for (int o = 0; o < norient; o++) D[o >> 6] |= 1ULL << (o & 63);
        else if (tipoc[k] == 1) for (int t = 0; t < ntori; t++) D[t >> 6] |= 1ULL << (t & 63);
    }
    memcpy(h->cint, cant_int, ntint * sizeof(int));
    for (int t = 0; t < ntori; t++) h->cori[t] = tori[t].cant;
    /* esquinas del grupo como piezas ya puestas */
    Pieza pe[4]; int ke[4] = {0, N - 1, NN - 1, (N - 1) * N};
    for (int s = 0; s < 4; s++) pe[s] = rotar(pieza_esquina(g4[s]), s);
    /* pistas */
    for (int i = 0; i < npistas; i++) {
        int k = pistas[i].r * N + pistas[i].c; const Pista *ps = &pistas[i];
        Pieza exacta = ps->giros >= 0 ? rotar(ps->orig, ps->giros) : ps->orig;
        #define CUMPLE(P) (ps->giros >= 0 ? !memcmp((P).c, exacta.c, 4) : !memcmp(canonica(P).c, ps->canon.c, 4))
        if (tipoc[k] == 2) { int s; for (s = 0; s < 4; s++) if (ke[s] == k) break; if (!CUMPLE(pe[s])) return 0; continue; }
        u64 *D = DOM(h, 0, k);
        if (tipoc[k] == 0) { for (int o = 0; o < norient; o++) { Pieza p; memcpy(p.c, orient[o].c, 4); if (!CUMPLE(p)) D[o >> 6] &= ~(1ULL << (o & 63)); } }
        else { int s = ladoc[k]; for (int t = 0; t < ntori; t++) { Pieza p; for (int d = 0; d < 4; d++) p.c[d] = COLB(s, t, d); if (!CUMPLE(p)) D[t >> 6] &= ~(1ULL << (t & 63)); } }
        #undef CUMPLE
        u64 any = 0; for (int w = 0; w < DW; w++) any |= D[w]; if (!any) return 0;
    }
    for (int s = 0; s < 4; s++) {
        int k = ke[s];
        for (int d = 0; d < 4; d++) {
            int k2 = vec4[k][d]; if (k2 < 0) continue;
            const u64 *m = mascara_celda(k2, (d + 2) & 3, pe[s].c[d]); u64 *D2 = DOM(h, 0, k2); u64 any = 0;
            for (int w = 0; w < palabras(k2); w++) { D2[w] &= m[w]; any |= D2[w]; }
            if (!any) return 0;
        }
    }
    if (!propagar_lados(h, 0, 15)) return 0;
    return propagar_anillo2(h, 0);
}

/* número de caminos del mapa de un lado (sin contar repeticiones de piezas): para ordenar grupos */
static double caminos_lado(u8 ini, u8 fin) {
    double w[64], w2[64]; for (int c = 0; c < 64; c++) w[c] = 0; w[ini] = 1;
    for (int q = 0; q < M; q++) {
        for (int c = 0; c < 64; c++) w2[c] = 0;
        for (int t = 0; t < ntori; t++) if (w[tori[t].a] > 0) w2[tori[t].b] += w[tori[t].a] * tori[t].cant;
        memcpy(w, w2, sizeof(w));
    }
    return w[fin];
}

static int buscar_unificado(int hilos, int prof, long long *nodos_out, int *maxprof_out, Pieza *grid) {
    HiloU *hs = xmalloc(hilos * sizeof(HiloU)); pthread_t *th = xmalloc(hilos * sizeof(pthread_t));
    atomic_store(&resuelto, 0); atomic_store(&terminados, 0);
    for (int x = 0; x < hilos; x++) {
        HiloU *h = &hs[x]; h->id = x; h->nhilos = hilos; h->prof_reparto = prof;
        h->dom = xmalloc((size_t)(nvar + 2) * NN * DW * sizeof(u64));
        h->puesto = xmalloc(NN * sizeof(int)); h->solucion = xmalloc(NN * sizeof(int));
        h->cint = xmalloc((ntint + 1) * sizeof(int)); h->cori = xmalloc((ntori + 1) * sizeof(int));
        if (!iniciar_grupo_u(h)) {
            for (int y = 0; y <= x; y++) { free(hs[y].dom); free(hs[y].puesto); free(hs[y].solucion); free(hs[y].cint); free(hs[y].cori); }
            free(hs); free(th); return 0;
        }
    }
    for (int x = 0; x < hilos; x++) pthread_create(&th[x], NULL, hilo_u, &hs[x]);
    double ultimo = ahora();
    while (atomic_load(&terminados) < hilos) {
        dormir_ms(5);
        if (!silencioso && ahora() - ultimo >= 2.0) {
            long long nd = 0; int mp = 0; for (int x = 0; x < hilos; x++) { nd += hs[x].nodos; if (hs[x].maxprof > mp) mp = hs[x].maxprof; }
            printf("    ... %.1f s, %lld nodos, máximo colocado: %d de %d piezas\n", ahora() - t_inicio, nd, mp + 4, NP);
            ultimo = ahora();
        }
    }
    int gan = -1;
    for (int x = 0; x < hilos; x++) { pthread_join(th[x], NULL); *nodos_out += hs[x].nodos; if (hs[x].maxprof > *maxprof_out) *maxprof_out = hs[x].maxprof; if (hs[x].encontrado && gan < 0) gan = x; }
    int res = 0;
    if (gan >= 0) {
        int ke[4] = {0, N - 1, NN - 1, (N - 1) * N};
        for (int s = 0; s < 4; s++) grid[ke[s]] = rotar(pieza_esquina(g4[s]), s);
        for (int i = 0; i < nvar; i++) {
            int k = var_celdas[i], o = hs[gan].solucion[k]; Pieza p;
            if (tipoc[k] == 0) memcpy(p.c, orient[o].c, 4); else for (int d = 0; d < 4; d++) p.c[d] = COLB(ladoc[k], o, d);
            grid[k] = p;
        }
        res = verificar(grid) ? 1 : -1;
    }
    for (int x = 0; x < hilos; x++) { free(hs[x].dom); free(hs[x].puesto); free(hs[x].solucion); free(hs[x].cint); free(hs[x].cori); }
    free(hs); free(th);
    return res;
}

/* ================================================================== versión 4.1: reparto DINÁMICO
 * Antes (v4): las ramas de un nivel fijo se repartían una sola vez entre los hilos; los que
 * terminaban pronto se quedaban parados. Ahora hay una cola de tareas compartida:
 *   - una tarea es un camino de decisiones desde el inicio del grupo;
 *   - un hilo sin trabajo avisa ("hambre"); un hilo ocupado le cede las opciones que aún no
 *     probó en su nivel menos profundo (las ramas más grandes) y deja de probarlas él;
 *   - el grupo termina cuando la cola está vacía y ningún hilo trabaja.
 * Se explora exactamente el mismo árbol que en v4, solo que repartido mientras corre. */
static int reparto_dinamico = 1;

typedef struct { int len; int *camino; double peso; } Tarea;
/* v4.3: "equipo" (pool) de hilos con su propia cola. Con el PORTAFOLIO hay dos equipos que
 * recorren el mismo grupo con órdenes distintos; gana el primero que encuentra una solución o
 * que termina de revisar todo el grupo (entonces el grupo no tiene solución). */
typedef struct {
    Tarea *t; int cap, ini, fin;
    pthread_mutex_t mx; pthread_cond_t cv;
    atomic_int hambre, en_cola;
    int activos, orden, nhilos, azar, lcv;
} Equipo;
#define EQ_INI {.mx = PTHREAD_MUTEX_INITIALIZER, .cv = PTHREAD_COND_INITIALIZER}
static Equipo equipos[4] = {EQ_INI, EQ_INI, EQ_INI, EQ_INI};
static const char *nombre_equipo(const Equipo *E) {
    static const char *n[8] = {"menos opciones", "interior primero", "menos opciones al azar", "interior primero al azar", "reinicios, menos opciones", "reinicios, interior primero", "menos opciones + pieza menos restrictiva", "interior primero + pieza menos restrictiva"};
    return n[(E->orden ? 1 : 0) + (E->lcv ? 6 : (E->azar == 1 ? 2 : (E->azar == 2 ? 4 : 0)))];
}
static int nequipos = 1;
static atomic_int grupo_agotado;   /* un equipo revisó todo el grupo */
static atomic_llong cedidas;     /* tareas creadas por cesión (estadística) */
#define cola_t (E->t)
#define cola_cap (E->cap)
#define cola_ini (E->ini)
#define cola_fin (E->fin)
#define cola_mx (E->mx)
#define cola_cv (E->cv)
#define hambre (E->hambre)
#define en_cola (E->en_cola)
#define activos_d (E->activos)

static void cola_meter(Equipo *E, const int *camino, int len, double peso) {   /* llamar con cola_mx tomado */
    if (cola_fin == cola_cap) {
        if (cola_ini > 0) { memmove(cola_t, cola_t + cola_ini, (cola_fin - cola_ini) * sizeof(Tarea)); cola_fin -= cola_ini; cola_ini = 0; }
        if (cola_fin == cola_cap) { cola_cap = cola_cap ? cola_cap * 2 : 256; cola_t = realloc(cola_t, cola_cap * sizeof(Tarea)); if (!cola_t) { fprintf(stderr, "Sin memoria\n"); exit(2); } }
    }
    Tarea *t = &cola_t[cola_fin++]; t->len = len; t->peso = peso; t->camino = xmalloc((len + 1) * sizeof(int));
    if (len) memcpy(t->camino, camino, len * sizeof(int));
    atomic_fetch_add(&en_cola, 1);
}

typedef struct {
    HiloU u;                /* estado de búsqueda (dominios, piezas puestas, conteos) */
    u64 *dom0; int vac0;    /* estado inicial del grupo, para reconstruir cada tarea */
    u64 *rest;              /* [nvar+2][DW]: opciones aún no probadas en cada nivel */
    int *camino;            /* opción elegida en cada nivel */
    int base;               /* primer nivel propio de la tarea actual */
    long long cesiones;
    /* v4.3: avance. Cada nodo reparte su peso por igual entre sus opciones; al terminar un
     * subárbol se suma su peso. hecho = fracción del grupo ya revisada por este hilo. */
    double *wniv;           /* [nvar+2] peso de cada opción del nivel */
    double *sub;            /* [nvar+2] peso ya sumado por los subárboles terminados de cada nivel */
    double *cedido;         /* [nvar+2] peso cedido a otros hilos desde cada nivel */
    volatile double hecho;
    Equipo *E;
    u64 rng;                /* para el orden al azar de las opciones */
    long long limite;       /* reinicios: nodos máximos de la búsqueda en curso (0 = sin límite) */
    float *puntaje;         /* [nvar+2][64*DW] valor de cada opción (orden por "menos restrictiva") */
    long long reinicios;
} HiloD;

/* ================================================================== versión 4.4 (pruebas): chequeos extra
 * paridad: "el complemento". Para cada color c, las mitades de color c que tienen las piezas que
 *   faltan (n_c) cubren los bordes abiertos de color c (b_c, uno cada uno) y el resto se emparejan
 *   entre sí. Por eso n_c >= b_c y n_c - b_c debe ser par.
 * conteo: cada pieza interior que falta debe tener al menos tantas casillas libres donde cabe como
 *   copias le quedan (antes solo se pedía una casilla). */
static int umbral_orilla = 1; static double umbral_portafolio = 1e9; static long long semilla_azar = 0;
static int paridad = 0, conteo_int = 0, orden_casillas = -1, portafolio = -1, portafolio_grupo = 0;   /* -1 = automático (Knuth decide por grupo) */
/* v4.4 (pruebas): EMPAREJAMIENTO (teorema de Hall). Todas las casillas libres de una clase (orilla o
 * interior) deben poder recibir, a la vez, una pieza distinta que les quepa. Se busca un emparejamiento
 * completo casillas <-> piezas restantes (con sus copias) por caminos de aumento (Kuhn). Si no existe,
 * la rama es imposible aunque cada casilla, por separado, tenga opciones. */
static int emparejar = 0, ramificar_pieza = 0, valor_lcv = 0;
#define EMP_TW 8           /* hasta 512 tipos */
typedef struct { int nc, nt; int cel[256]; u64 adj[256][EMP_TW]; int cap[512], usado[512], asig[512][8]; int *asig_big; unsigned char vis[512]; } Emp;
static int emp_intentar(Emp *E, int c) {
    for (int w = 0; w < EMP_TW; w++) { u64 x = E->adj[c][w]; while (x) { int t = w * 64 + ctz64(x); x &= x - 1;
        if (E->vis[t]) continue; E->vis[t] = 1;
        if (E->usado[t] < E->cap[t]) { if (E->usado[t] < 8) E->asig[t][E->usado[t]] = c; E->usado[t]++; return 1; }
        int lim = E->cap[t] < 8 ? E->cap[t] : 8;
        for (int i = 0; i < lim; i++) if (emp_intentar(E, E->asig[t][i])) { E->asig[t][i] = c; return 1; }
    } }
    return 0;
}
static int emp_resolver(Emp *E) {
    for (int t = 0; t < E->nt; t++) E->usado[t] = 0;
    for (int c = 0; c < E->nc; c++) {
        memset(E->vis, 0, E->nt);
        if (!emp_intentar(E, c)) return 0;
    }
    return 1;
}
static int emparejar_check(HiloU *h, int lv) {
    static __thread Emp E;
    for (int clase = 0; clase < 2; clase++) {
        if (!(emparejar & (clase ? 2 : 1))) continue;
        int nt = clase ? ntint : ntori; if (nt > 64 * EMP_TW) continue;
        E.nt = nt; E.nc = 0;
        for (int t = 0; t < nt; t++) { E.cap[t] = clase ? h->cint[t] : h->cori[t]; if (E.cap[t] > 8) return 1; }
        for (int i = 0; i < nvar && E.nc < 256; i++) {
            int k = var_celdas[i]; if (h->puesto[k] >= 0 || (tipoc[k] == 0) != (clase == 1)) continue;
            int c = E.nc++; memset(E.adj[c], 0, sizeof E.adj[c]);
            const u64 *D = DOM(h, lv, k);
            if (clase) { int prev = -1; for (int w = 0; w < OW; w++) { u64 x = D[w]; while (x) { int o = w * 64 + ctz64(x); x &= x - 1; int t = orient[o].tipo; if (t != prev) { if (E.cap[t] > 0) E.adj[c][t >> 6] |= 1ULL << (t & 63); prev = t; } } } }
            else { for (int w = 0; w < EW; w++) { u64 x = D[w]; while (x) { int t = w * 64 + ctz64(x); x &= x - 1; if (E.cap[t] > 0) E.adj[c][t >> 6] |= 1ULL << (t & 63); } } }
        }
        if (!emp_resolver(&E)) return 0;
    }
    return 1;
}

static int chequeos_extra(HiloU *h, int lv) {
    if (paridad) {
        int n[64], b[64]; memset(n, 0, NC * sizeof(int)); memset(b, 0, NC * sizeof(int));
        for (int t = 0; t < ntint; t++) if (h->cint[t] > 0) for (int d = 0; d < 4; d++) n[tint[t].c[d]] += h->cint[t];
        for (int t = 0; t < ntori; t++) if (h->cori[t] > 0) { n[tori[t].a] += h->cori[t]; n[tori[t].d] += h->cori[t]; n[tori[t].b] += h->cori[t]; }
        int ke[4] = {0, N - 1, NN - 1, (N - 1) * N};
        for (int k = 0; k < NN; k++) {
            int esq_s = -1; if (tipoc[k] == 2) { for (int s = 0; s < 4; s++) if (ke[s] == k) esq_s = s; }
            else if (h->puesto[k] < 0) continue;
            Pieza pe; if (esq_s >= 0) pe = rotar(pieza_esquina(g4[esq_s]), esq_s);
            for (int d = 0; d < 4; d++) {
                int k2 = vec4[k][d]; if (k2 < 0 || tipoc[k2] == 2 || h->puesto[k2] >= 0) continue;
                int c = esq_s >= 0 ? pe.c[d] : color_opcion(k, h->puesto[k], d);
                b[c]++;
            }
        }
        for (int c = 1; c < NC; c++) if (n[c] < b[c] || ((n[c] - b[c]) & 1)) return 0;
    }
    if (conteo_int) {
        for (int t = 0; t < ntint; t++) {
            if (h->cint[t] <= 1) continue;      /* con 1 copia ya lo revisa la unión */
            const u64 *mt = MT(t); int cnt = 0;
            for (int i = 0; i < nvar && cnt < h->cint[t]; i++) {
                int k = var_celdas[i]; if (tipoc[k] != 0 || h->puesto[k] >= 0) continue;
                const u64 *D = DOM(h, lv, k);
                for (int w = 0; w < OW; w++) if (D[w] & mt[w]) { cnt++; break; }
            }
            if (cnt < h->cint[t]) return 0;
        }
    }
    return 1;
}

/* elige la casilla con menos opciones; -1 si el estado ya es imposible (idéntico a rec_u de v4) */
static int seleccionar_u(HiloU *h, int lv) {
    int mejor = -1, mejor_v = 1 << 30;
    u64 uni_i[64], uni_b[64]; memset(uni_i, 0, OW * sizeof(u64)); memset(uni_b, 0, EW * sizeof(u64));
    int *cabe = NULL; int cabe_local[512];
    if (ntori <= 512) { cabe = cabe_local; memset(cabe, 0, ntori * sizeof(int)); }
    for (int i = 0; i < nvar; i++) {
        int k = var_celdas[i]; if (h->puesto[k] >= 0) continue;
        const u64 *D = DOM(h, lv, k); int nw = palabras(k), nb = 0;
        if (tipoc[k] == 0) for (int w = 0; w < nw; w++) { nb += popcount64(D[w]); uni_i[w] |= D[w]; }
        else for (int w = 0; w < nw; w++) { nb += popcount64(D[w]); uni_b[w] |= D[w];
                 if (cabe) { u64 x = D[w]; while (x) { cabe[w * 64 + ctz64(x)]++; x &= x - 1; } } }
        if (nb == 0) return -1;
        int vec = 0; for (int d = 0; d < 4; d++) { int k2 = vec4[k][d]; if (k2 >= 0 && (tipoc[k2] == 2 || h->puesto[k2] >= 0)) vec++; }
        int v = orden_peso ? (nb < 2 ? nb : nb + (4 - vec) * orden_peso) : nb * 8 - vec;
        if (h->orden >= 3 && nb > 1) {   /* pruebas de orden dentro del interior (la orilla siempre al final) */
            int r = k / N, c = k % N;
            if (tipoc[k] != 0) v = (1 << 24) + nb;
            else if (h->orden == 3) v = (1 << 12) + (r * N + c);                         /* fila por fila */
            else if (h->orden == 4) { int d = r - 1 < c - 1 ? r - 1 : c - 1; int d2 = (N - 2 - r) < (N - 2 - c) ? (N - 2 - r) : (N - 2 - c); if (d2 < d) d = d2;   /* espiral: de afuera hacia adentro */
                                      v = (1 << 12) + d * 4096 / N + nb; v = (1 << 12) + d * 256 + nb; }
            else if (h->orden == 5) v = (1 << 12) + nb - 64 * vec;                        /* más vecinas puestas primero */
            else if (h->orden == 7) v = (1 << 12) + (r + c) * 256 + nb;                  /* por diagonales */
            else if (h->orden == 8) v = (1 << 12) + r * 256 + nb;                        /* por filas, en la fila la de menos opciones */
            else if (h->orden == 9) v = (1 << 12) + (r + c) * 256 + r;                   /* diagonales en orden fijo */
            else if (h->orden == 6) { int dc = abs(2 * r - (N - 1)) + abs(2 * c - (N - 1)); v = (1 << 12) + dc * 256 + nb; }   /* del centro hacia afuera */
        }
        else if (h->orden == 1 && tipoc[k] != 0 && nb > umbral_orilla) v += 1 << 20;          /* interior primero */
        else if (h->orden == 2 && tipoc[k] == 0 && nb > 1) v += 1 << 20;     /* orilla primero */
        if (v < mejor_v) { mejor_v = v; mejor = k; }
    }
    for (int t = 0; t < ntint; t++) if (h->cint[t] > 0) { const u64 *mt = MT(t); int ok = 0; for (int w = 0; w < OW; w++) if (uni_i[w] & mt[w]) { ok = 1; break; } if (!ok) return -1; }
    for (int t = 0; t < ntori; t++) if (h->cori[t] > 0) {
        if (!((uni_b[t >> 6] >> (t & 63)) & 1)) return -1;
        if (cabe && cabe[t] < h->cori[t]) return -1;
    }
    if ((paridad || conteo_int) && !chequeos_extra(h, lv)) return -1;
    if (emparejar && !emparejar_check(h, lv)) return -1;
    return mejor;
}

/* "el inverso": para cada pieza, ¿en cuántas casillas libres cabe todavía?
 * 0 casillas -> imposible; menos casillas que copias -> imposible;
 * 1 sola casilla y 1 copia -> esa casilla queda reservada para esa pieza. */
static int forzar_unicas(HiloU *h, int lv) {
    int cnt[4096], ult[4096];
    if (ntint > 4096 || ntori > 4096) return 1;
    memset(cnt, 0, ntint * sizeof(int));
    for (int i = 0; i < nvar; i++) {
        int k = var_celdas[i]; if (tipoc[k] != 0 || h->puesto[k] >= 0) continue;
        const u64 *D = DOM(h, lv, k); int prev = -1;
        for (int w = 0; w < OW; w++) { u64 x = D[w]; while (x) { int o = w * 64 + ctz64(x); x &= x - 1; int t = orient[o].tipo; if (t != prev) { if (ult[t] != k || cnt[t] == 0) { cnt[t]++; ult[t] = k; } prev = t; } } }
    }
    for (int t = 0; t < ntint; t++) {
        if (h->cint[t] <= 0) continue;
        if (cnt[t] < h->cint[t]) return 0;
        if (cnt[t] == 1 && h->cint[t] == 1) {
            u64 *D = DOM(h, lv, ult[t]); const u64 *mt = MT(t); u64 any = 0;
            for (int w = 0; w < OW; w++) { D[w] &= mt[w]; any |= D[w]; }
            if (!any) return 0;
        }
    }
    return 1;
}

/* coloca la opción o en la casilla k: construye el nivel lv+1. Devuelve 1 si sigue siendo posible. */
static int aplicar_u(HiloU *h, int lv, int k, int o) {
    u64 *H = DOM(h, lv + 1, 0);
    memcpy(H, DOM(h, lv, 0), (size_t)NN * DW * sizeof(u64));
    u64 *Hk = DOM(h, lv + 1, k); memset(Hk, 0, DW * sizeof(u64)); Hk[o >> 6] = 1ULL << (o & 63);
    h->puesto[k] = o;
    int esint = tipoc[k] == 0, t = esint ? orient[o].tipo : o, ok = 1;
    unsigned lados = 0;
    if (esint) h->cint[t]--; else { h->cori[t]--; lados |= 1u << ladoc[k]; }
    if (esint && h->cint[t] == 0) {
        const u64 *mt = MT(t);
        for (int i = 0; i < nvar; i++) { int kk = var_celdas[i]; if (tipoc[kk] != 0 || h->puesto[kk] >= 0) continue; u64 *D = DOM(h, lv + 1, kk); for (int w = 0; w < OW; w++) D[w] &= ~mt[w]; }
    } else if (!esint && h->cori[t] == 0) {
        for (int i = 0; i < nvar; i++) { int kk = var_celdas[i]; if (tipoc[kk] != 1 || h->puesto[kk] >= 0) continue; DOM(h, lv + 1, kk)[t >> 6] &= ~(1ULL << (t & 63)); lados |= 1u << ladoc[kk]; }
    }
    for (int d = 0; d < 4 && ok; d++) {
        int k2 = vec4[k][d]; if (k2 < 0 || tipoc[k2] == 2 || h->puesto[k2] >= 0) continue;
        const u64 *m = mascara_celda(k2, (d + 2) & 3, color_opcion(k, o, d));
        u64 *D2 = DOM(h, lv + 1, k2); u64 any = 0; int n2 = palabras(k2);
        for (int w = 0; w < n2; w++) { D2[w] &= m[w]; any |= D2[w]; }
        if (!any) ok = 0;
        if (tipoc[k2] == 1) lados |= 1u << ladoc[k2];
    }
    if (ok && lados) ok = propagar_lados(h, lv + 1, lados);
    if (ok && nlin) ok = propagar_anillo2(h, lv + 1);
    if (ok && forzar) ok = forzar_unicas(h, lv + 1);
    return ok;
}
static inline void deshacer_u(HiloU *h, int k, int o) {
    h->puesto[k] = -1;
    if (tipoc[k] == 0) h->cint[orient[o].tipo]++; else h->cori[o]++;
}

/* cede a la cola las opciones no probadas de los niveles menos profundos */
static void ceder(HiloD *hd, int lv) {
    Equipo *E = hd->E;
    int quiere = atomic_load_explicit(&hambre, memory_order_relaxed), dadas = 0;
    pthread_mutex_lock(&cola_mx);
    for (int L = hd->base; L < lv && dadas < quiere; L++) {
        u64 *R = hd->rest + (size_t)L * DW; int actual = hd->camino[L];   /* la opción que este hilo explora ahora */
        for (int w = 0; w < DW; w++) {
            u64 x = R[w];
            while (x) { int o = w * 64 + ctz64(x); x &= x - 1; hd->camino[L] = o; cola_meter(E, hd->camino, L + 1, hd->wniv[L]); dadas++; hd->cedido[L] += hd->wniv[L]; }
            R[w] = 0;
        }
        hd->camino[L] = actual;
    }
    if (dadas) { hd->cesiones += dadas; atomic_fetch_add(&cedidas, dadas); pthread_cond_broadcast(&cola_cv); }
    pthread_mutex_unlock(&cola_mx);
}

static inline u64 rng_sig(u64 *s) { u64 z = (*s += 0x9E3779B97F4A7C15ULL); z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL; z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL; return z ^ (z >> 31); }
static inline void terminar_nodo(HiloD *hd, int lv, double peso) {
    double tot = peso - hd->cedido[lv];
    hd->hecho += tot - hd->sub[lv];
    if (lv > hd->base) { hd->sub[lv - 1] += tot; hd->cedido[lv - 1] += hd->cedido[lv]; }   /* lo cedido en el subárbol tampoco lo cuenta el padre */
}
static int rec_d(HiloD *hd, int lv, int vacias, double peso) {
    HiloU *h = &hd->u;
    hd->sub[lv] = 0; hd->cedido[lv] = 0;
    h->nodos++;
    if (lv > h->maxprof) h->maxprof = lv;
    if ((h->nodos & 255) == 0) {
        if ((h->nodos & 1023) == 0) {
            if (atomic_load_explicit(&resuelto, memory_order_relaxed) || atomic_load_explicit(&detener, memory_order_relaxed) || atomic_load_explicit(&grupo_agotado, memory_order_relaxed)) return -1;
            if (hd->limite && h->nodos > hd->limite) return -1;
            if (t_limite > 0 && ahora() - t_inicio > t_limite) { atomic_store(&detener, 1); return -1; }
        }
        Equipo *E = hd->E;
        if (atomic_load_explicit(&hambre, memory_order_relaxed) > 0 && atomic_load_explicit(&en_cola, memory_order_relaxed) == 0) ceder(hd, lv);
    }
    if (vacias == 0) { memcpy(h->solucion, h->puesto, NN * sizeof(int)); h->encontrado = 1; return 1; }
    int k = seleccionar_u(h, lv);
    if (k < 0) { terminar_nodo(hd, lv, peso); return 0; }
    u64 *R = hd->rest + (size_t)lv * DW; const u64 *Dk = DOM(h, lv, k);
    int nw = palabras(k), nop = 0; for (int w = 0; w < DW; w++) { R[w] = w < nw ? Dk[w] : 0; nop += popcount64(R[w]); }
    hd->wniv[lv] = nop ? peso / nop : 0;
    float *P = NULL;
    if ((hd->E->lcv ? hd->E->lcv : valor_lcv) && nop > 1 && !hd->E->azar) {
        int modo_lcv = hd->E->lcv ? hd->E->lcv : valor_lcv;
        /* "probabilidades": se prueba primero la opción que deja más opciones a las casillas vecinas
         * (la menos restrictiva). Las que ya fallan al aplicarlas se descartan aquí mismo. */
        P = hd->puntaje + (size_t)lv * 64 * DW;
        for (int w = 0; w < nw; w++) { u64 x = R[w]; while (x) { int o = w * 64 + ctz64(x); x &= x - 1;
            float sc = -1;
            if (aplicar_u(h, lv, k, o)) {
                sc = 0;
                if (modo_lcv == 1) { for (int d = 0; d < 4; d++) { int k2 = vec4[k][d]; if (k2 < 0 || tipoc[k2] == 2 || h->puesto[k2] >= 0) continue;
                    const u64 *D2 = DOM(h, lv + 1, k2); int c = 0; for (int ww = 0; ww < palabras(k2); ww++) c += popcount64(D2[ww]); sc += logf((float)c + 1); } }
                else { for (int i = 0; i < nvar; i++) { int k2 = var_celdas[i]; if (h->puesto[k2] >= 0) continue;
                    const u64 *D2 = DOM(h, lv + 1, k2); int c = 0; for (int ww = 0; ww < palabras(k2); ww++) c += popcount64(D2[ww]); sc += logf((float)c + 1); } }
            }
            deshacer_u(h, k, o);
            P[o] = sc;
        } }
    }
    for (;;) {
        int o = -1;
        if (P) {
            float mejor = -2;
            for (int w = 0; w < nw; w++) { u64 x = R[w]; while (x) { int oo = w * 64 + ctz64(x); x &= x - 1; if (P[oo] > mejor) { mejor = P[oo]; o = oo; } } }
            if (o >= 0) R[o >> 6] &= ~(1ULL << (o & 63));
            if (o >= 0 && mejor < 0) continue;      /* ya se sabe que falla */
        } else
        if (hd->E->azar) {         /* una opción al azar entre las que quedan (equipos al azar y de reinicios) */
            int n = 0; for (int w = 0; w < nw; w++) n += popcount64(R[w]);
            if (n) { int r = (int)(rng_sig(&hd->rng) % (u64)n);
                for (int w = 0; w < nw && o < 0; w++) { int c = popcount64(R[w]); if (r >= c) { r -= c; continue; }
                    u64 x = R[w]; while (r--) x &= x - 1; int b = ctz64(x); o = w * 64 + b; R[w] &= ~(1ULL << b); } }
        } else
        for (int w = 0; w < nw; w++) if (R[w]) { o = w * 64 + ctz64(R[w]); R[w] &= R[w] - 1; break; }
        if (o < 0) break;
        hd->camino[lv] = o;
        int ok = aplicar_u(h, lv, k, o);
        int r = ok ? rec_d(hd, lv + 1, vacias - 1, hd->wniv[lv]) : 0;
        deshacer_u(h, k, o);
        if (r != 0) return r;
    }
    terminar_nodo(hd, lv, peso);
    return 0;
}

/* reconstruye el estado de una tarea desde el inicio del grupo y la explora */
static int ejecutar_tarea(HiloD *hd, const Tarea *t) {
    HiloU *h = &hd->u;
    memcpy(DOM(h, 0, 0), hd->dom0, (size_t)NN * DW * sizeof(u64));
    for (int k = 0; k < NN; k++) h->puesto[k] = -1;
    memcpy(h->cint, cant_int, ntint * sizeof(int));
    for (int x = 0; x < ntori; x++) h->cori[x] = tori[x].cant;
    int lv = 0, vac = hd->vac0;
    for (int i = 0; i < t->len; i++) {
        int k = seleccionar_u(h, lv); if (k < 0) { hd->hecho += t->peso; return 0; }
        int o = t->camino[i]; hd->camino[lv] = o;
        if (!aplicar_u(h, lv, k, o)) { hd->hecho += t->peso; return 0; }
        lv++; vac--;
    }
    hd->base = lv;
    return rec_d(hd, lv, vac, t->peso);
}

static void despertar_todos(void) {
    for (int e = 0; e < nequipos; e++) { pthread_mutex_lock(&equipos[e].mx); pthread_cond_broadcast(&equipos[e].cv); pthread_mutex_unlock(&equipos[e].mx); }
}
/* REINICIOS (Fase B): cada hilo del equipo busca solo, con las opciones en orden al azar y un tope
 * de nodos que crece según la serie de Luby (1,1,2,1,1,2,4,...). Al llegar al tope, empieza de nuevo
 * con otro orden al azar. No revisa todo (no es completo), por eso va siempre junto a un equipo completo. */
static long long reinicio_base = 20000;
static long long luby(long long i) {   /* i >= 1 */
    for (long long k = 1;; k++) {
        long long p = (1LL << k) - 1;
        if (i == p) return 1LL << (k - 1);
        if (i < p) return luby(i - (1LL << (k - 1)) + 1);
    }
}
static void *hilo_reinicios(HiloD *hd) {
    Tarea raiz = {0, NULL, 1.0};
    for (long long i = 1; !atomic_load(&resuelto) && !atomic_load(&detener) && !atomic_load(&grupo_agotado); i++) {
        hd->limite = hd->u.nodos + reinicio_base * luby(i);
        hd->reinicios++;
        int r = ejecutar_tarea(hd, &raiz);
        if (r == 1) { atomic_store(&resuelto, 1); despertar_todos(); break; }
        if (r == 0) {   /* recorrió todo sin tope: el grupo no tiene solución */
            if (!atomic_exchange(&grupo_agotado, 1)) despertar_todos();
            break;
        }
    }
    hd->limite = 0;
    atomic_fetch_add(&terminados, 1);
    return NULL;
}

static void *hilo_d(void *arg) {
    HiloD *hd = arg; Equipo *E = hd->E;
    if (E->azar == 2) return hilo_reinicios(hd);
    for (;;) {
        pthread_mutex_lock(&cola_mx);
        atomic_fetch_add(&hambre, 1);
        while (cola_ini == cola_fin && activos_d > 0 && !atomic_load(&resuelto) && !atomic_load(&detener) && !atomic_load(&grupo_agotado))
            pthread_cond_wait(&cola_cv, &cola_mx);
        atomic_fetch_sub(&hambre, 1);
        if (cola_ini == cola_fin || atomic_load(&resuelto) || atomic_load(&detener) || atomic_load(&grupo_agotado)) {   /* no queda trabajo */
            int agotado = cola_ini == cola_fin && activos_d == 0 && !atomic_load(&resuelto) && !atomic_load(&detener);
            pthread_cond_broadcast(&cola_cv); pthread_mutex_unlock(&cola_mx);
            if (agotado && !atomic_exchange(&grupo_agotado, 1) && nequipos > 1) despertar_todos();   /* este equipo revisó todo: el grupo no tiene solución */
            break;
        }
        Tarea t = cola_t[cola_ini++]; atomic_fetch_sub(&en_cola, 1); activos_d++;
        pthread_mutex_unlock(&cola_mx);
        int r = ejecutar_tarea(hd, &t);
        free(t.camino);
        if (r == 1) { atomic_store(&resuelto, 1); if (nequipos > 1) despertar_todos(); }
        pthread_mutex_lock(&cola_mx); activos_d--; pthread_cond_broadcast(&cola_cv); pthread_mutex_unlock(&cola_mx);
    }
    atomic_fetch_add(&terminados, 1);
    return NULL;
}

/* ================================================================== versión 4.3: ESTIMADO DE KNUTH (1975)
 * Un sondeo baja desde la raíz eligiendo al azar una de las opciones viables en cada nivel.
 * Si en los niveles hubo d1, d2, ... opciones viables, el árbol tiene "en promedio"
 * 1 + d1 + d1*d2 + d1*d2*d3 + ... nodos. El promedio de muchos sondeos es un estimado sin sesgo
 * del tamaño del árbol del grupo (con mucha varianza: sirve para el orden de magnitud). */
static double estimar_seg = -1;     /* segundos de sondeo por grupo; -1 = automático */
static int orden_knuth = 0, solo_estimar = 0;         /* 1 = ordenar los grupos por su estimado (el más chico primero) */
static int grupo_act = 0, grupo_tot = 0;
static double est_grupo = 0;        /* estimado de nodos del grupo en curso (0 = no hay) */

typedef struct { HiloD *hd; double seg; u64 rng; double suma, suma_log; long long sondeos, nodos_sondeo; int viables[4096]; double perfil[512]; double perfil_ori[512]; long long ramas_pieza; } Sondeo;
static Sondeo *sondeos_ult; static int nsondeos_ult;
#include <math.h>
static void *hilo_sondeo(void *arg) {
    Sondeo *S = arg; HiloD *hd = S->hd; HiloU *h = &hd->u;
    double fin = ahora() + S->seg;
    while (ahora() < fin && !atomic_load_explicit(&detener, memory_order_relaxed)) {
        memcpy(DOM(h, 0, 0), hd->dom0, (size_t)NN * DW * sizeof(u64));
        for (int k = 0; k < NN; k++) h->puesto[k] = -1;
        memcpy(h->cint, cant_int, ntint * sizeof(int));
        for (int x = 0; x < ntori; x++) h->cori[x] = tori[x].cant;
        int lv = 0, vac = hd->vac0; double prod = 1, est = 1;
        while (vac > 0) {
            int k = seleccionar_u(h, lv); if (k < 0) break;
            const u64 *Dk = DOM(h, lv, k); int nw = palabras(k), nv = 0;
            u64 opc[64]; memcpy(opc, Dk, nw * sizeof(u64));
            int nbk = 0; for (int w = 0; w < nw; w++) nbk += popcount64(opc[w]);
            int pk[4096], po[4096], np = 0;
            if (ramificar_pieza && nbk > 1) {
                /* "el inverso": la pieza única con menos lugares posibles, si tiene menos que la casilla */
                int mejor_t = -1, mejor_n = nbk;
                for (int t = 0; t < ntint; t++) {
                    if (h->cint[t] != 1) continue; const u64 *mt = MT(t); int n = 0;
                    for (int i = 0; i < nvar && n < mejor_n; i++) { int kk = var_celdas[i]; if (tipoc[kk] != 0 || h->puesto[kk] >= 0) continue;
                        const u64 *D = DOM(h, lv, kk); for (int w = 0; w < OW; w++) n += popcount64(D[w] & mt[w]); }
                    if (n < mejor_n) { mejor_n = n; mejor_t = t; }
                }
                if (mejor_t >= 0) {
                    const u64 *mt = MT(mejor_t);
                    for (int i = 0; i < nvar; i++) { int kk = var_celdas[i]; if (tipoc[kk] != 0 || h->puesto[kk] >= 0) continue;
                        const u64 *D = DOM(h, lv, kk); for (int w = 0; w < OW; w++) { u64 x = D[w] & mt[w]; while (x) { pk[np] = kk; po[np] = w * 64 + ctz64(x); np++; x &= x - 1; } } }
                    S->ramas_pieza++;
                }
            }
            if (np == 0) { for (int w = 0; w < nw; w++) { u64 x = opc[w]; while (x) { pk[np] = k; po[np] = w * 64 + ctz64(x); np++; x &= x - 1; } } }
            for (int q = 0; q < np; q++) {
                S->nodos_sondeo++;
                if (aplicar_u(h, lv, pk[q], po[q]) && nv < 4096) S->viables[nv++] = q;
                deshacer_u(h, pk[q], po[q]);
            }
            if (!nv) break;
            prod *= nv; est += prod;
            if (lv + 1 < 512) { S->perfil[lv + 1] += prod; if (tipoc[k] != 0) S->perfil_ori[lv + 1] += prod; }
            int q = S->viables[rng_sig(&S->rng) % (u64)nv];
            aplicar_u(h, lv, pk[q], po[q]); lv++; vac--;
        }
        S->suma += est; S->suma_log += log10(est); S->sondeos++;
    }
    return NULL;
}
/* estima el tamaño del árbol del grupo con los estados iniciales ya preparados en hs[] */
static double estimar_grupo(HiloD *hs, int hilos, double seg, long long *sondeos_out) {
    Sondeo *S = xmalloc(hilos * sizeof(Sondeo)); pthread_t *th = xmalloc(hilos * sizeof(pthread_t));
    for (int x = 0; x < hilos; x++) { S[x].hd = &hs[x]; S[x].seg = seg; S[x].rng = 0x51ED27A5ULL * (x + 1) + (u64)grupo_act * 7919; pthread_create(&th[x], NULL, hilo_sondeo, &S[x]); }
    double suma = 0; long long n = 0;
    for (int x = 0; x < hilos; x++) { pthread_join(th[x], NULL); suma += S[x].suma; n += S[x].sondeos; }
    if (getenv("E2_PERFIL") && n) {
        fprintf(stderr, "PERFIL grupo %d (nivel: nodos estimados en ese nivel, %% de ellos donde se eligió una casilla de orilla)\n", grupo_act);
        for (int L = 1; L < 512 && L <= nvar; L++) { double a = 0, b = 0; for (int x = 0; x < hilos; x++) { a += S[x].perfil[L]; b += S[x].perfil_ori[L]; } if (a > 0) fprintf(stderr, "  %3d %.3g  orilla %.0f%%\n", L, a / n, 100 * b / a); }
    }
    free(S); free(th);
    if (sondeos_out) *sondeos_out = n;
    return n ? suma / n : 0;
}

/* tiempo en palabras: s, min, h, días, años */
static void fmt_tiempo(double s, char *b, size_t n) {
    if (!(s >= 0) || s > 1e18) snprintf(b, n, "muchísimo");
    else if (s < 120) snprintf(b, n, "%.0f s", s);
    else if (s < 7200) snprintf(b, n, "%.0f min", s / 60);
    else if (s < 172800) snprintf(b, n, "%.1f h", s / 3600);
    else if (s < 3 * 31557600.0) snprintf(b, n, "%.0f días", s / 86400);
    else snprintf(b, n, "%.3g años", s / 31557600.0);
}
static void fmt_pct(double p, char *b, size_t n) {
    p *= 100;
    if (p >= 1) snprintf(b, n, "%.1f%%", p);
    else if (p >= 0.01) snprintf(b, n, "%.3f%%", p);
    else snprintf(b, n, "%.2e%%", p);
}

/* est_previo: -1 = estimar aquí si toca; >= 0 estimado ya calculado; -2 = solo estimar y salir (resultado en est_grupo) */
static int buscar_dinamico(int hilos, long long *nodos_out, int *maxprof_out, Pieza *grid, double est_previo) {
    HiloD *hs = xmalloc(hilos * sizeof(HiloD)); pthread_t *th = xmalloc(hilos * sizeof(pthread_t));
    atomic_store(&resuelto, 0); atomic_store(&terminados, 0); atomic_store(&grupo_agotado, 0);
    /* equipos: con portafolio, la mitad de los hilos con cada orden */
    nequipos = (est_previo == -2 || hilos < 2) ? 1 : ((portafolio_grupo == 3 || portafolio_grupo == 5) && hilos >= 3 ? 3 : (portafolio_grupo >= 4 && hilos >= 4 ? 4 : (portafolio_grupo >= 2 ? 2 : 1)));
    for (int e = 0; e < nequipos; e++) {
        Equipo *E = &equipos[e];
        atomic_store(&hambre, 0); atomic_store(&en_cola, 0); cola_ini = cola_fin = 0; activos_d = 0;
        E->orden = nequipos >= 2 ? (e & 1) : orden_casillas; E->azar = e >> 1; E->nhilos = 0;
        E->lcv = 0;
        if (nequipos == 3 && portafolio_grupo == 3) {   /* portafolio 3: equipo completo con el orden elegido + reinicios con cada orden */
            E->orden = e == 0 ? orden_casillas : (e == 1 ? orden_casillas : !orden_casillas);
            E->azar = e == 0 ? 0 : 2;
        }
        if (nequipos == 3 && portafolio_grupo == 5) {   /* portafolio 5: tres estrategias distintas, cada una con un tercio de los hilos */
            E->orden = orden_casillas;
            E->azar = e == 2 ? 2 : 0;          /* equipo 3: reinicios al azar */
            E->lcv = e == 1 ? 1 : 0;           /* equipo 2: primero la pieza menos restrictiva (completo) */
        }
    }
    int posible = 1;
    for (int x = 0; x < hilos; x++) {
        HiloD *hd = &hs[x]; HiloU *h = &hd->u; h->id = x; h->nhilos = hilos;
        hd->E = &equipos[x % nequipos]; hd->E->nhilos++; h->orden = hd->E->orden;
        hd->rng = 0xA5A5F00DULL * (x + 1) + (u64)grupo_act * 104729ULL + (u64)semilla_azar;
        h->dom = xmalloc((size_t)(nvar + 2) * NN * DW * sizeof(u64));
        h->puesto = xmalloc(NN * sizeof(int)); h->solucion = xmalloc(NN * sizeof(int));
        h->cint = xmalloc((ntint + 1) * sizeof(int)); h->cori = xmalloc((ntori + 1) * sizeof(int));
        hd->dom0 = xmalloc((size_t)NN * DW * sizeof(u64));
        hd->rest = xmalloc((size_t)(nvar + 2) * DW * sizeof(u64)); hd->camino = xmalloc((nvar + 2) * sizeof(int));
        hd->puntaje = xmalloc((size_t)(nvar + 2) * 64 * DW * sizeof(float));
        hd->wniv = xmalloc((nvar + 2) * sizeof(double)); hd->sub = xmalloc((nvar + 2) * sizeof(double)); hd->cedido = xmalloc((nvar + 2) * sizeof(double)); hd->hecho = 0;
        if (posible && !iniciar_grupo_u(h)) posible = 0;
        memcpy(hd->dom0, DOM(h, 0, 0), (size_t)NN * DW * sizeof(u64));
        hd->vac0 = 0; for (int i = 0; i < nvar; i++) if (h->puesto[var_celdas[i]] < 0) hd->vac0++;
    }
    int res = 0;
    if (est_previo >= 0) est_grupo = est_previo;
    else est_grupo = 0;
    if (est_previo == -2) {   /* solo estimar (para ordenar los grupos) */
        double e = 0; if (posible) e = estimar_grupo(hs, hilos, estimar_seg, NULL);
        for (int x = 0; x < hilos; x++) { HiloU *h = &hs[x].u; free(h->dom); free(h->puesto); free(h->solucion); free(h->cint); free(h->cori); free(hs[x].dom0); free(hs[x].rest); free(hs[x].camino); free(hs[x].wniv); free(hs[x].sub); free(hs[x].cedido); }
        free(hs); free(th); est_grupo = e; return 0;
    }
    if (posible && estimar_seg > 0 && est_previo < 0) {
        long long ns = 0; double t0 = ahora();
        est_grupo = estimar_grupo(hs, hilos, estimar_seg, &ns);
        if (!silencioso) printf("[3] Grupo %d/%d: estimado de Knuth ~%.3g nodos (%lld sondeos, %.1f s)\n", grupo_act, grupo_tot, est_grupo, ns, ahora() - t0);
    }
    if (posible) {
        for (int e = 0; e < nequipos; e++) { Equipo *E = &equipos[e]; if (E->azar == 2) continue; pthread_mutex_lock(&cola_mx); cola_meter(E, NULL, 0, 1.0); pthread_mutex_unlock(&cola_mx); }   /* tarea raíz de cada equipo */
        double tg = ahora();
        for (int x = 0; x < hilos; x++) pthread_create(&th[x], NULL, hilo_d, &hs[x]);
        double ultimo = ahora();
        while (atomic_load(&terminados) < hilos) {
            dormir_ms(5);
            if (!silencioso && ahora() - ultimo >= 2.0) {
                long long nd = 0; int mp = 0; double hecho = 0, hq[4] = {0, 0, 0, 0};
                for (int x = 0; x < hilos; x++) { nd += hs[x].u.nodos; if (hs[x].u.maxprof > mp) mp = hs[x].u.maxprof; hq[hs[x].E - equipos] += hs[x].hecho; }
                for (int e = 0; e < nequipos; e++) if (equipos[e].azar != 2 && hq[e] > hecho) hecho = hq[e];
                int act = 0; for (int e = 0; e < nequipos; e++) { Equipo *E = &equipos[e]; pthread_mutex_lock(&cola_mx); act += activos_d; pthread_mutex_unlock(&cola_mx); }
                double tr = ahora() - tg, vel = tr > 0 ? nd / tr : 0;
                char sp[32], sa[48], sk[48];
                fmt_pct(hecho, sp, sizeof sp);
                if (hecho > 0) { fmt_tiempo(tr * (1 - hecho) / hecho, sa, sizeof sa); } else snprintf(sa, sizeof sa, "?");
                printf("    ... %.1f s | grupo %d/%d | %lld nodos | revisado %s, faltan ~%s", ahora() - t_inicio, grupo_act, grupo_tot, nd, sp, sa);
                if (nequipos >= 2) { printf(" ["); for (int e = 0; e < nequipos; e++) { char s0[32]; fmt_pct(hq[e], s0, sizeof s0); printf("%s%s %s", e ? ", " : "", nombre_equipo(&equipos[e]), s0); } printf("]"); }
                if (est_grupo > 0 && vel > 0) { double f = est_grupo - nd; if (f < 0) snprintf(sk, sizeof sk, "pasó el estimado"); else fmt_tiempo(f / vel, sk, sizeof sk); printf(" (Knuth: ~%s)", sk); }
                printf(" | máximo colocado %d de %d | hilos %d de %d\n", mp + 4, NP, act, hilos);
                ultimo = ahora();
            }
        }
        int gan = -1;
        for (int x = 0; x < hilos; x++) { pthread_join(th[x], NULL); *nodos_out += hs[x].u.nodos; if (hs[x].u.maxprof > *maxprof_out) *maxprof_out = hs[x].u.maxprof; if (hs[x].u.encontrado && gan < 0) gan = x; }
        for (int e = 0; e < nequipos; e++) { Equipo *E = &equipos[e]; pthread_mutex_lock(&cola_mx); for (int i = cola_ini; i < cola_fin; i++) free(cola_t[i].camino); cola_ini = cola_fin = 0; pthread_mutex_unlock(&cola_mx); atomic_store(&en_cola, 0); }
        if (nequipos >= 2 && !silencioso) {
            long long ne[4] = {0, 0, 0, 0}; for (int x = 0; x < hilos; x++) ne[hs[x].E - equipos] += hs[x].u.nodos;
            if (gan >= 0) printf("    portafolio: encontró la solución el equipo \"%s\" (nodos:", nombre_equipo(hs[gan].E));
            else if (atomic_load(&grupo_agotado)) printf("    portafolio: un equipo revisó todo el grupo (nodos:");
            if (gan >= 0 || atomic_load(&grupo_agotado)) { for (int e = 0; e < nequipos; e++) printf("%s %s %lld", e ? "," : "", nombre_equipo(&equipos[e]), ne[e]); printf(")\n"); }
        }
        if (gan >= 0) {
            int ke[4] = {0, N - 1, NN - 1, (N - 1) * N};
            for (int s = 0; s < 4; s++) grid[ke[s]] = rotar(pieza_esquina(g4[s]), s);
            for (int i = 0; i < nvar; i++) {
                int k = var_celdas[i], o = hs[gan].u.solucion[k]; Pieza p;
                if (tipoc[k] == 0) memcpy(p.c, orient[o].c, 4); else for (int d = 0; d < 4; d++) p.c[d] = COLB(ladoc[k], o, d);
                grid[k] = p;
            }
            res = verificar(grid) ? 1 : -1;
        }
    }
    for (int x = 0; x < hilos; x++) { HiloU *h = &hs[x].u; free(h->dom); free(h->puesto); free(h->solucion); free(h->cint); free(h->cori); free(hs[x].dom0); free(hs[x].rest); free(hs[x].camino); free(hs[x].wniv); free(hs[x].sub); free(hs[x].cedido); free(hs[x].puntaje); }
    free(hs); free(th);
    return res;
}

/* ------------------------------------------------------------------ programa principal */
typedef struct { double coste, est; int perm[3], orden, c0; } Grupo;
static Pieza *grid_pre(void) { static Pieza *g = NULL; if (!g) g = xmalloc(NP * sizeof(Pieza)); return g; }
static int cmp_grupo(const void *a, const void *b) { double x = ((const Grupo *)a)->coste, y = ((const Grupo *)b)->coste; return (x > y) - (x < y); }

int main(int argc, char **argv) {
    const char *ruta = NULL, *salida = NULL; int hilos = 0; long long semilla = -1; int prof = 3;
    int modo = 3;   /* por defecto: unificado (lo más rápido en las pruebas). 0 auto (v3), 1 siempre marcos, 2 siempre lados (v3), 3 siempre unificado (v4) */
    const char *pistas_txt[64]; int npt = 0;
    long long tope_auto = 3000000LL; int tope_dado = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-t") && i + 1 < argc) hilos = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-l") && i + 1 < argc) t_limite = atof(argv[++i]);
        else if (!strcmp(argv[i], "-s") && i + 1 < argc) semilla = atoll(argv[++i]);
        else if (!strcmp(argv[i], "-o") && i + 1 < argc) salida = argv[++i];
        else if (!strcmp(argv[i], "-p") && i + 1 < argc) prof = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-m") && i + 1 < argc) { tope_auto = atoll(argv[++i]); tope_dado = 1; }
        else if (!strcmp(argv[i], "--marcos")) modo = 1;
        else if (!strcmp(argv[i], "--lados")) modo = 2;
        else if (!strcmp(argv[i], "--unificado")) modo = 3;
        else if (!strcmp(argv[i], "--auto")) modo = 0;
        else if (!strcmp(argv[i], "--reparto-fijo")) reparto_dinamico = 0;
        else if (!strcmp(argv[i], "--anillo2") && i + 1 < argc) anillo2 = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--estimar") && i + 1 < argc) estimar_seg = atof(argv[++i]);
        else if (!strcmp(argv[i], "--orden-knuth")) orden_knuth = 1;
        else if (!strcmp(argv[i], "--solo-estimar")) { orden_knuth = 1; solo_estimar = 1; }
        else if (!strcmp(argv[i], "--paridad") && i + 1 < argc) paridad = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--punto-fijo") && i + 1 < argc) punto_fijo = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--forzar") && i + 1 < argc) forzar = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--orden") && i + 1 < argc) orden_casillas = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--portafolio") && i + 1 < argc) portafolio = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--umbral-orilla") && i + 1 < argc) umbral_orilla = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--emparejar") && i + 1 < argc) emparejar = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--reinicio-base") && i + 1 < argc) reinicio_base = atoll(argv[++i]);
        else if (!strcmp(argv[i], "--valor") && i + 1 < argc) valor_lcv = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--umbral-portafolio") && i + 1 < argc) umbral_portafolio = atof(argv[++i]);
        else if (!strcmp(argv[i], "--ramificar-pieza") && i + 1 < argc) ramificar_pieza = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--semilla-azar") && i + 1 < argc) semilla_azar = atoll(argv[++i]);
        else if (!strcmp(argv[i], "--conteo") && i + 1 < argc) conteo_int = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-w") && i + 1 < argc) orden_peso = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-P") && i + 1 < argc && npt < 64) pistas_txt[npt++] = argv[++i];
        else if (!strcmp(argv[i], "-q")) silencioso = 1;
        else if (argv[i][0] != '-') ruta = argv[i];
        else { fprintf(stderr, "Opción desconocida %s\n", argv[i]); return 1; }
    }
    if (!ruta) {
        fprintf(stderr, "Uso: e2marcos44 TABLERO.txt [opciones]\n"
                        "  -t N       hilos (por defecto: todos los del procesador)\n"
                        "  -l S       límite de tiempo en segundos\n"
                        "  -s K       mezclar y girar las piezas con la semilla K (modelos que vienen resueltos)\n"
                        "  -o ARCH    guardar la solución (solo si pasa la verificación)\n"
                        "  -m N       tope de marcos completos por grupo antes de pasar a firmas por lado (3 000 000)\n"
                        "  --marcos     usar siempre marcos completos\n"
                        "  --unificado  ruta unificada de la versión 4 (mapas por lado) — POR DEFECTO\n"
                        "  --auto       como la versión 3: marcos completos si caben en -m, si no ruta unificada\n"
                        "  --lados      usar la ruta por lados de la versión 3\n"
                        "  --anillo2 N  mapa del segundo anillo: 0 apagado, 1 solo el segundo anillo, 2 todas las filas y columnas (POR DEFECTO)\n"
                        "  --estimar S  segundos (en total) de sondeo de Knuth para estimar el tamaño de los grupos y elegir el orden;\n"
                        "               0 = no estimar; por defecto 3 s desde 8x8 y 0 en tableros más chicos\n"
                        "  --portafolio N  equipos de hilos que recorren el mismo grupo; gana el primero que encuentra o termina:\n"
                        "               POR DEFECTO automático: 5 si Knuth estima más de 1e9 nodos en el grupo, si no 0;\n"
                        "               0 = una sola estrategia con todos los hilos; 2 = un equipo por orden;\n"
                        "               3 = equipo completo + dos equipos de reinicios al azar;\n"
                        "               4 = dos órdenes, normal y al azar;\n"
                        "               5 = tres estrategias: normal, pieza menos restrictiva y reinicios al azar\n"
                        "  --umbral-portafolio X  nodos estimados desde los que se usa el portafolio automático (1e9)\n"
                        "  --valor N    1 = probar primero la pieza menos restrictiva (deja más opciones a las vecinas)\n"
                        "  --reinicio-base N  nodos del primer tramo de los reinicios (20000; luego crece con la serie de Luby)\n"
                        "  --semilla-azar K  semilla de los equipos al azar\n"
                        "  --orden N    0 casilla con menos opciones, 1 interior primero (orilla al final);\n"
                        "               por defecto automático: Knuth prueba los dos en cada grupo y elige el más chico\n"
                        "  --orden-knuth  estimar todos los grupos al principio y recorrerlos del más chico al más grande\n"
                        "  --reparto-fijo  reparto de ramas como en v4 (para comparar); por defecto: reparto dinámico v4.1\n"
                        "  -P F,C,K[,G] pieza fija: fila F, columna C (desde 1), pieza K del archivo (desde 1),\n"
                        "               G giros horarios opcionales respecto al archivo (sin G: cualquier orientación)\n"
                        "               con piezas fijas se prueban las 4 esquinas arriba a la izquierda (hasta 24 grupos)\n"
                        "  -q         mostrar solo la línea de resultado\n");
        return 1;
    }
#ifdef _WIN32
    SetConsoleOutputCP(65001);
#endif
    setvbuf(stdout, NULL, _IONBF, 0);   /* cada línea sale al momento (para la ventana) */
    if (!leer_tablero(ruta)) return 1;
    if (semilla >= 0) mezclar((u64)semilla);
    if (hilos <= 0) hilos = num_cpus();
    if (modo == 1 && !tope_dado) tope_auto = 50000000LL;
    if (estimar_seg < 0) estimar_seg = N >= 8 ? 3 : 0;
    if (orden_knuth && estimar_seg <= 0) estimar_seg = 3;
    t_inicio = ahora();
    if (!clasificar()) return 1;
    for (int i = 0; i < npt; i++) {
        int f, c, k, g = -1;
        int n = sscanf(pistas_txt[i], "%d,%d,%d,%d", &f, &c, &k, &g);
        if (n < 3 || f < 1 || f > N || c < 1 || c > N || k < 1 || k > NP) { fprintf(stderr, "Pista inválida: %s\n", pistas_txt[i]); return 1; }
        pistas[npistas].r = f - 1; pistas[npistas].c = c - 1; pistas[npistas].giros = n >= 4 ? (g & 3) : -1;
        pistas[npistas].orig = piezas_orig[k - 1]; pistas[npistas].canon = canonica(piezas_orig[k - 1]); npistas++;
    }
    if (npistas && modo != 3) modo = 3;      /* las pistas se aplican en la ruta unificada */
    if (NC > 64 && modo == 3) { fprintf(stderr, "Demasiados colores para la ruta unificada\n"); return 1; }
    const int L = 4 * M;
    if (!silencioso) printf("[0] Tablero %dx%d, %d colores, %d hilos\n", N, N, NC - 1, hilos);
    preparar_vecinos();
    preparar_unificado(); preparar_anillo2();

    /* v4.4 (observación de Carlos): con piezas fijas el tablero ya no se puede girar, así que la esquina
     * de arriba a la izquierda no se puede fijar: se prueban las 4 (hasta 4 x 6 = 24 grupos). */
    Grupo grupos[24]; int ng = 0;
    cnt_ori = xmalloc(ntori * sizeof(int));
    int nc0 = npistas ? 4 : 1;
    for (int c0 = 0; c0 < nc0; c0++) {
    int resto[3], nr = 0; for (int q = 0; q < 4; q++) if (q != c0) resto[nr++] = q;
    int perms[6][3] = {{0,1,2},{0,2,1},{1,0,2},{1,2,0},{2,0,1},{2,1,0}};
    for (int g = 0; g < 6; g++) {
        int pp[3] = {resto[perms[g][0]], resto[perms[g][1]], resto[perms[g][2]]};
        Esq e0 = esq[c0], e1 = esq[pp[0]], e2 = esq[pp[1]], e3 = esq[pp[2]]; int dup = 0;
        for (int h = 0; h < ng; h++) {
            Esq f0 = esq[grupos[h].c0], f1 = esq[grupos[h].perm[0]], f2 = esq[grupos[h].perm[1]], f3 = esq[grupos[h].perm[2]];
            if (e0.a == f0.a && e0.b == f0.b && e1.a == f1.a && e1.b == f1.b && e2.a == f2.a && e2.b == f2.b && e3.a == f3.a && e3.b == f3.b) dup = 1;
        }
        if (dup) continue;
        memcpy(grupos[ng].perm, pp, sizeof(pp)); grupos[ng].c0 = c0;
        Esq e4[4] = {e0, e1, e2, e3}; double coste = 1;
        for (int s = 0; s < 4; s++) coste *= caminos_lado(e4[s].b, e4[(s + 1) & 3].a);   /* caminos del mapa de cada lado */
        grupos[ng].coste = coste; ng++;
    }
    }
    qsort(grupos, ng, sizeof(Grupo), cmp_grupo);
    if (!silencioso) { if (npistas) printf("[1] Con piezas fijas: las 4 esquinas se prueban arriba a la izquierda. %d grupos, orden por coste:", ng); else printf("[1] Esquina fija (%d,%d). %d grupos, orden por coste:", color_original[esq[0].a], color_original[esq[0].b], ng); for (int g = 0; g < ng; g++) printf(" %.3g", grupos[g].coste); printf("\n"); }
    grupo_tot = ng;
    for (int g = 0; g < ng; g++) grupos[g].est = -1;
    int orden_fijo = orden_casillas;
    /* v4.4: por defecto (-1) portafolio AUTOMÁTICO por grupo: si Knuth estima un grupo grande (más de
     * umbral_portafolio nodos), las tres estrategias a la vez (--portafolio 5); si es chico, una sola con todos los hilos. */
    if (portafolio == 1) portafolio = 2;
    if (portafolio == 3 && orden_casillas < 0 && !(estimar_seg > 0)) orden_casillas = 0;
    for (int g = 0; g < ng; g++) grupos[g].orden = orden_fijo >= 0 ? orden_fijo : 0;
    int auto_orden = orden_fijo < 0 && estimar_seg > 0 && modo == 3 && reparto_dinamico;
    if (estimar_seg > 0 && modo == 3 && reparto_dinamico) {
        long long nd0 = 0; int mp0 = 0;
        double seg_total = estimar_seg;
        estimar_seg = seg_total / (ng * (auto_orden ? 2 : 1));   /* el tiempo total se reparte entre grupos y órdenes */
        if (estimar_seg < 0.3) estimar_seg = 0.3;
        for (int g = 0; g < ng; g++) {
            g4[0] = esq[grupos[g].c0]; for (int q = 0; q < 3; q++) g4[q + 1] = esq[grupos[g].perm[q]];
            grupo_act = g + 1;
            double mejor = -1; int mo = grupos[g].orden;
            for (int o = auto_orden ? 0 : grupos[g].orden; o <= (auto_orden ? 1 : grupos[g].orden); o++) {
                orden_casillas = o;
                buscar_dinamico(hilos, &nd0, &mp0, grid_pre(), -2);
                if (auto_orden && !silencioso) printf("    grupo %d, orden %s: estimado %.3g nodos\n", g + 1, o ? "interior primero" : "menos opciones", est_grupo);
                /* "interior primero" solo si su árbol estimado es al menos 3 veces más chico (el estimado tiene mucho ruido) */
                if (mejor < 0 || (o == 0 ? est_grupo < mejor : est_grupo * 3 < mejor)) { mejor = est_grupo; mo = o; }
            }
            grupos[g].orden = mo; grupos[g].est = mejor;
            if (orden_knuth) grupos[g].coste = mejor;
        }
        estimar_seg = seg_total;
        if (orden_knuth) qsort(grupos, ng, sizeof(Grupo), cmp_grupo);
        if (!silencioso || solo_estimar) { printf("[1] %s (nodos):", orden_knuth ? "Grupos ordenados por estimado de Knuth" : "Estimado de Knuth por grupo"); for (int g = 0; g < ng; g++) printf(" %.3g%s", grupos[g].est, grupos[g].orden ? "(i)" : ""); printf("   (i) = interior primero\n"); }
        if (solo_estimar) {
            double tot = 0; for (int g = 0; g < ng; g++) tot += grupos[g].est;
            printf("ESTIMADO total=%.4g grupos=%d ms=%.1f\n", tot, ng, (ahora() - t_inicio) * 1000);
            return 0;
        }
    }

    col_d = xmalloc(L + 1); col_t = xmalloc(L + 1); cad_d = xmalloc(M + 1); cad_t = xmalloc(M + 1);
    SetFirmas descartadas; set_init(&descartadas, L, 0);
    long long nodos_total = 0, marcos_total = 0, compr_total = 0; int resultado = 0, grupos_recorridos = 0, uso_lados = 0, uso_unif = 0, maxprof = 0;
    Pieza *grid = xmalloc(NP * sizeof(Pieza));
    double t_marcos = 0, t_interior = 0;

    for (int gi = 0; gi < ng && resultado != 1; gi++) {
        if (atomic_load(&detener)) break;
        grupos_recorridos = gi + 1; grupo_act = gi + 1; orden_casillas = grupos[gi].orden;
        portafolio_grupo = portafolio >= 0 ? portafolio : ((grupos[gi].est > umbral_portafolio && hilos >= 3) ? 5 : 0);
        g4[0] = esq[grupos[gi].c0]; for (int q = 0; q < 3; q++) g4[q + 1] = esq[grupos[gi].perm[q]];
        double t0 = ahora();
        int ruta_lados = (modo == 2), ruta_unif = (modo == 3);
        SetFirmas sg; memset(&sg, 0, sizeof(sg));
        if (!ruta_lados && !ruta_unif) {
            /* etapas 3-4: marcos completos y firmas */
            SetFirmas sf; set_init(&sf, L, 1); set_actual = &sf; marcos_grupo = 0; demasiados = 0; tope_marcos = tope_auto;
            for (int j = 0; j < ntori; j++) cnt_ori[j] = tori[j].cant;
            dfs_marco(0, 0, g4[0].b);
            if (demasiados) {
                set_free(&sf);
                if (modo == 1) { printf("AVISO: el grupo %d supera el tope de %lld marcos (usa -m)\n", gi + 1, tope_auto); resultado = -2; break; }
                ruta_unif = 1;
                if (!silencioso) printf("[2] Grupo %d/%d: más de %lld marcos -> RUTA UNIFICADA (mapas por lado)\n", gi + 1, ng, tope_auto);
            } else {
                marcos_total += marcos_grupo;
                set_init(&sg, L, 1); int quitadas = 0;
                for (int i = 0; i < sf.n; i++) {
                    const u8 *f = sf.datos + (size_t)i * L;
                    if (set_buscar(&descartadas, f) >= 0) { quitadas++; continue; }
                    set_insertar(&sg, f, sf.ejemplo + (size_t)i * L);
                }
                set_free(&sf);
                if (!silencioso) printf("[2] Grupo %d/%d: %lld marcos -> %d firmas%s\n", gi + 1, ng, marcos_grupo, sg.n + quitadas, sg.n == 0 ? "  -> sin marcos, se descarta" : "");
                if (quitadas && !silencioso) printf("    quitadas %d firmas ya probadas; quedan %d\n", quitadas, sg.n);
                if (sg.n == 0) { set_free(&sg); t_marcos += ahora() - t0; continue; }
                preparar_marcos(&sg);
            }
        }
        if (ruta_lados) {
            uso_lados = 1;
            Cadenas cads[4]; memset(cads, 0, sizeof(cads)); int vacio = 0;
            for (int s = 0; s < 4 && !vacio; s++) {
                set_init(&cads[s].clave, 2 * M, 0); cad_actual = &cads[s]; cad_contadas = 0; cad_excede = 0;
                for (int j = 0; j < ntori; j++) cnt_ori[j] = tori[j].cant;
                dfs_cadena(0, g4[s].b, g4[(s + 1) & 3].a);
                set_free(&cads[s].clave);
                if (cad_excede) { printf("AVISO: un lado del grupo %d supera %lld cadenas\n", gi + 1, tope_cadenas); resultado = -2; vacio = 1; }
                else if (cads[s].n == 0) vacio = 1;
            }
            if (resultado == -2) break;
            if (!vacio) preparar_lados(cads);
            if (!silencioso) {
                if (vacio) printf("[2] Grupo %d/%d: algún lado sin cadenas -> se descarta\n", gi + 1, ng);
                else printf("[2] Grupo %d/%d (por lados): cadenas %d/%d/%d/%d, firmas por lado %d/%d/%d/%d\n", gi + 1, ng,
                            cads[0].n, cads[1].n, cads[2].n, cads[3].n, PR.nsig[0], PR.nsig[1], PR.nsig[2], PR.nsig[3]);
            }
            for (int s = 0; s < 4; s++) { free(cads[s].firma); free(cads[s].tipos); }
            if (vacio) { t_marcos += ahora() - t0; continue; }
        }
        t_marcos += ahora() - t0;

        /* etapas 5-6: interior con todos los hilos en este grupo */
        double t1 = ahora();
        int r;
        if (ruta_unif) {
            uso_unif = 1;
            if (!silencioso && modo == 3) printf("[2] Grupo %d/%d: ruta unificada (mapas por lado%s), orden: %s\n", gi + 1, ng, npistas ? ", con piezas fijas" : "", portafolio_grupo == 5 ? (orden_casillas ? "interior primero; grupo grande: 3 estrategias a la vez (normal, pieza menos restrictiva, reinicios)" : "menos opciones; grupo grande: 3 estrategias a la vez (normal, pieza menos restrictiva, reinicios)") : portafolio_grupo == 3 ? "equipo completo + reinicios" : portafolio_grupo >= 4 ? "PORTAFOLIO de 4 equipos (2 órdenes, normal y al azar)" : portafolio_grupo ? "PORTAFOLIO (mitad de los hilos con cada orden)" : (orden_casillas ? "interior primero" : "casilla con menos opciones"));
            r = reparto_dinamico ? buscar_dinamico(hilos, &nodos_total, &maxprof, grid, grupos[gi].est) : buscar_unificado(hilos, prof, &nodos_total, &maxprof, grid);
        } else r = buscar_interior(hilos, prof, &nodos_total, &compr_total, grid, &sg);
        t_interior += ahora() - t1;
        if (r != 0) {
            resultado = r;
            if (!silencioso) printf("[4] Grupo %d: SOLUCIÓN. Verificación: %s\n", gi + 1, r == 1 ? "OK" : "FALLÓ");
        } else if (atomic_load(&detener)) {
            if (!silencioso) printf("[4] Grupo %d: cortado por límite de tiempo\n", gi + 1);
        } else if (!ruta_lados && !ruta_unif) {
            if (!silencioso) printf("[4] Grupo %d: sin solución; sus %d firmas se descartan para los siguientes\n", gi + 1, sg.n);
            for (int i = 0; i < sg.n; i++) set_insertar(&descartadas, sg.datos + (size_t)i * L, NULL);
        } else if (!silencioso) printf("[4] Grupo %d: sin solución\n", gi + 1);
        set_free(&sg);
    }
    double total = ahora() - t_inicio;
    if (resultado == 1 && salida) {
        FILE *f = fopen(salida, "w");
        if (f) { for (int r = 0; r < N; r++) { for (int c = 0; c < N; c++) { Pieza p = grid[r * N + c]; fprintf(f, "%s%d %d %d %d", c ? "   " : "", color_original[p.c[0]], color_original[p.c[1]], color_original[p.c[2]], color_original[p.c[3]]); } fprintf(f, "\n"); } fclose(f); }
    }
    if (!silencioso) { char st[48]; fmt_tiempo(total, st, sizeof st); printf("[5] Tiempo total: %s\n", st); }
    printf("RESULTADO resuelto=%d ms=%.1f marcos=%lld nodos=%lld grupos=%d/%d hilos=%d t_marcos=%.3f t_interior=%.3f lados=%d unificado=%d max_colocadas=%d%s\n",
           resultado == 1, total * 1000, marcos_total, nodos_total, grupos_recorridos, ng, hilos, t_marcos, t_interior, uso_lados, uso_unif, uso_unif ? maxprof + 4 : 0,
           atomic_load(&detener) ? " motivo=limite" : (resultado == -2 ? " motivo=tope" : (resultado == -1 ? " motivo=verificacion_fallida" : "")));
    return resultado == 1 ? 0 : 3;
}
