from .archive import archive_file
from .move import move_file
from .notify import send_notification
from .rename import rename_file

ACTIONS = {
    "move": move_file,
    "rename": rename_file,
    "notify": send_notification,
    "archive": archive_file,
}


def run_action(rule, event):
    ACTIONS[rule.action](rule, event)
