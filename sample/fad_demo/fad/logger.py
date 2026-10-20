import logging

from .config import Settings


def get_logger(name, settings: Settings = None):
    logger = logging.getLogger(name)
    logger.setLevel(settings.log_level if settings else "INFO")
    return logger
