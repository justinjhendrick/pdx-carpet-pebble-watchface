#!/usr/bin/env python3
from collections import defaultdict
import os
import time
import shutil
from pathlib import Path
from typing import Any
import sys
from subprocess import check_call, CalledProcessError

THIS_DIR = Path(__file__).absolute().parent


def pebble(*args: str, **kwargs: Any) -> None:
    check_call(["pebble", *args], **kwargs)


def reset() -> None:
    pebble("kill")
    pebble("wipe")


def run(platforms: list[str]) -> dict[str, list[Path]]:
    screenshot_fnames: dict[str, list[Path]] = defaultdict(list)
    frames = THIS_DIR / "frames"
    shutil.rmtree(frames, ignore_errors=True)
    frames.mkdir()
    for platform in platforms:
        (frames / platform).mkdir()
    for hour in range(10, 13):
        for minute in range(0, 60, 3):
            env = os.environ.copy()
            env["CFLAGS"] = f"-DHOUR_OVERRIDE={hour} -DMINUTE_OVERRIDE={minute}"
            pebble("build", env=env)
            success_count = 0
            for platform in platforms:
                pebble("install", "--emulator", platform)
                screenshot_fname = frames / platform / f"screenshot_{platform}_{hour:02d}{minute:02d}.png"
                pebble("screenshot", "--emulator", platform, str(screenshot_fname))
                screenshot_fnames[platform].append(screenshot_fname)
    return screenshot_fnames


def combine(screenshot_fnames: dict[str, list[Path]]) -> None:
    # Combine all the pngs into one gif with ImageMagick
    # https://joeldare.com/combining-multiple-images-into-an-animated-gif-with-imagemagick
    for platform, fnames in screenshot_fnames.items():
        check_call(
            [
                "convert",
                "-delay",
                "60",
                "-loop",
                "0",
                "-dispose",
                "previous",
                *[str(fname) for fname in fnames],
                f"screenshots_{platform}.gif",
            ]
        )


def main() -> None:
    reset()
    combine(run(["emery", "gabbro"]))
    reset()


if __name__ == "__main__":
    main()
