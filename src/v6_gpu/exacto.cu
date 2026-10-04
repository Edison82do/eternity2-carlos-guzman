/* exacto.cu — búsqueda EXACTA repartida en miles de hilos (v6, proyecto de Carlos Edison Guzman Marte).
 * Recorrido fijo de casillas (por filas o por diagonales): cada casilla ya tiene puestas la de arriba y la de la
 * izquierda, así que los candidatos se buscan en una lista por (clase de casilla, color arriba, color izquierda).
 * El procesador reparte el árbol: genera todos los "prefijos" (tableros parciales) hasta cierta profundidad y cada
 * hilo de la tarjeta agota por completo los subárboles que le tocan. Si todos se agotan sin solución, eso es una
 * PRUEBA de que el tablero no tiene solución (con esas piezas fijas).
 * Mismo código para la tarjeta (NVRTC) y para el procesador (gcc / mingw).
 * código = pieza*4 + giro ; col4[código] = N | E<<8 | S<<16 | O<<24 */
#ifdef __CUDACC_RTC__
  #define DEV __device__ __forceinline__
  typedef unsigned long long u64; typedef unsigned short u16; typedef unsigned int u32;
#else
  #define DEV static inline
  #include <string.h>
  typedef unsigned long long u64; typedef unsigned short u16; typedef unsigned int u32;
  #ifdef _WIN32
    #define EXPORTA __declspec(dllexport)
  #else
    #define EXPORTA
  #endif
#endif
#define MAXN 256
#ifdef __CUDACC_RTC__
  #define LDG(x) __ldg(&(x))
#else
  #define LDG(x) (x)
#endif

/* Datos del tablero (iguales para todos los hilos):
 *  CLS[pos]  clase de la casilla en la posición pos del recorrido (la clase dice qué dos lados se usan de llave);
 *  KEY[pos*4 + 0..3] = (posición vecina A, lado de esa vecina, posición vecina B, lado)  (posición -1 = color 0);
 *  TINI/TNUM[(cls*C + colorA)*C + colorB] = trozo de CAND con los candidatos;
 *  CHK[pos*4 + d] = revisión extra del lado d: -1 nada; >= 0 posición de una vecina ya puesta (su lado opuesto
 *  debe coincidir); <= -2 color exigido (-2 - color) por una pieza fija que se pondrá después. */
#define ARGS_DATOS const u16 *CAND, const int *TINI, const int *TNUM, const int *CLS, const int *KEY, \
                   const int *CHK, const u32 *COL4, int NN, int C
#define PASA_DATOS CAND, TINI, TNUM, CLS, KEY, CHK, COL4, NN, C

/* Avanza la búsqueda desde el estado (d, cod, k, usado). d0 = fondo (no retrocede más allá), dlim = profundidad
 * meta. Devuelve 0 = subárbol agotado, 1 = se acabó el presupuesto (estado guardado), 2 = llegó a dlim.
 * k[d] = siguiente candidato a probar en la posición d. */
