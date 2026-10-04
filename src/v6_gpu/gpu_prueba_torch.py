"""gpu_prueba_torch.py — ¿PyTorch puede usar la GTX 1070? (solo prueba, no instala)."""
import time, torch
print('torch', torch.__version__, '| CUDA de torch:', torch.version.cuda)
print('cuda disponible:', torch.cuda.is_available())
if torch.cuda.is_available():
    print('tarjeta:', torch.cuda.get_device_name(0), '| capacidad:', torch.cuda.get_device_capability(0))
    print('arquitecturas compiladas en esta versión:', torch.cuda.get_arch_list())
    try:
        a = torch.rand(4096, 4096, device='cuda'); torch.cuda.synchronize(); t = time.time()
        for _ in range(10): b = a @ a
        torch.cuda.synchronize(); dt = time.time() - t
        print(f'multiplicación de matrices: {10 * 2 * 4096**3 / dt / 1e12:.2f} TFLOPS (float32)')
    except Exception as e: print('FALLÓ el cálculo en la tarjeta:', e)
