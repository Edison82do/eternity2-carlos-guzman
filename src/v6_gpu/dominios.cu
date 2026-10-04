/* dominios.cu — búsqueda exacta "estilo V4.6" repartida en miles de hilos (v6, proyecto de Carlos Edison Guzman Marte).
 * Cada casilla guarda el conjunto de piezas-con-giro que todavía le caben (su DOMINIO, en bits).
 * En cada paso: se elige la casilla con MENOS opciones; al poner una pieza se quita de todas las demás casillas y
 * se recortan las vecinas (deben coincidir en el color del lado común); si alguna casilla queda sin opciones, o
 * alguna pieza que falta ya no cabe en ninguna casilla, esa rama se corta.
 * Mismo código para la tarjeta (NVRTC) y para el procesador (gcc / mingw).
 *
 * Bloque de cada nivel (en palabras de 64 bits): [DOM NN*NW][PUESTAS 4][USADAS 4][RESTO NW]
 *   PUESTAS = casillas ya decididas, USADAS = piezas ya puestas, RESTO = opciones que faltan por probar en la casilla
 *   elegida de ese nivel (SEL[nivel]). */
#ifdef __CUDACC_RTC__
  #define DEV __device__ __forceinline__
  typedef unsigned long long u64; typedef unsigned short u16; typedef unsigned int u32;
  #define POPC(x) __popcll(x)
  #define CTZ(x) (__ffsll((long long)(x)) - 1)
#else
  #define DEV static inline
  #include <string.h>
  typedef unsigned long long u64; typedef unsigned short u16; typedef unsigned int u32;
  #define POPC(x) __builtin_popcountll(x)
  #define CTZ(x) __builtin_ctzll(x)
  #ifdef _WIN32
    #define EXPORTA __declspec(dllexport)
  #else
    #define EXPORTA
  #endif
#endif

/* Datos fijos:  OC[o] colores de la opción o (N | E<<8 | S<<16 | O<<24);  OP[o] pieza de la opción;
 *  MASK[(d*C + c)*NW + w] opciones con color c en el lado d;  PMASK[p*NW + w] opciones de la pieza p;
 *  VEC[k*4 + d] casilla vecina (-1 = afuera). */
#define ARGS_D const u32 *OC, const u16 *OP, const u64 *MASK, const u64 *PMASK, const int *VEC, int NN, int NW, int NP, int C, int n, int modo, int ST
#define PASA_D OC, OP, MASK, PMASK, VEC, NN, NW, NP, C, n, modo, ST
#define TAM(NN, NW) ((NN) * (NW) + 8 + (NW))
/* Los bloques de niveles pueden estar "entrelazados" entre hilos (ST = número de hilos en la tarjeta, 1 en el
 * procesador): el elemento i de un bloque está en p[i*ST]. Así los hilos vecinos leen posiciones vecinas. */
#define X(p, i) (p)[(long long)(i) * ST]
#define AT(p, i) ((p) + (long long)(i) * ST)

DEV int puesta(ARGS_D, const u64 *PU, int k) { return (int)((X(PU, k >> 6) >> (k & 63)) & 1); }

/* colores que muestran las opciones del dominio D (en un bloque) en el lado d */
DEV u64 colores_en(ARGS_D, const u64 *D, int d)
{
    u64 m[16]; for (int w = 0; w < NW; w++) m[w] = X(D, w);
    u64 cols = 0;
    for (int c = 0; c < C; c++) { const u64 *lc = MASK + (d * C + c) * NW; for (int w = 0; w < NW; w++) if (m[w] & lc[w]) { cols |= 1ULL << c; break; } }
    return cols;
}
/* MAPAS DE FILAS Y COLUMNAS (la idea del segundo anillo de la V4.2/V4.6, aquí sobre TODAS las filas y columnas,
 * incluida la orilla, que hace de mapa de lado): pasada de ida y de vuelta; se borran las opciones que no forman
 * ninguna fila (o columna) completa compatible. Devuelve 0 si alguna casilla queda vacía.
 * Con LP (nivel anterior) es "perezoso": salta las líneas que no cambiaron, salvo por quitar la pieza pq. */
