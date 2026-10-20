import requests

from ..config import Settings


def send_notification(rule, event):
    requests.post(rule.target, json={"file": event.path, "kind": event.kind}, timeout=5)
