from PIL import Image, ImageDraw, ImageFont
import time
import logging
import threading
import queue
import os

logger = logging.getLogger("loki.display")

# Guard and single shared instance for the framebuffer/display
_fb_opened = False
_display_instance = None
_display_lock = threading.Lock()


class LokiDisplay:
    """
    LokiDisplay opens the framebuffer once and exposes simple drawing helpers.
    """

    def __init__(self, config):
        global _fb_opened, _display_instance
        # If another instance was created while this constructor was queued,
        # reuse it to avoid opening the device twice.
        if _fb_opened and _display_instance is not None:
            logger.debug("LokiDisplay.__init__: reusing existing display instance")
            existing = _display_instance
            self.width = existing.width
            self.height = existing.height
            self.animation = existing.animation
            self.fb_dev = existing.fb_dev
            self.fb = existing.fb
            return

        # Normal initialization (first instance)
        try:
            disp_cfg = config.display() if hasattr(config, "display") else {}
        except Exception:
            disp_cfg = {}
        self.width = disp_cfg.get("width", 480)
        self.height = disp_cfg.get("height", 320)
        self.animation = disp_cfg.get("animation", "boot_sequence")
        self.fb_dev = disp_cfg.get("device", "/dev/fb1") if isinstance(disp_cfg, dict) else "/dev/fb1"

        try:
            self.fb = open(self.fb_dev, "wb", buffering=0)
            logger.info("Opened framebuffer device %s", self.fb_dev)
        except Exception as e:
            logger.error("Unexpected error opening framebuffer %s: %s", self.fb_dev, e)
            self.fb = None

        _display_instance = self
        _fb_opened = True
        # Rendering state and font/queue
        self._fps = 20
        self._frame_lock = threading.Lock()
        self._current_frame = None
        self._stop_event = threading.Event()

        # Font: try a common system TTF, fallback to default
        try:
            ttf_path = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"
            if os.path.exists(ttf_path):
                self._font = ImageFont.truetype(ttf_path, 14)
            else:
                self._font = getattr(self, "_font", ImageFont.load_default())
        except Exception:
            self._font = getattr(self, "_font", ImageFont.load_default())

        # Nonblocking status queue for plugins
        self._status_queue = queue.Queue()
        self._status_worker = threading.Thread(target=self._status_worker_loop, name="LokiStatusWorker", daemon=True)
        self._status_worker.start()

        # Render thread placeholder (starts a render loop if implemented)
        self._render_thread = threading.Thread(target=self._render_loop, name="LokiDisplayRender", daemon=True)
        self._render_thread.start()

    def request_frame(self, img):
        """Queue a PIL Image to be shown on the next render tick. Nonblocking."""
        try:
            with self._frame_lock:
                # store a copy so callers can reuse their Image
                self._current_frame = img.copy()
        except Exception:
            logger.exception("Failed to request frame")

    def draw_frame(self, img):
        """Backwards-compatible immediate write. Prefer request_frame for smoothness."""
        self.request_frame(img)
    def draw_frame(self, img):
        if not self.fb:
            return
        try:
            self.fb.write(img.tobytes())
            self.fb.flush()
        except Exception:
            logger.exception("Error writing frame to framebuffer")

    def boot_animation(self, duration_seconds=3.0):
        for i in range(60):
            img = Image.new("RGB", (self.width, self.height), (i * 4 % 255, 0, 40))
            draw = ImageDraw.Draw(img)
            draw.text((10, 10), "Loki booting...", fill=(255, 255, 255))
            draw.text((10, 40), f"Step {i}", fill=(200, 200, 200))
            self.request_frame(img)
            time.sleep(0.05)

    def show_plugin_status(self, active_plugins, enabled_plugins, scroll=False, speed=1.0):
        """
        Render plugin list. If scroll is True and list exceeds height, scroll vertically.
        """
        font = getattr(self, "_font", ImageFont.load_default())
        line_h = font.getsize("Ay")[1] + 2
        visible_lines = max(1, self.height // line_h)
        names = list(enabled_plugins)

        if len(names) <= visible_lines or not scroll:
            img = Image.new("RGB", (self.width, self.height), (0, 0, 0))
            draw = ImageDraw.Draw(img)
            draw.text((10, 10), "Loki Plugins", fill=(0, 255, 0), font=font)
            y = 30
            for name in names:
                color = (0, 255, 0) if name in active_plugins else (255, 255, 255)
                draw.text((10, y), f"- {name}", fill=color, font=font)
                y += line_h
            self.request_frame(img)
            return

        # Scrolling mode: create tall image and scroll window
        tall_h = line_h * (len(names) + 3)
        tall = Image.new("RGB", (self.width, tall_h), (0, 0, 0))
        draw = ImageDraw.Draw(tall)
        draw.text((10, 2), "Loki Plugins", fill=(0, 255, 0), font=font)
        y = 20
        for name in names:
            color = (0, 255, 0) if name in active_plugins else (255, 255, 255)
            draw.text((10, y), f"- {name}", fill=color, font=font)
            y += line_h

        offset = 0
        step = max(1, int(speed))
        while not self._stop_event.is_set():
            if offset + self.height > tall_h:
                offset = 0
            window = tall.crop((0, offset, self.width, offset + self.height))
            self.request_frame(window)
            time.sleep(1.0 / max(1, int(self._fps)))
            offset += step

        img = Image.new("RGB", (self.width, self.height), (0, 0, 0))
        draw = ImageDraw.Draw(img)
        draw.text((10, 10), "Loki Plugins", fill=(0, 255, 0))
        y = 40
        for name in enabled_plugins:
            color = (255, 255, 255)
            if name in active_plugins:
                color = (0, 255, 0)
            draw.text((10, y), f"- {name}", fill=color)
            y += 20
        self.request_frame(img)

    def close(self):
        try:
            if self.fb:
                self.fb.close()
        except Exception:
            logger.exception("Error closing framebuffer")


    def enqueue_status(self, active_plugins, enabled_plugins, scroll=True, speed=1.0):
        """Nonblocking API for plugins: enqueue a status update."""
        try:
            self._status_queue.put_nowait((set(active_plugins), list(enabled_plugins), bool(scroll), float(speed)))
        except Exception:
            logger.exception("Failed to enqueue status update")

    def _status_worker_loop(self):
        """Worker thread that consumes status updates and runs show_plugin_status.
        If multiple updates arrive quickly, only the latest is processed.
        """
        while not getattr(self, "_stop_event", threading.Event()).is_set():
            try:
                item = self._status_queue.get(timeout=0.5)
            except Exception:
                continue
            # drain queue to keep only the latest update
            while True:
                try:
                    nxt = self._status_queue.get_nowait()
                    item = nxt
                except Exception:
                    break
            try:
                active, enabled, scroll, speed = item
                self.show_plugin_status(active, enabled, scroll=scroll, speed=speed)
            except Exception:
                logger.exception("Error processing status update")
 
    def _render_loop(self):
        """Minimal render loop: writes the latest requested frame to the framebuffer at the configured FPS."""
        period = 1.0 / max(1, int(getattr(self, "_fps", 20)))
        while not getattr(self, "_stop_event", threading.Event()).is_set():
            start = time.time()
            frame = None
            try:
                with self._frame_lock:
                    if self._current_frame is not None:
                        frame = self._current_frame.copy()
                        self._current_frame = None
                if frame is not None and getattr(self, "fb", None):
                    try:
                        self.fb.write(frame.tobytes())
                        try:
                            self.fb.flush()
                        except Exception:
                            pass
                    except Exception:
                        logger.exception("Error writing frame to framebuffer")
            except Exception:
                logger.exception("Unexpected error in render loop")
            elapsed = time.time() - start
            to_sleep = period - elapsed
            if to_sleep > 0:
                time.sleep(to_sleep)
    def stop(self):
        """Signal worker and render threads to stop and join them."""
        try:
            self._stop_event.set()
            if hasattr(self, "_render_thread") and self._render_thread.is_alive():
                self._render_thread.join(timeout=1.0)
            if hasattr(self, "_status_worker") and self._status_worker.is_alive():
                self._status_worker.join(timeout=1.0)
        except Exception:
            logger.exception("Error stopping display threads")

def init_display(config):
    """
    Return a single shared display instance. Thread-safe: prevents concurrent creation.
    """
    global _fb_opened, _display_instance

    # Fast path
    if _fb_opened and _display_instance is not None:
        logger.debug("Display already initialized; returning existing instance")
        return _display_instance

    # Slow path: ensure only one thread creates the instance
    with _display_lock:
        if _fb_opened and _display_instance is not None:
            logger.debug("Display initialized while waiting for lock; returning existing instance")
            return _display_instance

        disp = LokiDisplay(config)
        _display_instance = disp
        _fb_opened = True
        return _display_instance