DEV int lineas(ARGS_D, u64 *L, int *cambio, const u64 *LP, int pq)
{
    const u64 *PU = AT(L, NN * NW); const u64 todos = C >= 64 ? ~0ULL : ((1ULL << C) - 1);
    u64 U[16];
    for (int ln = 0; ln < 2 * n; ln++) {
        int fila = ln < n, idx = fila ? ln : ln - n;
        int d = fila ? 1 : 2, b = (d + 2) & 3;
        int libres = 0;
        for (int j = 0; j < n; j++) { int k = fila ? idx * n + j : j * n + idx; if (!puesta(PASA_D, PU, k)) { libres = 1; break; } }
        if (!libres) continue;
        if (LP) {
            const u64 *pm = PMASK + pq * NW; int cambio_l = 0;
            for (int j = 0; j < n && !cambio_l; j++) { int k = fila ? idx * n + j : j * n + idx; const u64 *A = AT(L, k * NW), *Bp = AT(LP, k * NW);
                if (puesta(PASA_D, PU, k)) { for (int w = 0; w < NW; w++) if (X(A, w) != X(Bp, w)) { cambio_l = 1; break; } }
                else for (int w = 0; w < NW; w++) if (X(A, w) != (X(Bp, w) & ~pm[w])) { cambio_l = 1; break; } }
            if (!cambio_l) continue;
        }
        u64 F = todos;
        for (int j = 0; j < n; j++) {
            int k = fila ? idx * n + j : j * n + idx; u64 *D = AT(L, k * NW);
            if (F != todos) {
                for (int w = 0; w < NW; w++) U[w] = 0;
                u64 x = F; while (x) { int c = CTZ(x); x &= x - 1; const u64 *lc = MASK + (b * C + c) * NW; for (int w = 0; w < NW; w++) U[w] |= lc[w]; }
                u64 any = 0;
                for (int w = 0; w < NW; w++) { u64 v = X(D, w), nv = v & U[w]; if (nv != v) { X(D, w) = nv; *cambio = 1; } any |= nv; }
                if (!any) return 0;
            }
            F = colores_en(PASA_D, D, d);
        }
        u64 Bc = todos;
        for (int j = n - 1; j >= 0; j--) {
            int k = fila ? idx * n + j : j * n + idx; u64 *D = AT(L, k * NW);
            if (Bc != todos) {
                for (int w = 0; w < NW; w++) U[w] = 0;
                u64 x = Bc; while (x) { int c = CTZ(x); x &= x - 1; const u64 *lc = MASK + (d * C + c) * NW; for (int w = 0; w < NW; w++) U[w] |= lc[w]; }
                u64 any = 0;
                for (int w = 0; w < NW; w++) { u64 v = X(D, w), nv = v & U[w]; if (nv != v) { X(D, w) = nv; *cambio = 1; } any |= nv; }
                if (!any) return 0;
            }
            Bc = colores_en(PASA_D, D, b);
        }
    }
    return 1;
}

