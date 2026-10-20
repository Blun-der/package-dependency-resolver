import zipfile

from .compress import zip_folder


def archive_file(rule, event):
    with zipfile.ZipFile(rule.target, "a") as zf:
        zf.write(event.path)
