"""gpu_info.py — Diagnóstico (solo mira, no instala nada): Python, tarjeta de video, CUDA y librerías."""
import sys, platform, subprocess, importlib, shutil
print('Python', sys.version.replace('\n', ' '))
print('Ejecutable:', sys.executable)
print('Sistema:', platform.platform())
smi = shutil.which('nvidia-smi')
print('nvidia-smi:', smi)
if smi:
    try:
        r = subprocess.run([smi], capture_output=True, text=True, timeout=30); print(r.stdout)
        r = subprocess.run([smi, '--query-gpu=name,driver_version,memory.total,memory.used,compute_cap', '--format=csv'], capture_output=True, text=True, timeout=30); print(r.stdout, r.stderr)
    except Exception as e: print('error nvidia-smi:', e)
for m in ('numpy', 'cupy', 'torch', 'numba'):
    try:
        x = importlib.import_module(m); print(f'{m}: instalado, versión {getattr(x, "__version__", "?")}')
    except Exception as e: print(f'{m}: no instalado ({type(e).__name__})')
r = subprocess.run([sys.executable, '-m', 'pip', '--version'], capture_output=True, text=True); print('pip:', r.stdout.strip(), r.stderr.strip())
