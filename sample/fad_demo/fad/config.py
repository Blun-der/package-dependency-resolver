import os
from dataclasses import dataclass, field

import yaml


@dataclass
class Settings:
    watch_folders: list = field(default_factory=list)
    rules: list = field(default_factory=list)
    log_level: str = "INFO"


def load_config(path):
    with open(os.path.expanduser(path)) as fh:
        return Settings(**yaml.safe_load(fh))
