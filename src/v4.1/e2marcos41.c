/*
 * e2marcos41 — Motor (versión 4.1: reparto dinámico del trabajo entre hilos)
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
    return propagar_lados(h, 0, 15);
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

typedef struct { int len; int *camino; } Tarea;
static Tarea *cola_t = NULL; static int cola_cap = 0, cola_ini = 0, cola_fin = 0;
static pthread_mutex_t cola_mx = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cola_cv = PTHREAD_COND_INITIALIZER;
static atomic_int hambre;        /* hilos esperando trabajo */
static atomic_int en_cola;       /* tareas en la cola */
static int activos_d = 0;        /* hilos procesando una tarea (protegido por cola_mx) */
static atomic_llong cedidas;     /* tareas creadas por cesión (estadística) */

static void cola_meter(const int *camino, int len) {   /* llamar con cola_mx tomado */
    if (cola_fin == cola_cap) {
        if (cola_ini > 0) { memmove(cola_t, cola_t + cola_ini, (cola_fin - cola_ini) * sizeof(Tarea)); cola_fin -= cola_ini; cola_ini = 0; }
        if (cola_fin == cola_cap) { cola_cap = cola_cap ? cola_cap * 2 : 256; cola_t = realloc(cola_t, cola_cap * sizeof(Tarea)); if (!cola_t) { fprintf(stderr, "Sin memoria\n"); exit(2); } }
    }
    Tarea *t = &cola_t[cola_fin++]; t->len = len; t->camino = xmalloc((len + 1) * sizeof(int));
    memcpy(t->camino, camino, len * sizeof(int));
    atomic_fetch_add(&en_cola, 1);
}

typedef struct {
    HiloU u;                /* estado de búsqueda (dominios, piezas puestas, conteos) */
    u64 *dom0; int vac0;    /* estado inicial del grupo, para reconstruir cada tarea */
    u64 *rest;              /* [nvar+2][DW]: opciones aún no probadas en cada nivel */
    int *camino;            /* opción elegida en cada nivel */
    int base;               /* primer nivel propio de la tarea actual */
    long long cesiones;
} HiloD;

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
        if (v < mejor_v) { mejor_v = v; mejor = k; }
    }
    for (int t = 0; t < ntint; t++) if (h->cint[t] > 0) { const u64 *mt = MT(t); int ok = 0; for (int w = 0; w < OW; w++) if (uni_i[w] & mt[w]) { ok = 1; break; } if (!ok) return -1; }
    for (int t = 0; t < ntori; t++) if (h->cori[t] > 0) {
        if (!((uni_b[t >> 6] >> (t & 63)) & 1)) return -1;
        if (cabe && cabe[t] < h->cori[t]) return -1;
    }
    return mejor;
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
    return ok;
}
static inline void deshacer_u(HiloU *h, int k, int o) {
    h->puesto[k] = -1;
    if (tipoc[k] == 0) h->cint[orient[o].tipo]++; else h->cori[o]++;
}

/* cede a la cola las opciones no probadas de los niveles menos profundos */
static void ceder(HiloD *hd, int lv) {
    int quiere = atomic_load_explicit(&hambre, memory_order_relaxed), dadas = 0;
    pthread_mutex_lock(&cola_mx);
    for (int L = hd->base; L < lv && dadas < quiere; L++) {
        u64 *R = hd->rest + (size_t)L * DW; int actual = hd->camino[L];   /* la opción que este hilo explora ahora */
        for (int w = 0; w < DW; w++) {
            u64 x = R[w];
            while (x) { int o = w * 64 + ctz64(x); x &= x - 1; hd->camino[L] = o; cola_meter(hd->camino, L + 1); dadas++; }
            R[w] = 0;
        }
        hd->camino[L] = actual;
    }
    if (dadas) { hd->cesiones += dadas; atomic_fetch_add(&cedidas, dadas); pthread_cond_broadcast(&cola_cv); }
    pthread_mutex_unlock(&cola_mx);
}

