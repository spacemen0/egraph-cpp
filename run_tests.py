#!/usr/bin/env python3
import argparse
import pathlib
import shutil
import subprocess


def run(command: list[str], cwd: pathlib.Path) -> None:
    subprocess.run(command, cwd=cwd, check=True)


def main() -> int:
    parser = argparse.ArgumentParser(description="Build and run unit tests with coverage")
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
            "-S",
            ".",
            "-B",
            "build",
            # "-DENABLE_COVERAGE=ON",
            "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
        ],
        cwd=repo_root,
    )
    run(["cmake", "--build", "build", "--target", "unit_tests"], cwd=repo_root)

    print("Running tests...")
    test_command = [str(repo_root / "build" / "tests" / "unit_tests")]
    if args.gtest_filter:
        test_command.append(f"--gtest_filter={args.gtest_filter}")
    run(test_command, cwd=repo_root)

    # print("Generating coverage report...")
    # run(["grcov", ".", "-s", ".", "--binary-path", "./build/", "-t", "html", "-o", "./coverage_report"], cwd=repo_root)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
