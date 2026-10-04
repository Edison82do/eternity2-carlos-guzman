"""gpu_instalar_cupy.py — Instala CuPy para CUDA 12 (con sus librerías de CUDA desde pip) y lo prueba."""
import subprocess, sys
r = subprocess.run([sys.executable, '-m', 'pip', 'install', '--user', 'cupy-cuda12x', 'nvidia-cuda-nvrtc-cu12==12.6.*', 'nvidia-cuda-runtime-cu12==12.6.*'], capture_output=True, text=True)
print(r.stdout[-3000:]); print(r.stderr[-3000:])
try:
    import cupy as cp
    print('cupy', cp.__version__, '| tarjeta:', cp.cuda.runtime.getDeviceProperties(0)['name'].decode())
    x = cp.arange(10, dtype=cp.float32); print('suma en la tarjeta:', float((x * 2).sum()))
    k = cp.RawKernel(r'extern "C" __global__ void doble(float* a){int i=threadIdx.x; a[i]*=2.0f;}', 'doble')
    k((1,), (10,), (x,)); print('kernel propio (compilado al momento):', x.get().tolist())
except Exception as e:
    print('CuPy falló:', type(e).__name__, e)
