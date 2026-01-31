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


def _strip_zlib_include(path: Path) -> None:
    try:
        if not path.exists():
            return
        text = path.read_text(encoding="utf-8")
    except OSError:
        return
    if "#include <zlib.h>" not in text:
        return
    text = text.replace("#include <zlib.h>\n", "")
    path.write_text(text, encoding="utf-8")


def patch_mdxtools(*_args, **_kwargs):
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
        "adpcm_midi_driver.c",
        "adpcm_midi_driver.h",
        "adpcm_pcm_mix_driver.c",
        "adpcm_pcm_mix_driver.h",
        "mdx_pcm_driver.c",
        "mdx_pcm_renderer.c",
        "speex_resampler.c",
        "speex_resampler.h",
        "fixed_resampler.c",
        "fixed_resampler.h",
        "gensinc.c",
        "resample-test.c",
        "pdxinfo.c",
        "mkpdx.c",
    }
    remove_globs = [
        "*-test.c",
        "mdx2*.c",
        "*midi*.c",
        "*midi*.h",
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
        _strip_zlib_include(mdx_dir / "tools.c")


# Run once for already-installed libdeps, then hook into common targets.
patch_mdxtools()
env.AddPostAction("libdeps", patch_mdxtools)
env.AddPreAction("buildprog", patch_mdxtools)
