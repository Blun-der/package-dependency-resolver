import os
from pathlib import Path


def safe_path(folder, name):
    """Return a path inside `folder` that does not overwrite an existing file."""
    base = Path(folder) / Path(name).name
    candidate, n = base, 1
    while candidate.exists():
        candidate = base.with_name(f"{base.stem}_{n}{base.suffix}")
        n += 1
    return str(candidate)
