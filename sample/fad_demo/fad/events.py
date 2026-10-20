from dataclasses import dataclass
from datetime import datetime

from .config import Settings


@dataclass
class FileEvent:
    path: str
    kind: str
    at: datetime

    @classmethod
    def created(cls, path):
        return cls(path=path, kind="created", at=datetime.now())
