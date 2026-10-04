/* nucleo2.cu — v6 optimizada (proyecto de Carlos Edison Guzman Marte).
 * Cada tablero es un arreglo de NN números de 16 bits: código = pieza*4 + giro.
 * Mientras trabaja, el tablero vive en la memoria compartida (rápida) de la tarjeta.
 * col4[código] = los 4 colores de esa pieza girada, empaquetados (N | E<<8 | S<<16 | O<<24). */
#ifdef __CUDACC_RTC__
  #define __expf_compat __expf
  #define DEV __device__ __forceinline__
  typedef unsigned long long u64; typedef unsigned short u16; typedef unsigned int u32;
#else
  #define __expf_compat expf
  #define DEV static inline
  #include <math.h>
  #include <stdlib.h>
  #include <string.h>
  typedef unsigned long long u64; typedef unsigned short u16; typedef unsigned int u32;
#endif

DEV u64 azar(u64 *s) { u64 x = *s; x ^= x >> 12; x ^= x << 25; x ^= x >> 27; *s = x; return x * 2685821657736338717ULL; }
DEV int azar_n(u64 *s, int m) { return (int)(((azar(s) >> 32) * (u64)(unsigned)m) >> 32); }
DEV float azar01(u64 *s) { return (float)(azar(s) >> 40) * (1.0f / 16777216.0f); }
DEV int lado(const u32 *col4, u16 v, int d) { return (col4[v] >> (8 * d)) & 255; }

DEV int local(const u32 *col4, const int *vec, const u16 *t, int c, int ST) {
    u32 me = col4[t[c * ST]]; int s = 0;
    #pragma unroll
    for (int d = 0; d < 4; d++) { int o = vec[c * 4 + d]; if (o < 0) continue;
        s += (int)(((me >> (8 * d)) & 255) == ((col4[t[o * ST]] >> (8 * ((d + 2) & 3))) & 255)); }
    return s;
}
DEV int encaja_dir(const u32 *col4, const u16 *t, int a, int d, int b, int ST) { return lado(col4, t[a * ST], d) == lado(col4, t[b * ST], (d + 2) & 3); }
DEV int dir_vecino(const int *vec, int a, int b) { for (int d = 0; d < 4; d++) if (vec[a * 4 + d] == b) return d; return -1; }

/* pone la pieza p en la casilla c con su mejor giro (o el giro fijo de orilla/esquina) */
DEV void poner(const u32 *col4, const int *vec, const signed char *rotfija, int NN, u16 *t, int c, int p, u64 *s, int ST) {
    int rf = rotfija[p * NN + c];
    if (rf >= 0) { t[c * ST] = (u16)(p * 4 + rf); return; }
    int mejor = -1, mg = 0, emp = 0;
    for (int r = 0; r < 4; r++) { t[c * ST] = (u16)(p * 4 + r); int v = local(col4, vec, t, c, ST);
        if (v > mejor) { mejor = v; mg = r; emp = 1; } else if (v == mejor && azar_n(s, ++emp) == 0) mg = r; }
    t[c * ST] = (u16)(p * 4 + mg);
}

