import shutil

from ..logger import get_logger
from ..rules import Rule
from ..utils.paths import safe_path

log = get_logger(__name__)


def move_file(rule: Rule, event):
    dest = safe_path(rule.target, event.path)
    shutil.move(event.path, dest)
    log.info("moved %s -> %s", event.path, dest)
