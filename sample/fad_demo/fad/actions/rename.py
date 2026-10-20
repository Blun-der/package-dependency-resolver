import os

from ..utils.paths import safe_path


def rename_file(rule, event):
    folder = os.path.dirname(event.path)
    os.rename(event.path, safe_path(folder, rule.target))