DEV int avanzar(ARGS_DATOS, int d0, int dlim, int *pd, u16 *cod, u16 *k, u32 *usado, int us, long long presupuesto, long long *pnodos, int *pmax)
{
    int d = *pd; long long nodos = 0; int r = 1; int dm = *pmax;
    while (nodos < presupuesto) {
        if (d >= dlim) { r = 2; break; }
        int pa = KEY[d * 4], pb = KEY[d * 4 + 2];
        int ca = pa < 0 ? 0 : (int)((COL4[cod[pa]] >> (8 * KEY[d * 4 + 1])) & 255);
        int cb = pb < 0 ? 0 : (int)((COL4[cod[pb]] >> (8 * KEY[d * 4 + 3])) & 255);
        int ix = (CLS[d] * C + ca) * C + cb;
        int ini = LDG(TINI[ix]), num = LDG(TNUM[ix]);
        int req[4];
        for (int e = 0; e < 4; e++) { int x = CHK[d * 4 + e];
            req[e] = x == -1 ? -1 : (x <= -2 ? -2 - x : (int)((COL4[cod[x]] >> (8 * ((e + 2) & 3))) & 255)); }
        int j = k[d], c = -1;
        for (; j < num; j++) {
            int v = LDG(CAND[ini + j]), p = v >> 2;
            if (usado[(p >> 5) * us] & (1u << (p & 31))) continue;
            u32 q = COL4[v];
            if (req[0] >= 0 && (int)(q & 255) != req[0]) continue;
            if (req[1] >= 0 && (int)((q >> 8) & 255) != req[1]) continue;
            if (req[2] >= 0 && (int)((q >> 16) & 255) != req[2]) continue;
            if (req[3] >= 0 && (int)(q >> 24) != req[3]) continue;
            c = v; break;
        }
        if (c >= 0) {
            nodos++; cod[d] = (u16)c; k[d] = (u16)(j + 1); usado[((c >> 2) >> 5) * us] |= 1u << ((c >> 2) & 31);
            d++; if (d < NN) k[d] = 0; if (d > dm) dm = d;
        } else {
            if (d <= d0) { r = 0; break; }
            d--; { int p = cod[d] >> 2; usado[(p >> 5) * us] &= ~(1u << (p & 31)); }
        }
    }
    *pd = d; *pnodos += nodos; *pmax = dm; return r;
}

#ifdef __CUDACC_RTC__
/* Cada hilo tiene una "ranura": su estado se guarda entre lanzamientos (para no pasar de ~1 s por lanzamiento).
 * EST_D[h] = -1 ranura libre (toma el siguiente prefijo), -2 = ya no quedan prefijos. */
DEV void cuerpo(ARGS_DATOS, int h, u32 *usado, int us, const u16 *PREF, int d0, long long npref, unsigned long long *SIG,
        u16 *EST_C, u16 *EST_K, u32 *EST_U, int *EST_D, int H, long long presupuesto,
        unsigned long long *NODOS, unsigned long long *HECHOS, int *SOL, u16 *SOLTAB, int *MAXD)
{
    int d = EST_D[h]; if (d == -2) return;
    u16 cod[MAXN], k[MAXN];
    if (d >= 0) {
        for (int i = 0; i < d; i++) cod[i] = EST_C[(long long)i * H + h];
        for (int i = d0; i <= d && i < NN; i++) k[i] = EST_K[(long long)i * H + h];
        for (int w = 0; w < 8; w++) usado[w * us] = EST_U[w * H + h];
    }
    long long nodos = 0, resto = presupuesto; unsigned long long hechos = 0; int dmax = 0;
    while (resto > 0 && !*SOL) {
        if (d < 0) {   /* tomar el siguiente prefijo */
            unsigned long long i = atomicAdd(SIG, 1ULL);
            if ((long long)i >= npref) { d = -2; break; }
            for (int w = 0; w < 8; w++) usado[w * us] = 0;
            for (int x = 0; x < d0; x++) { int v = PREF[i * d0 + x]; cod[x] = (u16)v; usado[((v >> 2) >> 5) * us] |= 1u << ((v >> 2) & 31); }
            d = d0; if (d < NN) k[d] = 0;
        }
        long long n0 = nodos;
        int r = avanzar(PASA_DATOS, d0, NN, &d, cod, k, usado, us, resto, &nodos, &dmax);
        resto -= (nodos - n0); if (nodos == n0) resto--;
        if (r == 0) { hechos++; d = -1; }
        else if (r == 2) {   /* ¡solución! */
            if (atomicExch(SOL, 1) == 0) for (int i = 0; i < NN; i++) SOLTAB[i] = cod[i];
            break;
        }
    }
    EST_D[h] = d;
    if (d >= 0) {
        for (int i = 0; i < d; i++) EST_C[(long long)i * H + h] = cod[i];
        for (int i = d0; i <= d && i < NN; i++) EST_K[(long long)i * H + h] = k[i];
        for (int w = 0; w < 8; w++) EST_U[w * H + h] = usado[w * us];
    }
    atomicAdd(NODOS, (unsigned long long)nodos); atomicAdd(HECHOS, hechos); atomicMax(MAXD, dmax);
}
#define ARGS_RESTO const u16 *PREF, int d0, long long npref, unsigned long long *SIG, \
        u16 *EST_C, u16 *EST_K, u32 *EST_U, int *EST_D, int H, long long presupuesto, \
        unsigned long long *NODOS, unsigned long long *HECHOS, int *SOL, u16 *SOLTAB, int *MAXD