DEV void correr(u16 *t, int ST, int b, int NN, int NE, int K, const u32 *col4, const int *vec, const signed char *rotfija,
                const int *libres, const int *nlib, u16 *TAB, int *PUNT, int *MEJ, u16 *MTAB, const float *TEMP, u64 *RNG, long long *ACEPT)
{
    u64 s = RNG[b]; float T = TEMP[b]; int punt = PUNT[b], mej = MEJ[b]; long long ac = 0;
    float invT = 1.0f / T;
    int n0 = nlib[0], n1 = nlib[1], n2 = nlib[2], ntot = n0 + n1 + n2;
    for (int it = 0; it < K && punt < NE; it++) {
        int u = azar_n(&s, ntot), k = 0;
        if (u >= n0) { u -= n0; k = 1; if (u >= n1) { u -= n1; k = 2; } }
        int nk = k == 0 ? n0 : (k == 1 ? n1 : n2);
        int i = libres[k * NN + u], d;
        if (k == 0 && azar01(&s) < 0.3f) {
            u16 v0 = t[i * ST]; int antes = local(col4, vec, t, i, ST);
            t[i * ST] = (u16)((v0 & ~3) | ((v0 + 1 + azar_n(&s, 3)) & 3));
            d = local(col4, vec, t, i, ST) - antes;
            if (d >= 0 || azar01(&s) < __expf_compat((float)d * invT)) { punt += d; ac++; } else t[i * ST] = v0;
        } else {
            if (nk < 2) continue;
            int j = libres[k * NN + azar_n(&s, nk)];
            if (j == i) continue;
            int dij = dir_vecino(vec, i, j);
            int antes = local(col4, vec, t, i, ST) + local(col4, vec, t, j, ST) - (dij >= 0 ? encaja_dir(col4, t, i, dij, j, ST) : 0);
            u16 vi = t[i * ST], vj = t[j * ST];
            poner(col4, vec, rotfija, NN, t, i, vj >> 2, &s, ST);
            poner(col4, vec, rotfija, NN, t, j, vi >> 2, &s, ST);
            int despues = local(col4, vec, t, i, ST) + local(col4, vec, t, j, ST) - (dij >= 0 ? encaja_dir(col4, t, i, dij, j, ST) : 0);
            d = despues - antes;
            if (d >= 0 || azar01(&s) < __expf_compat((float)d * invT)) { punt += d; ac++; } else { t[i * ST] = vi; t[j * ST] = vj; }
        }
        if (punt > mej) { mej = punt; u16 *m = MTAB + (long long)b * NN; for (int c = 0; c < NN; c++) m[c] = t[c * ST]; }
    }
    RNG[b] = s; PUNT[b] = punt; MEJ[b] = mej; ACEPT[b] += ac;
}


/* ------------------------------------------------------------------ cierre exacto (mezcla con el método por marcos)
 * Si un tablero tiene pocas uniones malas, se rehace EXACTAMENTE la zona: las casillas con una unión mala y sus
 * vecinas hasta un radio. Las piezas de esa zona se reacomodan (con cualquier giro) para que todo encaje; la
 * búsqueda elige siempre la casilla con más lados conocidos y menos candidatas. Si lo logra, el tablero queda resuelto. */
