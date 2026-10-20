import pytest

from fad.config import Settings
from fad.rules import RuleEngine


def test_engine_loads_rules():
    s = Settings(rules=[{"pattern": "*.pdf", "action": "move", "target": "docs"}])
    assert len(RuleEngine(s).rules) == 1
