"""
Control de ejecución: pausa, paso a paso, cámara lenta, detener y límite de tiempo.
Sin control (ctrl=None) el solucionador corre a máxima velocidad sin ningún coste extra.
"""
import threading
import time


class Detenido(Exception):
    """Se lanza para cortar la búsqueda (botón Detener o límite de tiempo)."""


class Control:
    def __init__(self, retardo=0.0, limite_segundos=None):
        self.retardo = retardo                 # segundos entre pasos (cámara lenta)
        self._corriendo = threading.Event()
        self._corriendo.set()                  # set = no pausado
        self._paso = threading.Event()
        self.detener = False
        self.limite = limite_segundos
        self.t0 = time.perf_counter()
        self.lock = threading.Lock()
        self.estado = {}                       # lo que la interfaz dibuja
        self.version = 0
        self._ultimo_copiado = 0.0

    # --- órdenes desde la interfaz
    def pausar(self):
        self._corriendo.clear()

    def continuar(self):
        self._corriendo.set()

    @property
    def pausado(self):
        return not self._corriendo.is_set()

    def un_paso(self):
        self._paso.set()

    # --- llamado por el solucionador
    def tick(self, **estado):
        if self.detener:
            raise Detenido("detenido por el usuario")
        if self.limite is not None and time.perf_counter() - self.t0 > self.limite:
            raise Detenido("límite de tiempo")
        ahora = time.perf_counter()
        # Copiar estado para dibujar: siempre en cámara lenta/pausa, y a 20 fps si va rápido.
        if self.retardo > 0 or self.pausado or ahora - self._ultimo_copiado > 0.05:
            with self.lock:
                self.estado = {k: (list(v) if isinstance(v, list) else v) for k, v in estado.items()}
                self.version += 1
            self._ultimo_copiado = ahora
        while not self._corriendo.is_set():
            if self._paso.wait(0.05):
                self._paso.clear()
                break
            if self.detener:
                raise Detenido("detenido por el usuario")
        if self.retardo > 0:
            time.sleep(self.retardo)