/* Construye el nivel siguiente: casilla k := opción o. Devuelve 0 si la rama muere. */
DEV int aplicar(ARGS_D, const u64 *L, u64 *N, int k, int o)
{
    int S = NN * NW + 8;
    for (int i = 0; i < S; i++) X(N, i) = X(L, i);
    u64 *PU = AT(N, NN * NW), *US = AT(N, NN * NW + 4);
    X(PU, k >> 6) |= 1ULL << (k & 63);
    int p = OP[o]; X(US, p >> 6) |= 1ULL << (p & 63);
    u64 *Dk = AT(N, k * NW);
    for (int w = 0; w < NW; w++) X(Dk, w) = 0;
    X(Dk, o >> 6) = 1ULL << (o & 63);
    const u64 *pm = PMASK + p * NW;
    for (int kk = 0; kk < NN; kk++) {
        if (puesta(PASA_D, PU, kk)) continue;
        u64 *D = AT(N, kk * NW), any = 0;
        for (int w = 0; w < NW; w++) { u64 v = X(D, w) & ~pm[w]; X(D, w) = v; any |= v; }
        if (!any) return 0;
    }
    u32 q = OC[o];
    for (int d = 0; d < 4; d++) {
        int k2 = VEC[k * 4 + d];
        if (k2 < 0 || puesta(PASA_D, PU, k2)) continue;
        int col = (q >> (8 * d)) & 255;
        const u64 *m = MASK + (((d + 2) & 3) * C + col) * NW;
        u64 *D = AT(N, k2 * NW), any = 0;
        for (int w = 0; w < NW; w++) { u64 v = X(D, w) & m[w]; X(D, w) = v; any |= v; }
        if (!any) return 0;
    }
    if (modo & 1) {
        int vueltas = (modo & 2) ? 8 : 1;
        for (int it = 0; it < vueltas; it++) { int cb = 0; if (!lineas(PASA_D, N, &cb, (it == 0 && (modo & 8)) ? L : 0, p)) return 0; if (!cb) break; }
    }
    return 1;
}

/* Elige la casilla con menos opciones y revisa que cada pieza que falta quepa en algún lado.
 * Devuelve -1 = rama muerta, 0 = tablero completo, 1 = elegida (*kout) y el RESTO del nivel queda listo. */
DEV int elegir(ARGS_D, u64 *L, int *kout)
{
    const u64 *PU = AT(L, NN * NW), *US = AT(L, NN * NW + 4); u64 *RE = AT(L, NN * NW + 8);
    u64 U[16]; for (int w = 0; w < NW; w++) U[w] = 0;
    int mejor = -1, mv = 1 << 30;
    for (int k = 0; k < NN; k++) {
        if (puesta(PASA_D, PU, k)) continue;
        const u64 *D = AT(L, k * NW); int nb = 0;
        for (int w = 0; w < NW; w++) { u64 v = X(D, w); nb += POPC(v); U[w] |= v; }
        if (nb == 0) return -1;
        int vec = 0;
        for (int d = 0; d < 4; d++) { int k2 = VEC[k * 4 + d]; if (k2 < 0 || puesta(PASA_D, PU, k2)) vec++; }
        int v = nb * 8 - vec;
        if ((modo & 4) && nb > 1) { int r = k / n, c = k % n; if (r == 0 || c == 0 || r == n - 1 || c == n - 1) v += 1 << 20; }   /* interior primero */
        if (v < mv) { mv = v; mejor = k; }
    }
    if (mejor < 0) return 0;
    for (int p = 0; p < NP; p++) {
        if ((X(US, p >> 6) >> (p & 63)) & 1) continue;
        const u64 *pm = PMASK + p * NW; u64 any = 0;
        for (int w = 0; w < NW; w++) any |= U[w] & pm[w];
        if (!any) return -1;
    }
    const u64 *D = AT(L, mejor * NW);
    for (int w = 0; w < NW; w++) X(RE, w) = X(D, w);
    *kout = mejor; return 1;
}

/* Avanza desde el nivel *plv (B = bloques de niveles, SEL = casilla elegida por nivel). lv0 = fondo.
 * Devuelve 0 = agotado, 1 = presupuesto, 2 = solución (en el nivel *plv). */