#define PASA_RESTO PREF, d0, npref, SIG, EST_C, EST_K, EST_U, EST_D, H, presupuesto, NODOS, HECHOS, SOL, SOLTAB, MAXD
extern "C" __global__ void buscar_gpu(ARGS_DATOS, ARGS_RESTO)
{
    int h = blockIdx.x * blockDim.x + threadIdx.x; if (h >= H) return;
    u32 usado[8];
    cuerpo(PASA_DATOS, h, usado, 1, PASA_RESTO);
}
/* Igual, pero los datos chicos de cada casilla y los colores van en la memoria compartida (más rápida). */
extern "C" __global__ void buscar_gpu2(ARGS_DATOS, ARGS_RESTO)
{
    extern __shared__ int sh[];
    u32 *sCOL4 = (u32 *)sh; int *sCLS = sh + NN * 4; int *sKEY = sCLS + NN; int *sCHK = sKEY + NN * 4;
    for (int i = threadIdx.x; i < NN * 4; i += blockDim.x) { sCOL4[i] = COL4[i]; sKEY[i] = KEY[i]; sCHK[i] = CHK[i]; }
    for (int i = threadIdx.x; i < NN; i += blockDim.x) sCLS[i] = CLS[i];
    __syncthreads();
    int h = blockIdx.x * blockDim.x + threadIdx.x; if (h >= H) return;
    u32 usado[8];
    cuerpo(CAND, TINI, TNUM, sCLS, sKEY, sCHK, sCOL4, NN, C, h, usado, 1, PASA_RESTO);
}
/* Como la 2, y además las piezas usadas de cada hilo van en la memoria compartida. */
extern "C" __global__ void buscar_gpu3(ARGS_DATOS, ARGS_RESTO)
{
    extern __shared__ int sh[];
    u32 *sCOL4 = (u32 *)sh; int *sCLS = sh + NN * 4; int *sKEY = sCLS + NN; int *sCHK = sKEY + NN * 4;
    u32 *sU = (u32 *)(sCHK + NN * 4);
    for (int i = threadIdx.x; i < NN * 4; i += blockDim.x) { sCOL4[i] = COL4[i]; sKEY[i] = KEY[i]; sCHK[i] = CHK[i]; }
    for (int i = threadIdx.x; i < NN; i += blockDim.x) sCLS[i] = CLS[i];
    __syncthreads();
    int h = blockIdx.x * blockDim.x + threadIdx.x; if (h >= H) return;
    cuerpo(CAND, TINI, TNUM, sCLS, sKEY, sCHK, sCOL4, NN, C, h, sU + threadIdx.x, blockDim.x, PASA_RESTO);
}
#else
/* ---------------- versión para el procesador ---------------- */
/* Genera los prefijos de profundidad dlim (hasta max). Devuelve cuántos hay (aunque no quepan). */
EXPORTA long long prefijos(ARGS_DATOS, int dlim, long long max, u16 *OUT, long long *nodos_out)
{
    u16 cod[MAXN], k[MAXN]; u32 usado[8] = {0}; int d = 0, dm = 0; long long n = 0, nodos = 0;
    k[0] = 0;
    for (;;) {
        int r = avanzar(PASA_DATOS, 0, dlim, &d, cod, k, usado, 1, (long long)4e18, &nodos, &dm);
        if (r == 0) break;
        if (n < max) memcpy(OUT + n * dlim, cod, dlim * sizeof(u16));
        n++;
        d--; { int p = cod[d] >> 2; usado[p >> 5] &= ~(1u << (p & 31)); }
    }
    *nodos_out = nodos; return n;
}
/* Agota los prefijos [i0, i1) (un hilo de Python por llamada). SOL se revisa entre prefijos y cada tanto.
 * Devuelve el número de prefijos agotados; nodos en *nodos_out; si halla solución, la copia en SOLTAB y pone *SOL=1. */