#define RMAX 96
DEV int lados_conocidos(const u32 *col4, const int *vec, const u16 *t, int ST, const unsigned char *est, int c, int *req) {
    int k = 0;
    for (int d = 0; d < 4; d++) {
        int o = vec[c * 4 + d];
        if (o < 0) { req[d] = 0; k++; continue; }
        if (est[o] != 1) { req[d] = (int)((col4[t[o * ST]] >> (8 * ((d + 2) & 3))) & 255); k++; } else req[d] = -1;
    }
    return k;
}
DEV int cabe(u32 cc, const int *req) {
    for (int d = 0; d < 4; d++) if (req[d] >= 0 && (int)((cc >> (8 * d)) & 255) != req[d]) return 0;
    return 1;
}
DEV int cierre(u16 *t, int ST, int NN, const u32 *col4, const int *vec, const signed char *rotfija, const unsigned char *fija,
               int radio, int n, long long limite, long long *nodos_out) {
    unsigned char est[1024];         /* 0 afuera (fija o encaja), 1 en la zona y libre, 2 en la zona y puesta */
    int reg[RMAX], nreg = 0; u16 pz[RMAX]; unsigned char usado[RMAX];
    for (int c = 0; c < NN; c++) est[c] = 0;
    for (int c = 0; c < NN; c++) {
        int mala = 0;
        for (int d = 1; d <= 2; d++) { int o = vec[c * 4 + d]; if (o < 0) continue;
            if ((int)((col4[t[c * ST]] >> (8 * d)) & 255) != (int)((col4[t[o * ST]] >> (8 * ((d + 2) & 3))) & 255)) mala = 1; }
        if (!mala) continue;
        /* esta casilla y la de abajo/derecha tienen una unión mala: marcar el radio alrededor de las dos */
        for (int d = 0; d < 3; d++) {
            int base = d == 0 ? c : vec[c * 4 + d];
            if (d > 0 && base < 0) continue;
            if (d > 0) { int o = vec[c * 4 + d];
                if ((int)((col4[t[c * ST]] >> (8 * d)) & 255) == (int)((col4[t[o * ST]] >> (8 * ((d + 2) & 3))) & 255)) continue; }
            int r0 = base / n, c0 = base % n;
            for (int dr = -radio; dr <= radio; dr++) for (int dc = -radio; dc <= radio; dc++) {
                int rr = r0 + dr, cc = c0 + dc; if (rr < 0 || rr >= n || cc < 0 || cc >= n) continue;
                int x = rr * n + cc; if (fija[x] || est[x]) continue;
                if (nreg >= RMAX) return 0;
                est[x] = 1; reg[nreg] = x; pz[nreg] = (u16)(t[x * ST] >> 2); usado[nreg] = 0; nreg++;
            }
        }
    }
    if (nreg == 0) return 0;
    /* respaldo para deshacer */
    u16 resp[RMAX]; for (int i = 0; i < nreg; i++) resp[i] = t[reg[i] * ST];
    int pila_c[RMAX], pila_i[RMAX], pila_r[RMAX]; int prof = 0; long long nodos = 0;
    int req[4];
    /* elegir la primera casilla */
    #define ELEGIR(dest) { int mk = -1, mc = 1 << 30, best = -1; \
        for (int q = 0; q < nreg; q++) { int c = reg[q]; if (est[c] != 1) continue; int rq[4]; int k = lados_conocidos(col4, vec, t, ST, est, c, rq); \
            if (k < mk) continue; int cnt = 0; \
            for (int i2 = 0; i2 < nreg && cnt < mc; i2++) { if (usado[i2]) continue; int p = pz[i2]; int rf = rotfija[p * NN + c]; if (rf == -2) continue; \
                if (rf >= 0) cnt += cabe(col4[p * 4 + rf], rq); else for (int r = 0; r < 4; r++) cnt += cabe(col4[p * 4 + r], rq); } \
            if (k > mk || cnt < mc) { mk = k; mc = cnt; best = c; } } \
        dest = best; }
    int cel; ELEGIR(cel);
    pila_c[0] = cel; pila_i[0] = 0; pila_r[0] = 0;
    int ok = 0;
    while (prof >= 0) {
        int c = pila_c[prof];
        lados_conocidos(col4, vec, t, ST, est, c, req);
        int hallado = 0;
        while (pila_i[prof] < nreg && !hallado) {
            int i = pila_i[prof];
            if (usado[i]) { pila_i[prof]++; pila_r[prof] = 0; continue; }
            int p = pz[i], rf = rotfija[p * NN + c];
            if (rf == -2) { pila_i[prof]++; pila_r[prof] = 0; continue; }
            int r = pila_r[prof];
            if (rf >= 0) { if (r > 0) { pila_i[prof]++; pila_r[prof] = 0; continue; } r = rf; }
            if (r > 3) { pila_i[prof]++; pila_r[prof] = 0; continue; }
            pila_r[prof]++;
            if (rf >= 0) pila_r[prof] = 9;
            if (cabe(col4[p * 4 + r], req)) {
                hallado = 1; usado[i] = 1; t[c * ST] = (u16)(p * 4 + r); est[c] = 2;
            }
        }
        if (hallado) {
            if (++nodos > limite) break;
            if (prof + 1 == nreg) { ok = 1; break; }
            int sig; ELEGIR(sig);
            if (sig < 0) { ok = 1; break; }
            prof++; pila_c[prof] = sig; pila_i[prof] = 0; pila_r[prof] = 0;
            continue;
        }
        /* nada más que probar aquí: retroceder */
        est[c] = 1; prof--;
        if (prof >= 0) {
            int cp = pila_c[prof]; int pp = t[cp * ST] >> 2;
            for (int i = 0; i < nreg; i++) if (usado[i] && pz[i] == pp) { usado[i] = 0; break; }
            est[cp] = 1;
        }
    }
    *nodos_out += nodos;
    if (!ok) { for (int i = 0; i < nreg; i++) t[reg[i] * ST] = resp[i]; return 0; }
    return 1;
}