DEV int avanzar(ARGS_D, u64 *B, int *SEL, int lv0, int *plv, long long presupuesto, long long *pnodos, int *pmax)
{
    int lv = *plv, T = TAM(NN, NW), r = 1, dm = *pmax; long long nodos = 0;
    while (nodos < presupuesto) {
        u64 *L = AT(B, (long long)lv * T), *RE = AT(L, NN * NW + 8);
        int w = 0; while (w < NW && !X(RE, w)) w++;
        if (w == NW) { if (lv <= lv0) { r = 0; break; } lv--; continue; }
        u64 v = X(RE, w); int o = w * 64 + CTZ(v); X(RE, w) = v & (v - 1);
        nodos++;
        u64 *N = AT(L, T);
        if (!aplicar(PASA_D, L, N, X(SEL, lv), o)) continue;
        int k2, e = elegir(PASA_D, N, &k2);
        if (e < 0) continue;
        lv++; if (lv > dm) dm = lv;
        if (e == 0) { r = 2; break; }
        X(SEL, lv) = k2;
    }
    *plv = lv; *pnodos += nodos; *pmax = dm; return r;
}

/* Arranca un prefijo: copia la raíz al nivel 0 y aplica las elecciones (casilla, opción) del prefijo.
 * Devuelve el nivel alcanzado, -1 si el prefijo no es válido, o -2 si ya es solución (queda en el nivel 0). */
DEV int arrancar(ARGS_D, const u64 *RAIZ, int SEL0, u64 *B, int *SEL, const u16 *pref, int d0)
{
    int T = TAM(NN, NW);
    for (int i = 0; i < T; i++) X(B, i) = RAIZ[i];
    X(SEL, 0) = SEL0;
    for (int i = 0; i < d0; i++) {
        int k = pref[2 * i], o = pref[2 * i + 1];
        u64 *L = AT(B, (long long)i * T), *N = AT(L, T);
        if (!aplicar(PASA_D, L, N, k, o)) return -1;
        int k2, e = elegir(PASA_D, N, &k2);
        if (e < 0) return -1;
        if (e == 0) { for (int x = 0; x < T; x++) X(B, x) = X(N, x); return -2; }
        X(SEL, i + 1) = k2;
    }
    return d0;
}

