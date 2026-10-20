from watchdog.events import FileSystemEventHandler
from watchdog.observers import Observer

from .events import FileEvent
from .logger import get_logger
from .rules import RuleEngine

log = get_logger(__name__)


class Watcher(FileSystemEventHandler):
    def __init__(self, settings):
        self.engine = RuleEngine(settings)
        self.observer = Observer()
        for folder in settings.watch_folders:
            self.observer.schedule(self, folder, recursive=True)

    def on_created(self, event):
        if not event.is_directory:
            self.engine.handle(FileEvent.created(event.src_path))

    def start(self):
        self.observer.start()

    def stop(self):
        self.observer.stop()
