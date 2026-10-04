/* nucleo.cu — Recocido con réplicas para tableros tipo Eternity, "piezas reales, uniones que encajan".
 * Autor del proyecto: Carlos Edison Guzman Marte.  Este mismo texto se compila:
 *   - en la tarjeta de video (CuPy / NVRTC): kernel "pasos", un hilo por tablero;
 *   - en el procesador (gcc, solo para probar): función "pasos_cpu".
 * Estado de cada tablero b: pieza y giro en cada casilla (perm, giro), su puntaje y su mejor tablero. */
#ifdef __CUDACC_RTC__
  #define DEV __device__
  typedef unsigned long long u64;
#else
  #define DEV static
  #include <math.h>
  typedef unsigned long long u64;
#endif

DEV inline u64 azar(u64 *s) { u64 x = *s; x ^= x >> 12; x ^= x << 25; x ^= x >> 27; *s = x; return x * 2685821657736338717ULL; }
DEV inline float azar01(u64 *s) { return (float)(azar(s) >> 40) * (1.0f / 16777216.0f); }

/* color del lado d de la pieza p puesta con giro r (giro horario: el lado d muestra el lado (d - r) de la pieza) */
DEV inline int color(const int *pc, int p, int r, int d) { return pc[p * 4 + ((d - r) & 3)]; }

/* uniones que encajan alrededor de la casilla c */
DEV inline int local(const int *pc, const int *vec, const int *perm, const int *giro, int c) {
    int s = 0, p = perm[c], r = giro[c];
    for (int d = 0; d < 4; d++) { int o = vec[c * 4 + d]; if (o < 0) continue;
        if (color(pc, p, r, d) == color(pc, perm[o], giro[o], (d + 2) & 3)) s++; }
    return s;
}
DEV inline int encaja(const int *pc, const int *perm, const int *giro, int a, int d, int b) {
    return color(pc, perm[a], giro[a], d) == color(pc, perm[b], giro[b], (d + 2) & 3);
}
DEV inline int dir_vecino(const int *vec, int a, int b) { for (int d = 0; d < 4; d++) if (vec[a * 4 + d] == b) return d; return -1; }

/* mejor giro para la pieza en la casilla c (si es interior); en orillas y esquinas el giro viene fijo (rotfija) */
DEV inline int poner_mejor_giro(const int *pc, const int *vec, int *perm, int *giro, const int *rotfija, int NN, int c, u64 *s) {
    int p = perm[c], rf = rotfija[p * NN + c];
    if (rf >= 0) { giro[c] = rf; return local(pc, vec, perm, giro, c); }
    int mejor = -1, mg = 0, empate = 0;
    for (int r = 0; r < 4; r++) { giro[c] = r; int v = local(pc, vec, perm, giro, c);
        if (v > mejor) { mejor = v; mg = r; empate = 1; } else if (v == mejor && (int)(azar(s) % (u64)(++empate)) == 0) mg = r; }
    giro[c] = mg; return mejor;
}

DEV void correr(int b, int NN, int NE, int K,
                const int *pc, const int *vec, const int *rotfija,
                const int *libres, const int *nlib,          /* casillas libres por clase: [3][NN], cuántas */
                int *PERM, int *GIRO, int *PUNT, int *MEJ, int *MPERM, int *MGIRO,
                const float *TEMP, u64 *RNG, long long *ACEPT)
{
    int *perm = PERM + (long long)b * NN, *giro = GIRO + (long long)b * NN;
    u64 s = RNG[b]; float T = TEMP[b]; int punt = PUNT[b], mej = MEJ[b]; long long ac = 0;
    int ntot = nlib[0] + nlib[1] + nlib[2];
    for (int it = 0; it < K; it++) {
        if (punt == NE) break;
        int u = (int)(azar(&s) % (u64)ntot), k = 0;
        while (u >= nlib[k]) { u -= nlib[k]; k++; }
        int i = libres[k * NN + u];
        int d;
        if (k == 0 && azar01(&s) < 0.3f) {            /* girar una pieza interior */
            int r0 = giro[i], antes = local(pc, vec, perm, giro, i);
            giro[i] = (r0 + 1 + (int)(azar(&s) % 3ULL)) & 3;
            d = local(pc, vec, perm, giro, i) - antes;
            if (d >= 0 || azar01(&s) < expf((float)d / T)) { punt += d; ac++; }
            else giro[i] = r0;
        } else {                                       /* intercambiar dos piezas de la misma clase */
            if (nlib[k] < 2) continue;
            int j = libres[k * NN + (int)(azar(&s) % (u64)nlib[k])];
            if (j == i) continue;
            int dij = dir_vecino(vec, i, j);
            int antes = local(pc, vec, perm, giro, i) + local(pc, vec, perm, giro, j) - (dij >= 0 ? encaja(pc, perm, giro, i, dij, j) : 0);
            int pi = perm[i], pj = perm[j], gi = giro[i], gj = giro[j];
            perm[i] = pj; perm[j] = pi;
            poner_mejor_giro(pc, vec, perm, giro, rotfija, NN, i, &s);
            poner_mejor_giro(pc, vec, perm, giro, rotfija, NN, j, &s);
            int despues = local(pc, vec, perm, giro, i) + local(pc, vec, perm, giro, j) - (dij >= 0 ? encaja(pc, perm, giro, i, dij, j) : 0);
            d = despues - antes;
            if (d >= 0 || azar01(&s) < expf((float)d / T)) { punt += d; ac++; }
            else { perm[i] = pi; perm[j] = pj; giro[i] = gi; giro[j] = gj; }
        }
        if (punt > mej) {
            mej = punt;
            int *mp = MPERM + (long long)b * NN, *mg = MGIRO + (long long)b * NN;
            for (int c = 0; c < NN; c++) { mp[c] = perm[c]; mg[c] = giro[c]; }
        }
    }
    RNG[b] = s; PUNT[b] = punt; MEJ[b] = mej; ACEPT[b] += ac;
}

#ifdef __CUDACC_RTC__
extern "C" __global__ void pasos(int B, int NN, int NE, int K, const int *pc, const int *vec, const int *rotfija,
        const int *libres, const int *nlib, int *PERM, int *GIRO, int *PUNT, int *MEJ, int *MPERM, int *MGIRO,
        const float *TEMP, u64 *RNG, long long *ACEPT) {
    int b = blockIdx.x * blockDim.x + threadIdx.x;
    if (b < B) correr(b, NN, NE, K, pc, vec, rotfija, libres, nlib, PERM, GIRO, PUNT, MEJ, MPERM, MGIRO, TEMP, RNG, ACEPT);
}
#else
void pasos_cpu(int B, int NN, int NE, int K, const int *pc, const int *vec, const int *rotfija,
        const int *libres, const int *nlib, int *PERM, int *GIRO, int *PUNT, int *MEJ, int *MPERM, int *MGIRO,
        const float *TEMP, u64 *RNG, long long *ACEPT) {
    #pragma omp parallel for schedule(dynamic, 16)
    for (int b = 0; b < B; b++) correr(b, NN, NE, K, pc, vec, rotfija, libres, nlib, PERM, GIRO, PUNT, MEJ, MPERM, MGIRO, TEMP, RNG, ACEPT);
}
#endif
