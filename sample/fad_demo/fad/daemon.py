import signal
import time

from .logger import get_logger
from .scheduler import Scheduler
from .watcher import Watcher

log = get_logger(__name__)


class Daemon:
    def __init__(self, settings):
        self.settings = settings
        self.watcher = Watcher(settings)
        self.scheduler = Scheduler(settings)
        self.running = True
        signal.signal(signal.SIGTERM, self.stop)

    def stop(self, *_):
        log.info("stopping daemon")
        self.running = False

    def run_forever(self):
        self.watcher.start()
        while self.running:
            self.scheduler.tick()
            time.sleep(1)
        self.watcher.stop()
