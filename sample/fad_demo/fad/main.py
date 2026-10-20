import argparse

from fad.config import load_config
from fad.daemon import Daemon


def main():
    parser = argparse.ArgumentParser(description="File Automation Daemon")
    parser.add_argument("--config", default="fad.yaml")
    args = parser.parse_args()
    Daemon(load_config(args.config)).run_forever()


if __name__ == "__main__":
    main()
