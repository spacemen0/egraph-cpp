#!/usr/bin/env python3
import argparse
import pathlib
import shutil
import subprocess
import sys


def run(command: list[str], cwd: pathlib.Path) -> None:
    subprocess.run(command, cwd=cwd, check=True)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Build and run unit tests with coverage"
    )
    parser.add_argument("gtest_filter", nargs="?", help="Optional gtest filter pattern")
    args = parser.parse_args()

    repo_root = pathlib.Path(__file__).resolve().parent

    print("Cleaning up old data...")
    shutil.rmtree(repo_root / "coverage_report", ignore_errors=True)
    for svg_file in repo_root.glob("*.svg"):
        svg_file.unlink(missing_ok=True)

    build_dir = repo_root / "build"
    if build_dir.is_dir():
        for gcda_file in build_dir.rglob("*.gcda"):
            gcda_file.unlink(missing_ok=True)
    else:
        print("No build directory found, skipping .gcda cleanup.")

    print("Building project...")
    run(
        [
            "cmake",
            "-G",
            "Ninja",
            "-S",
            ".",
            "-B",
            "build",
            "-DENABLE_COVERAGE=OFF",
            "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
        ],
        cwd=repo_root,
    )
    run(
        ["cmake", "--build", "build", "--target", "unit_tests", "--config", "Debug"],
        cwd=repo_root,
    )

    print("Running tests...")
    import os

    exe_name = "unit_tests.exe" if os.name == "nt" else "unit_tests"
    tests_base = repo_root / "build" / "tests"
    candidates = [
        tests_base / exe_name,
        tests_base / "Debug" / exe_name,
        tests_base / "Release" / exe_name,
    ]
    test_path = next((p for p in candidates if p.exists()), None)
    if not test_path:
        print(f"Error: Could not find {exe_name} in {tests_base}", file=sys.stderr)
        return 1

    test_command = [str(test_path)]
    if args.gtest_filter:
        test_command.append(f"--gtest_filter={args.gtest_filter}")
    run(test_command, cwd=repo_root)

    # print("Generating coverage report...")
    # run(["grcov", ".", "-s", ".", "--binary-path", "./build/", "-t", "html", "-o", "./coverage_report"], cwd=repo_root)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
