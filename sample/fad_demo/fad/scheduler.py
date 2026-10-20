import sched
import time

from .config import Settings
from .rules import RuleEngine


class Scheduler:
    """Runs periodic clean-up rules, for example 'archive files older than 30 days'."""

    def __init__(self, settings: Settings):
        self.engine = RuleEngine(settings)
        self.queue = sched.scheduler(time.time, time.sleep)

    def tick(self):
        self.queue.run(blocking=False)
