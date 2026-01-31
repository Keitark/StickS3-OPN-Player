Import("env")

from pathlib import Path
import shutil


def _safe_unlink(path: Path) -> None:
    try:
        if path.exists():
            path.unlink()
    except OSError:
        pass


def _safe_rmtree(path: Path) -> None:
    try:
        if path.exists():
            shutil.rmtree(path)
    except OSError:
        pass


def patch_mdxtools():
    project_dir = Path(env["PROJECT_DIR"])
    libdeps_dir = project_dir / ".pio" / "libdeps"
    if not libdeps_dir.exists():
        return

    remove_files = {
        "cmdline.c",
        "stream.c",
        "stream.h",
        "Stream.h",
        "mdxplay.c",
        "mdxinfo.c",
        "mdxdump.c",
        "mdx_compiler.c",
        "mdx_decompiler.c",
        "adpcm-encode.c",
        "adpcm-decode.c",
        "mdx_pcm_driver.c",
        "mdx_pcm_renderer.c",
        "resample-test.c",
    }
    remove_globs = [
        "*-test.c",
        "mdx2*.c",
        "mml*.c",
        "pdx2*.c",
    ]

    for env_dir in libdeps_dir.iterdir():
        mdx_dir = env_dir / "mdxtools"
        if not mdx_dir.exists():
            continue

        for name in remove_files:
            _safe_unlink(mdx_dir / name)

        for pattern in remove_globs:
            for hit in mdx_dir.glob(pattern):
                _safe_unlink(hit)

        _safe_rmtree(mdx_dir / "midilib")


patch_mdxtools()