static int rec_d(HiloD *hd, int lv, int vacias) {
    HiloU *h = &hd->u;
    h->nodos++;
    if (lv > h->maxprof) h->maxprof = lv;
    if ((h->nodos & 255) == 0) {
        if ((h->nodos & 1023) == 0) {
            if (atomic_load_explicit(&resuelto, memory_order_relaxed) || atomic_load_explicit(&detener, memory_order_relaxed)) return -1;
            if (t_limite > 0 && ahora() - t_inicio > t_limite) { atomic_store(&detener, 1); return -1; }
        }
        if (atomic_load_explicit(&hambre, memory_order_relaxed) > 0 && atomic_load_explicit(&en_cola, memory_order_relaxed) == 0) ceder(hd, lv);
    }
    if (vacias == 0) { memcpy(h->solucion, h->puesto, NN * sizeof(int)); h->encontrado = 1; return 1; }
    int k = seleccionar_u(h, lv);
    if (k < 0) return 0;
    u64 *R = hd->rest + (size_t)lv * DW; const u64 *Dk = DOM(h, lv, k);
    int nw = palabras(k); for (int w = 0; w < DW; w++) R[w] = w < nw ? Dk[w] : 0;
    for (;;) {
        int o = -1;
        for (int w = 0; w < nw; w++) if (R[w]) { o = w * 64 + ctz64(R[w]); R[w] &= R[w] - 1; break; }
        if (o < 0) break;
        hd->camino[lv] = o;
        int ok = aplicar_u(h, lv, k, o);
        int r = ok ? rec_d(hd, lv + 1, vacias - 1) : 0;
        deshacer_u(h, k, o);
        if (r != 0) return r;
    }
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
        int k = seleccionar_u(h, lv); if (k < 0) return 0;
        int o = t->camino[i]; hd->camino[lv] = o;
        if (!aplicar_u(h, lv, k, o)) return 0;
        lv++; vac--;
    }
    hd->base = lv;
    return rec_d(hd, lv, vac);
}

static void *hilo_d(void *arg) {
    HiloD *hd = arg;
    for (;;) {
        pthread_mutex_lock(&cola_mx);
        atomic_fetch_add(&hambre, 1);
        while (cola_ini == cola_fin && activos_d > 0 && !atomic_load(&resuelto) && !atomic_load(&detener))
            pthread_cond_wait(&cola_cv, &cola_mx);
        atomic_fetch_sub(&hambre, 1);
        if (cola_ini == cola_fin || atomic_load(&resuelto) || atomic_load(&detener)) {   /* no queda trabajo */
            pthread_cond_broadcast(&cola_cv); pthread_mutex_unlock(&cola_mx); break;
        }
        Tarea t = cola_t[cola_ini++]; atomic_fetch_sub(&en_cola, 1); activos_d++;
        pthread_mutex_unlock(&cola_mx);
        int r = ejecutar_tarea(hd, &t);
        free(t.camino);
        if (r == 1) atomic_store(&resuelto, 1);
        pthread_mutex_lock(&cola_mx); activos_d--; pthread_cond_broadcast(&cola_cv); pthread_mutex_unlock(&cola_mx);
    }
    atomic_fetch_add(&terminados, 1);
    return NULL;
}

