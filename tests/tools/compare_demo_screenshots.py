#!/usr/bin/env python3
"""Run deterministic demos and compare GDI/OpenGL frame captures.

The selected demos save a fixed frame through EGE itself, so the comparison
does not depend on desktop occlusion, window placement, or capture timing.
"""

from __future__ import annotations

import argparse
import json
import shutil
import subprocess
import sys
from pathlib import Path

try:
    from PIL import Image, ImageChops, ImageEnhance, ImageStat
except ImportError as error:  # pragma: no cover - dependency diagnostic
    raise SystemExit(
        "Pillow is required: install it with 'python -m pip install Pillow'"
    ) from error


ROOT = Path(__file__).resolve().parents[2]
DEMOS = {
    "graph_alpha": "graph_alpha_frame10.png",
    "graph_backend_validation": "graph_backend_validation_frame10.png",
    "graph_ball": "graph_ball_frame10.png",
    "test_demo": "test_demo_frame10.png",
}

# This case intentionally packs antialiased curves, text, image transfers, and
# three fill patterns into a single frame.  Its edge-pixel density is much
# higher than the ordinary demos, and Windows GDI maps WIDE_DOT_FILL to its
# legacy diagonal-cross hatch even though the public enum specifies sparse
# dots.  Keep a bounded case-specific allowance while still reporting every
# raw metric and the enhanced diff image for review.
CASE_LIMITS = {
    "graph_backend_validation": {
        "mean_absolute_error": 5.5,
        "large_difference_fraction": 0.06,
    },
}


def executable_path(build_dir: Path, name: str, configuration: str) -> Path:
    suffix = ".exe" if sys.platform == "win32" else ""
    candidates = (
        build_dir / "demo" / configuration / f"{name}{suffix}",
        build_dir / "demo" / f"{name}{suffix}",
        build_dir / configuration / f"{name}{suffix}",
        build_dir / f"{name}{suffix}",
    )
    for candidate in candidates:
        if candidate.is_file():
            return candidate.resolve()
    raise FileNotFoundError(
        f"unable to find {name}{suffix} under {build_dir}; build the demos first"
    )


def run_demo(
    backend: str,
    build_dir: Path,
    name: str,
    screenshot_name: str,
    output_dir: Path,
    configuration: str,
    timeout: float,
) -> Path:
    executable = executable_path(build_dir, name, configuration)
    run_dir = output_dir / "runs" / backend / name
    run_dir.mkdir(parents=True, exist_ok=True)
    generated = run_dir / screenshot_name
    if generated.exists():
        generated.unlink()

    completed = subprocess.run(
        [str(executable)],
        cwd=run_dir,
        capture_output=True,
        text=True,
        timeout=timeout,
        check=False,
    )
    if completed.returncode != 0:
        raise RuntimeError(
            f"{backend}/{name} exited with {completed.returncode}\n"
            f"stdout:\n{completed.stdout}\nstderr:\n{completed.stderr}"
        )
    if not generated.is_file():
        raise RuntimeError(
            f"{backend}/{name} exited successfully without creating {screenshot_name}"
        )

    captured = output_dir / f"{name}_{backend}.png"
    shutil.copy2(generated, captured)
    return captured


def compare_pair(gdi_path: Path, opengl_path: Path, diff_path: Path) -> dict[str, object]:
    with Image.open(gdi_path) as gdi_source, Image.open(opengl_path) as gl_source:
        gdi = gdi_source.convert("RGB")
        opengl = gl_source.convert("RGB")
        if gdi.size != opengl.size:
            return {
                "passed": False,
                "gdi_size": list(gdi.size),
                "opengl_size": list(opengl.size),
                "reason": "image dimensions differ",
            }

        difference = ImageChops.difference(gdi, opengl)
        channels = difference.split()
        maximum_channel = ImageChops.lighter(
            ImageChops.lighter(channels[0], channels[1]), channels[2]
        )
        histogram = maximum_channel.histogram()
        pixel_count = gdi.width * gdi.height
        changed_fraction = (pixel_count - histogram[0]) / pixel_count
        large_difference_fraction = sum(histogram[33:]) / pixel_count
        mean_absolute_error = sum(ImageStat.Stat(difference).mean) / 3.0
        maximum_error = max(channel_range[1] for channel_range in difference.getextrema())

        # Brighten the raw absolute difference so small antialiasing changes
        # remain visible during failure triage.
        ImageEnhance.Brightness(difference).enhance(4.0).save(diff_path)
        return {
            "passed": True,
            "size": list(gdi.size),
            "mean_absolute_error": mean_absolute_error,
            "maximum_error": maximum_error,
            "changed_pixel_fraction": changed_fraction,
            "large_difference_fraction": large_difference_fraction,
        }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--gdi-build", type=Path, required=True)
    parser.add_argument("--opengl-build", type=Path, required=True)
    parser.add_argument(
        "--output-dir", type=Path, default=ROOT / "build" / "visual-results" / "current"
    )
    parser.add_argument("--configuration", default="Release")
    parser.add_argument("--timeout", type=float, default=30.0)
    parser.add_argument("--max-mae", type=float, default=2.0)
    parser.add_argument("--max-large-difference-fraction", type=float, default=0.03)
    parser.add_argument(
        "--skip-run", action="store_true",
        help="compare name_gdi.png/name_opengl.png files already in the output directory",
    )
    args = parser.parse_args()
    args.output_dir.mkdir(parents=True, exist_ok=True)

    report: dict[str, object] = {
        "limits": {
            "mean_absolute_error": args.max_mae,
            "large_difference_fraction": args.max_large_difference_fraction,
        },
        "demos": {},
    }
    all_passed = True
    for name, screenshot_name in DEMOS.items():
        if args.skip_run:
            gdi_path = args.output_dir / f"{name}_gdi.png"
            opengl_path = args.output_dir / f"{name}_opengl.png"
        else:
            gdi_path = run_demo(
                "gdi", args.gdi_build, name, screenshot_name, args.output_dir,
                args.configuration, args.timeout,
            )
            opengl_path = run_demo(
                "opengl", args.opengl_build, name, screenshot_name, args.output_dir,
                args.configuration, args.timeout,
            )

        metrics = compare_pair(
            gdi_path, opengl_path, args.output_dir / f"{name}_diff.png"
        )
        metrics["gdi"] = str(gdi_path)
        metrics["opengl"] = str(opengl_path)
        limits = CASE_LIMITS.get(
            name,
            {
                "mean_absolute_error": args.max_mae,
                "large_difference_fraction": args.max_large_difference_fraction,
            },
        )
        metrics["limits"] = limits
        if metrics.get("passed"):
            metrics["passed"] = (
                float(metrics["mean_absolute_error"])
                <= float(limits["mean_absolute_error"])
                and float(metrics["large_difference_fraction"])
                <= float(limits["large_difference_fraction"])
            )
        all_passed = bool(metrics["passed"]) and all_passed
        report["demos"][name] = metrics
        status = "PASS" if metrics["passed"] else "FAIL"
        print(
            f"{status} {name}: MAE={metrics.get('mean_absolute_error', 'n/a')}, "
            f"large-diff={metrics.get('large_difference_fraction', 'n/a')}"
        )

    report["passed"] = all_passed
    report_path = args.output_dir / "report.json"
    report_path.write_text(json.dumps(report, indent=2), encoding="utf-8")
    print(f"report: {report_path}")
    return 0 if all_passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
