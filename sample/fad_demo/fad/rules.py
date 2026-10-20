import fnmatch
from dataclasses import dataclass

from .actions import run_action
from .config import Settings
from .logger import get_logger

log = get_logger(__name__)


@dataclass
class Rule:
    pattern: str
    action: str
    target: str = ""


class RuleEngine:
    def __init__(self, settings: Settings):
        self.rules = [Rule(**r) for r in settings.rules]

    def handle(self, event):
        for rule in self.rules:
            if fnmatch.fnmatch(event.path, rule.pattern):
                log.info("rule %s matched %s", rule.pattern, event.path)
                run_action(rule, event)
