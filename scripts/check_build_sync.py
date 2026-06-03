#!/usr/bin/env python3
"""Guard against app-path vs Zephyr-module-path build/config drift.

The SDK is consumed two ways that must stay in lockstep:

  * App / sample build : app/CMakeLists.txt  +  app/Kconfig
  * Zephyr-module build: CMakeLists.txt (root)  +  Kconfig.ibattery -> app/Kconfig.battery

Historically these drifted silently (root CMakeLists.txt once omitted
battery_soh.c, and the module Kconfig lagged 17 options) because nothing
exercised the module path. This check enforces two invariants so that class
of drift fails CI instead of shipping broken to downstream consumers:

  1. The C source set compiled by the root CMakeLists.txt equals the set
     compiled by app/CMakeLists.txt.
  2. All CONFIG_BATTERY_* options are defined in exactly one place
     (app/Kconfig.battery), sourced by BOTH app/Kconfig and Kconfig.ibattery.

Pure stdlib; no toolchain needed. Exit 0 on success, 1 on drift.
"""

import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent


def check_cmake_sync(errors: list[str]) -> None:
    root_cmake = (ROOT / "CMakeLists.txt").read_text()
    app_cmake = (ROOT / "app" / "CMakeLists.txt").read_text()

    # Root (library/module) build references SDK sources as "src/...".
    root_src = set(re.findall(r"\bsrc/[A-Za-z0-9_/]+\.c", root_cmake))
    # App build pulls the SAME SDK sources via "../src/..."; its own entry
    # point app/src/main.c (referenced as "src/main.c", no "../") is app-only
    # and intentionally excluded from the shared-library comparison.
    app_src = {m[len("../"):] for m in re.findall(r"\.\./src/[A-Za-z0-9_/]+\.c", app_cmake)}

    only_app = app_src - root_src
    only_root = root_src - app_src
    if only_app:
        errors.append(
            "Sources in app/CMakeLists.txt but MISSING from root CMakeLists.txt "
            f"(module consumers won't compile these): {sorted(only_app)}"
        )
    if only_root:
        errors.append(
            "Sources in root CMakeLists.txt but not in app/CMakeLists.txt: "
            f"{sorted(only_root)}"
        )


def check_kconfig_single_source(errors: list[str]) -> None:
    app_kconfig = (ROOT / "app" / "Kconfig").read_text()
    module_kconfig = (ROOT / "Kconfig.ibattery").read_text()

    # No CONFIG_BATTERY_* option may be DEFINED outside app/Kconfig.battery.
    stray = re.findall(r"^config\s+(BATTERY_\w+)", app_kconfig, re.MULTILINE)
    if stray:
        errors.append(
            "app/Kconfig defines battery options directly (must live only in "
            f"app/Kconfig.battery): {sorted(set(stray))}"
        )

    # Both entry points must source the single options file.
    if "Kconfig.battery" not in app_kconfig:
        errors.append("app/Kconfig does not source Kconfig.battery")
    if "app/Kconfig.battery" not in module_kconfig:
        errors.append("Kconfig.ibattery does not source app/Kconfig.battery")


def main() -> int:
    errors: list[str] = []
    check_cmake_sync(errors)
    check_kconfig_single_source(errors)

    if errors:
        print("Build-config drift detected:\n")
        for e in errors:
            print(f"  ✗ {e}")
        print("\nKeep the app path and the Zephyr-module path in sync "
              "(see scripts/check_build_sync.py docstring).")
        return 1

    print("✓ app-path and module-path build/config are in sync.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