#ifdef __CUDACC_RTC__
extern "C" __global__ void pasos(int B, int NN, int NE, int K, const u32 *col4, const int *vec, const signed char *rotfija,
        const int *libres, const int *nlib, u16 *TAB, int *PUNT, int *MEJ, u16 *MTAB, const float *TEMP, u64 *RNG, long long *ACEPT) {
    extern __shared__ u32 shm[];
    u32 *scol = shm;                                   /* tabla de colores: NN*4 */
    int *svec = (int *)(shm + NN * 4);                 /* vecinos: NN*4 */
    u16 *sh = (u16 *)(shm + NN * 8);                   /* tableros del bloque */
    for (int x = threadIdx.x; x < NN * 4; x += blockDim.x) { scol[x] = col4[x]; svec[x] = vec[x]; }
    __syncthreads();
    int b = blockIdx.x * blockDim.x + threadIdx.x;
    if (b >= B) return;
    int ST = blockDim.x; u16 *t = sh + threadIdx.x;
    u16 *g = TAB + (long long)b * NN;
    for (int c = 0; c < NN; c++) t[c * ST] = g[c];
    correr(t, ST, b, NN, NE, K, scol, svec, rotfija, libres, nlib, TAB, PUNT, MEJ, MTAB, TEMP, RNG, ACEPT);
    for (int c = 0; c < NN; c++) g[c] = t[c * ST];
}
#else
void pasos_cpu(int B, int NN, int NE, int K, const u32 *col4, const int *vec, const signed char *rotfija,
        const int *libres, const int *nlib, u16 *TAB, int *PUNT, int *MEJ, u16 *MTAB, const float *TEMP, u64 *RNG, long long *ACEPT) {
    #pragma omp parallel for schedule(dynamic, 16)
    for (int b = 0; b < B; b++) {
        u16 t[1024]; memcpy(t, TAB + (long long)b * NN, NN * sizeof(u16));
        correr(t, 1, b, NN, NE, K, col4, vec, rotfija, libres, nlib, TAB, PUNT, MEJ, MTAB, TEMP, RNG, ACEPT);
        memcpy(TAB + (long long)b * NN, t, NN * sizeof(u16));
    }
}
#endif

/* intento de cierre para los tableros con pocas uniones malas */
#ifdef __CUDACC_RTC__
extern "C" __global__ void cerrar(int B, int NN, int NE, int n, int kmax, int radio, long long limite, const u32 *col4, const int *vec,
        const signed char *rotfija, const unsigned char *fija, u16 *TAB, int *PUNT, int *MEJ, u16 *MTAB, long long *NODOS, int *INTENTOS) {
    int b = blockIdx.x * blockDim.x + threadIdx.x;
    if (b >= B || PUNT[b] < NE - kmax || PUNT[b] == NE) return;
    u16 *t = TAB + (long long)b * NN;
    INTENTOS[b]++;
    if (cierre(t, 1, NN, col4, vec, rotfija, fija, radio, n, limite, &NODOS[b])) {
        PUNT[b] = NE; MEJ[b] = NE; u16 *m = MTAB + (long long)b * NN; for (int c = 0; c < NN; c++) m[c] = t[c];
    }
}
#else
void cerrar_cpu(int B, int NN, int NE, int n, int kmax, int radio, long long limite, const u32 *col4, const int *vec,
        const signed char *rotfija, const unsigned char *fija, u16 *TAB, int *PUNT, int *MEJ, u16 *MTAB, long long *NODOS, int *INTENTOS) {
    #pragma omp parallel for schedule(dynamic, 1)
    for (int b = 0; b < B; b++) {
        if (PUNT[b] < NE - kmax || PUNT[b] == NE) continue;
        u16 *t = TAB + (long long)b * NN;
        INTENTOS[b]++;
        if (cierre(t, 1, NN, col4, vec, rotfija, fija, radio, n, limite, &NODOS[b])) {
            PUNT[b] = NE; MEJ[b] = NE; u16 *m = MTAB + (long long)b * NN; for (int c = 0; c < NN; c++) m[c] = t[c];
        }
    }
}
#endif
