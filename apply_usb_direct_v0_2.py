#!/usr/bin/env python3
"""Safely apply USB Direct v0.2 to a COPY of the known working v0.1 tree.

Does not edit/replace a built v0.1 ELF or install anything on a PS4.
Distribution: GPL-3.0-or-later, consistent with Control4Free.
"""
from hashlib import sha256
from pathlib import Path
import shutil
import sys

EXPECTED_V01_USB_SHA256 = "04928ed42e6bbd62a23ae64dbafc9c4e253b381de7198456fefc10dcd76c813e"
BASE_MARKER = "USB direct: Xbox 360 claimed; worker started"


def replace_exact(contents: str, old: str, new: str, label: str) -> str:
    occurrences = contents.count(old)
    if occurrences != 1:
        raise ValueError(f"{label}: expected 1 occurrence, found {occurrences}: {old!r}")
    return contents.replace(old, new, 1)


def main():
    if len(sys.argv) != 2:
        raise ValueError("Usage: python3 apply_usb_direct_v0_2.py /path/to/COPY-of-v0.1-project")
    root = Path(sys.argv[1]).expanduser().resolve()
    base = Path(__file__).resolve().parent / "patch_files"
    c = root / "src/xbox_direct.c"
    h = root / "include/c4f_xbox_direct.h"
    web = root / "src/web.c"
    make = root / "Makefile"
    version = root / "VERSION"
    for path in (c, h, web, make, version):
        if not path.is_file():
            raise ValueError(f"Not a USB Direct v0.1 project: missing {path}")
    if version.read_text().strip() != "1.2.0":
        raise ValueError("Only Control4Free v1.2.0 is supported")
    old_src = c.read_bytes()
    if sha256(old_src).hexdigest() != EXPECTED_V01_USB_SHA256:
        raise ValueError("src/xbox_direct.c does not match working v0.1; refusing overwrite")
    ws = web.read_text()
    ms = make.read_text()
    if "C4F_USB_DIRECT" not in ws or BASE_MARKER not in old_src.decode():
        raise ValueError("USB Direct v0.1 base not found")
    ws2 = replace_exact(
        ws,
        "if (!app->usbActive || app->usbPadReady || app->creationBlocked ||",
        "if (!app->usbActive || !c4fXboxDirectConnected() ||\n        app->usbPadReady || app->creationBlocked ||",
        "web.c",
    )
    ms2 = replace_exact(ms, "NAME    := control4free-usb", "NAME    := control4free-usb-v0_2", "Makefile")
    ms2 = replace_exact(ms2, "VERSION := $(shell cat VERSION)",
                        "VERSION := $(shell cat VERSION)-usb-direct-v0.2", "Makefile")
    if "src/xbox_direct.c" not in ms2 or "-DC4F_USB_DIRECT" not in ms2:
        raise ValueError("Makefile is not the USB Direct v0.1 build")
    replacement_h = (base / "include/c4f_xbox_direct.h").read_bytes()
    replacement_c = (base / "src/xbox_direct.c").read_bytes()
    # Make all validations before any file is mutated.
    if b"c4fXboxDirectConnected" not in replacement_h or b"c4fXboxDirectConnected" not in replacement_c:
        raise ValueError("Missing connection-state API in patch")
    changes = ((c, replacement_c), (h, replacement_h),
               (web, ws2.encode()), (make, ms2.encode()))
    for path, content in changes:
        backup = path.with_suffix(path.suffix + ".pre_usb_direct_v0_2")
        if backup.exists():
            raise ValueError(f"Backup already exists; refusing second patch: {backup}")
    for path, content in changes:
        backup = path.with_suffix(path.suffix + ".pre_usb_direct_v0_2")
        shutil.copy2(path, backup)
        path.write_bytes(content)
    print("USB Direct v0.2 applied to COPY of v0.1 (Control4Free 1.2.0).")
    print("New build target: build/control4free-usb-v0_2.elf")
    print("Only changed USB recovery, USB-connected guard, and build name/version.")
    print("Original working v0.1 not touched by this script unless you explicitly targeted it.")
    print("WARNING: experimental, hot-unplug behavior not verified on PS4 hardware.")


if __name__ == "__main__":
    try:
        main()
    except (ValueError, OSError) as exc:
        sys.exit(f"ERROR: {exc}")