EXPORTA long long agotar_cpu(ARGS_DATOS, const u16 *PREF, int d0, long long i0, long long i1, volatile int *SOL,
                             u16 *SOLTAB, long long *nodos_out, volatile int *maxd)
{
    u16 cod[MAXN], k[MAXN]; u32 usado[8]; long long hechos = 0; int dm = *maxd;
    for (long long i = i0; i < i1 && !*SOL; i++) {
        for (int w = 0; w < 8; w++) usado[w] = 0;
        for (int x = 0; x < d0; x++) { int v = PREF[i * d0 + x]; cod[x] = (u16)v; usado[(v >> 2) >> 5] |= 1u << ((v >> 2) & 31); }
        int d = d0; if (d < NN) k[d] = 0;
        for (;;) {
            long long nd = 0;
            int r = avanzar(PASA_DATOS, d0, NN, &d, cod, k, usado, 1, 1 << 22, &nd, &dm);
            *nodos_out += nd; if (dm > *maxd) *maxd = dm;
            if (r == 0) break;
            if (r == 2) { *SOL = 1; memcpy(SOLTAB, cod, NN * sizeof(u16)); return hechos; }
            if (*SOL) return hechos;
        }
        hechos++;
    }
    return hechos;
}
/* Estimado de Knuth del tamaño del árbol (sondas al azar). */
EXPORTA double knuth(ARGS_DATOS, long long sondas, unsigned long long semilla)
{
    u16 cod[MAXN]; u32 usado[8]; double tot = 0; u64 s = semilla * 2654435761ULL + 1;
    int opc[1024];
    for (long long it = 0; it < sondas; it++) {
        for (int w = 0; w < 8; w++) usado[w] = 0;
        double w = 1, est = 1;
        for (int d = 0; d < NN; d++) {
            int pa = KEY[d * 4], pb = KEY[d * 4 + 2];
            int ca = pa < 0 ? 0 : (int)((COL4[cod[pa]] >> (8 * KEY[d * 4 + 1])) & 255);
            int cb = pb < 0 ? 0 : (int)((COL4[cod[pb]] >> (8 * KEY[d * 4 + 3])) & 255);
            int ix = (CLS[d] * C + ca) * C + cb, ini = TINI[ix], num = TNUM[ix], no = 0, req[4];
            for (int e = 0; e < 4; e++) { int x = CHK[d * 4 + e];
                req[e] = x == -1 ? -1 : (x <= -2 ? -2 - x : (int)((COL4[cod[x]] >> (8 * ((e + 2) & 3))) & 255)); }
            for (int j = 0; j < num && no < 1024; j++) {
                int v = CAND[ini + j], p = v >> 2; if (usado[p >> 5] & (1u << (p & 31))) continue;
                u32 q = COL4[v]; int ok = 1;
                for (int e = 0; e < 4; e++) if (req[e] >= 0 && (int)((q >> (8 * e)) & 255) != req[e]) ok = 0;
                if (ok) opc[no++] = v;
            }
            if (!no) break;
            w *= no; est += w;
            s ^= s >> 12; s ^= s << 25; s ^= s >> 27; u64 r = s * 2685821657736338717ULL;
            int v = opc[(int)(((r >> 32) * (u64)no) >> 32)];
            cod[d] = (u16)v; usado[(v >> 2) >> 5] |= 1u << ((v >> 2) & 31);
        }
        tot += est;
    }
    return tot / sondas;
}
#endif