#ifdef __CUDACC_RTC__
/* Cada hilo tiene su bloque de niveles en la memoria global (B + h*NIV*T). EST_D[h]: nivel actual, -1 libre, -2 terminado. */
extern "C" __global__ void buscar_dom(ARGS_D, const u64 *RAIZ, int SEL0, const u16 *PREF, int d0, long long npref,
        unsigned long long *SIG, u64 *BLOQ, int *SELS, int *EST_D, int H, int NIV, long long presupuesto,
        unsigned long long *NODOS, unsigned long long *HECHOS, int *SOL, u64 *SOLB, int *MAXD)
{
    int h = blockIdx.x * blockDim.x + threadIdx.x; if (h >= H) return;
    int lv = EST_D[h]; if (lv == -2) return;
    int T = TAM(NN, NW);
    u64 *B = BLOQ + h; int *SEL = SELS + h;      /* entrelazado: ST = H */
    long long nodos = 0, resto = presupuesto; unsigned long long hechos = 0; int dmax = 0;
    while (resto > 0 && !*(volatile int *)SOL) {
        if (lv < 0) {
            unsigned long long i = atomicAdd(SIG, 1ULL);
            if ((long long)i >= npref) { lv = -2; break; }
            lv = arrancar(PASA_D, RAIZ, SEL0, B, SEL, PREF + i * 2 * d0, d0);
            resto -= d0 + 1;
            if (lv == -1) { hechos++; continue; }
            if (lv == -2) { if (atomicExch(SOL, 1) == 0) for (int x = 0; x < NN * NW; x++) SOLB[x] = X(B, x); lv = -2; break; }
        }
        long long n0 = nodos;
        int r = avanzar(PASA_D, B, SEL, d0, &lv, resto, &nodos, &dmax);
        resto -= (nodos - n0); if (nodos == n0) resto--;
        if (r == 0) { hechos++; lv = -1; }
        else if (r == 2) {
            if (atomicExch(SOL, 1) == 0) for (int x = 0; x < NN * NW; x++) SOLB[x] = X(B, (long long)lv * T + x);
            break;
        }
    }
    EST_D[h] = lv;
    atomicAdd(NODOS, (unsigned long long)nodos); atomicAdd(HECHOS, hechos); atomicMax(MAXD, dmax);
}
#else
/* Genera los prefijos de profundidad dlim: pares (casilla, opción). Devuelve cuántos hay; -2 si se halló solución antes. */
EXPORTA long long prefijos_dom(ARGS_D, const u64 *RAIZ, int SEL0, int dlim, long long max, u16 *OUT, u64 *B, int *SEL, u64 *SOLB)
{
    int T = TAM(NN, NW); long long np_ = 0, nodos = 0; int lv = 0, dm = 0;
    for (int i = 0; i < T; i++) B[i] = RAIZ[i];
    SEL[0] = SEL0;
    /* DFS hasta dlim: reutiliza avanzar con presupuesto 1 paso para detectar llegadas a dlim */
    for (;;) {
        u64 *L = B + (long long)lv * T, *RE = L + NN * NW + 8;
        if (lv == dlim) {
            if (np_ < max) for (int i = 0; i < dlim; i++) {
                /* la opción elegida en el nivel i es la única del dominio de SEL[i] en el nivel i+1 */
                const u64 *D = B + (long long)(i + 1) * T + SEL[i] * NW; int o = 0;
                for (int w = 0; w < NW; w++) if (D[w]) { o = w * 64 + CTZ(D[w]); break; }
                OUT[(np_ * dlim + i) * 2] = (u16)SEL[i]; OUT[(np_ * dlim + i) * 2 + 1] = (u16)o;
            }
            np_++; lv--; continue;
        }
        int w = 0; while (w < NW && !RE[w]) w++;
        if (w == NW) { if (lv == 0) break; lv--; continue; }
        int o = w * 64 + CTZ(RE[w]); RE[w] &= RE[w] - 1;
        nodos++;
        u64 *N = L + T;
        if (!aplicar(PASA_D, L, N, SEL[lv], o)) continue;
        int k2, e = elegir(PASA_D, N, &k2);
        if (e < 0) continue;
        lv++;
        if (e == 0) { for (int x = 0; x < NN * NW; x++) SOLB[x] = N[x]; return -2; }
        SEL[lv] = k2;
    }
    (void)dm; (void)nodos;
    return np_;
}
EXPORTA int lineas_cpu(ARGS_D, u64 *L) { for (int it = 0; it < 50; it++) { int cb = 0; if (!lineas(PASA_D, L, &cb, 0, 0)) return 0; if (!cb) break; } return 1; }
EXPORTA int elegir_cpu(ARGS_D, u64 *L, int *kout) { return elegir(PASA_D, L, kout); }
/* Agota los prefijos [i0, i1) en un hilo del procesador. */
EXPORTA long long agotar_dom(ARGS_D, const u64 *RAIZ, int SEL0, const u16 *PREF, int d0, long long i0, long long i1,
                             u64 *B, int *SEL, volatile int *SOL, u64 *SOLB, long long *nodos_out, int *maxd)
{
    long long hechos = 0; int T = TAM(NN, NW);
    for (long long i = i0; i < i1 && !*SOL; i++) {
        int lv = arrancar(PASA_D, RAIZ, SEL0, B, SEL, PREF + i * 2 * d0, d0);
        if (lv == -1) { hechos++; continue; }
        if (lv == -2) { *SOL = 1; memcpy(SOLB, B, NN * NW * sizeof(u64)); return hechos; }
        for (;;) {
            long long nd = 0;
            int r = avanzar(PASA_D, B, SEL, d0, &lv, 1 << 16, &nd, maxd);
            *nodos_out += nd;
            if (r == 0) break;
            if (r == 2) { *SOL = 1; memcpy(SOLB, B + (long long)lv * T, NN * NW * sizeof(u64)); return hechos; }
            if (*SOL) return hechos;
        }
        hechos++;
    }
    return hechos;
}
#endif