static int buscar_dinamico(int hilos, long long *nodos_out, int *maxprof_out, Pieza *grid) {
    HiloD *hs = xmalloc(hilos * sizeof(HiloD)); pthread_t *th = xmalloc(hilos * sizeof(pthread_t));
    atomic_store(&resuelto, 0); atomic_store(&terminados, 0); atomic_store(&hambre, 0); atomic_store(&en_cola, 0);
    cola_ini = cola_fin = 0; activos_d = 0;
    int posible = 1;
    for (int x = 0; x < hilos; x++) {
        HiloD *hd = &hs[x]; HiloU *h = &hd->u; h->id = x; h->nhilos = hilos;
        h->dom = xmalloc((size_t)(nvar + 2) * NN * DW * sizeof(u64));
        h->puesto = xmalloc(NN * sizeof(int)); h->solucion = xmalloc(NN * sizeof(int));
        h->cint = xmalloc((ntint + 1) * sizeof(int)); h->cori = xmalloc((ntori + 1) * sizeof(int));
        hd->dom0 = xmalloc((size_t)NN * DW * sizeof(u64));
        hd->rest = xmalloc((size_t)(nvar + 2) * DW * sizeof(u64)); hd->camino = xmalloc((nvar + 2) * sizeof(int));
        if (posible && !iniciar_grupo_u(h)) posible = 0;
        memcpy(hd->dom0, DOM(h, 0, 0), (size_t)NN * DW * sizeof(u64));
        hd->vac0 = 0; for (int i = 0; i < nvar; i++) if (h->puesto[var_celdas[i]] < 0) hd->vac0++;
    }
    int res = 0;
    if (posible) {
        pthread_mutex_lock(&cola_mx); cola_meter(NULL, 0); pthread_mutex_unlock(&cola_mx);   /* tarea raíz */
        for (int x = 0; x < hilos; x++) pthread_create(&th[x], NULL, hilo_d, &hs[x]);
        double ultimo = ahora();
        while (atomic_load(&terminados) < hilos) {
            dormir_ms(5);
            if (!silencioso && ahora() - ultimo >= 2.0) {
                long long nd = 0; int mp = 0; for (int x = 0; x < hilos; x++) { nd += hs[x].u.nodos; if (hs[x].u.maxprof > mp) mp = hs[x].u.maxprof; }
                pthread_mutex_lock(&cola_mx); int act = activos_d; pthread_mutex_unlock(&cola_mx);
                printf("    ... %.1f s, %lld nodos, máximo colocado: %d de %d piezas, hilos trabajando: %d de %d\n", ahora() - t_inicio, nd, mp + 4, NP, act, hilos);
                ultimo = ahora();
            }
        }
        int gan = -1;
        for (int x = 0; x < hilos; x++) { pthread_join(th[x], NULL); *nodos_out += hs[x].u.nodos; if (hs[x].u.maxprof > *maxprof_out) *maxprof_out = hs[x].u.maxprof; if (hs[x].u.encontrado && gan < 0) gan = x; }
        pthread_mutex_lock(&cola_mx); for (int i = cola_ini; i < cola_fin; i++) free(cola_t[i].camino); cola_ini = cola_fin = 0; pthread_mutex_unlock(&cola_mx);
        atomic_store(&en_cola, 0);
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
    for (int x = 0; x < hilos; x++) { HiloU *h = &hs[x].u; free(h->dom); free(h->puesto); free(h->solucion); free(h->cint); free(h->cori); free(hs[x].dom0); free(hs[x].rest); free(hs[x].camino); }
    free(hs); free(th);
    return res;
}

/* ------------------------------------------------------------------ programa principal */
typedef struct { double coste; int perm[3]; } Grupo;
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
        else if (!strcmp(argv[i], "-w") && i + 1 < argc) orden_peso = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-P") && i + 1 < argc && npt < 64) pistas_txt[npt++] = argv[++i];
        else if (!strcmp(argv[i], "-q")) silencioso = 1;
        else if (argv[i][0] != '-') ruta = argv[i];
        else { fprintf(stderr, "Opción desconocida %s\n", argv[i]); return 1; }
    }
    if (!ruta) {
        fprintf(stderr, "Uso: e2marcos41 TABLERO.txt [opciones]\n"
                        "  -t N       hilos (por defecto: todos los del procesador)\n"
                        "  -l S       límite de tiempo en segundos\n"
                        "  -s K       mezclar y girar las piezas con la semilla K (modelos que vienen resueltos)\n"
                        "  -o ARCH    guardar la solución (solo si pasa la verificación)\n"
                        "  -m N       tope de marcos completos por grupo antes de pasar a firmas por lado (3 000 000)\n"
                        "  --marcos     usar siempre marcos completos\n"
                        "  --unificado  ruta unificada de la versión 4 (mapas por lado) — POR DEFECTO\n"
                        "  --auto       como la versión 3: marcos completos si caben en -m, si no ruta unificada\n"
                        "  --lados      usar la ruta por lados de la versión 3\n"
                        "  --reparto-fijo  reparto de ramas como en v4 (para comparar); por defecto: reparto dinámico v4.1\n"
                        "  -P F,C,K[,G] pieza fija: fila F, columna C (desde 1), pieza K del archivo (desde 1),\n"
                        "               G giros horarios opcionales respecto al archivo (sin G: cualquier orientación)\n"
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
    preparar_unificado();

    int perms[6][3] = {{1,2,3},{1,3,2},{2,1,3},{2,3,1},{3,1,2},{3,2,1}};
    Grupo grupos[6]; int ng = 0;
    cnt_ori = xmalloc(ntori * sizeof(int));
    for (int g = 0; g < 6; g++) {
        Esq e1 = esq[perms[g][0]], e2 = esq[perms[g][1]], e3 = esq[perms[g][2]]; int dup = 0;
        for (int h = 0; h < ng; h++) {
            Esq f1 = esq[grupos[h].perm[0]], f2 = esq[grupos[h].perm[1]], f3 = esq[grupos[h].perm[2]];
            if (e1.a == f1.a && e1.b == f1.b && e2.a == f2.a && e2.b == f2.b && e3.a == f3.a && e3.b == f3.b) dup = 1;
        }
        if (dup) continue;
        memcpy(grupos[ng].perm, perms[g], sizeof(perms[g]));
        Esq e4[4] = {esq[0], e1, e2, e3}; double coste = 1;
        for (int s = 0; s < 4; s++) coste *= caminos_lado(e4[s].b, e4[(s + 1) & 3].a);   /* caminos del mapa de cada lado */
        grupos[ng].coste = coste; ng++;
    }
    qsort(grupos, ng, sizeof(Grupo), cmp_grupo);
    if (!silencioso) { printf("[1] Esquina fija (%d,%d). %d grupos, orden por coste:", color_original[esq[0].a], color_original[esq[0].b], ng); for (int g = 0; g < ng; g++) printf(" %.3g", grupos[g].coste); printf("\n"); }

    col_d = xmalloc(L + 1); col_t = xmalloc(L + 1); cad_d = xmalloc(M + 1); cad_t = xmalloc(M + 1);
    SetFirmas descartadas; set_init(&descartadas, L, 0);
    long long nodos_total = 0, marcos_total = 0, compr_total = 0; int resultado = 0, grupos_recorridos = 0, uso_lados = 0, uso_unif = 0, maxprof = 0;
    Pieza *grid = xmalloc(NP * sizeof(Pieza));
    double t_marcos = 0, t_interior = 0;

    for (int gi = 0; gi < ng && resultado != 1; gi++) {
        if (atomic_load(&detener)) break;
        grupos_recorridos = gi + 1;
        g4[0] = esq[0]; for (int q = 0; q < 3; q++) g4[q + 1] = esq[grupos[gi].perm[q]];
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
            if (!silencioso && modo == 3) printf("[2] Grupo %d/%d: ruta unificada (mapas por lado%s)\n", gi + 1, ng, npistas ? ", con piezas fijas" : "");
            r = reparto_dinamico ? buscar_dinamico(hilos, &nodos_total, &maxprof, grid) : buscar_unificado(hilos, prof, &nodos_total, &maxprof, grid);
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
    printf("RESULTADO resuelto=%d ms=%.1f marcos=%lld nodos=%lld grupos=%d/%d hilos=%d t_marcos=%.3f t_interior=%.3f lados=%d unificado=%d max_colocadas=%d%s\n",
           resultado == 1, total * 1000, marcos_total, nodos_total, grupos_recorridos, ng, hilos, t_marcos, t_interior, uso_lados, uso_unif, uso_unif ? maxprof + 4 : 0,
           atomic_load(&detener) ? " motivo=limite" : (resultado == -2 ? " motivo=tope" : (resultado == -1 ? " motivo=verificacion_fallida" : "")));
    return resultado == 1 ? 0 : 3;
}
