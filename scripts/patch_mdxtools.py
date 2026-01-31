Import("env")

from pathlib import Path
import shutil
import math


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

def _write_sinctbl(path: Path, denominator: int, zero_crossings: int = 26, alpha: float = 5.0) -> None:
    total_sinc_samples = zero_crossings * denominator

    def I0(x: float) -> float:
        f = 0.0
        kfac = 1.0
        for k in range(50):
            if k > 0:
                if k == 1:
                    kfac = 1.0
                else:
                    kfac *= k
            frac = (x / 2.0) ** k / kfac
            f += frac * frac
        return f

    def kaiser(M: float, B: float, n: float) -> float:
        if n > M / 2.0:
            return 0.0
        I0B = I0(B)
        return I0(B * math.sqrt(1.0 - (n / (M / 2.0)) ** 2.0)) / I0B

    def sinc(x: float) -> float:
        if x == 0.0:
            return 1.0
        return math.sin(x) / x

    lines = []
    for n in range(total_sinc_samples + 1):
        k = kaiser(total_sinc_samples * 2.0, alpha * math.pi, float(n))
        s = sinc(float(n) * math.pi / float(denominator))
        ks = k * s
        ksi = int(ks * 32767)
        if ksi > 32767:
            ksi = 32767
        if ksi < -32767:
            ksi = -32767
        lines.append(f"{ksi:6d},")

    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


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
        "mdx_pcm_driver.c",
        "mdx_pcm_renderer.c",
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

        sinctbl4 = mdx_dir / "sinctbl4.h"
        sinctbl3 = mdx_dir / "sinctbl3.h"
        if not sinctbl4.exists():
            _write_sinctbl(sinctbl4, denominator=4)
        if not sinctbl3.exists():
            _write_sinctbl(sinctbl3, denominator=3)


# Run once for already-installed libdeps, then hook into common targets.
patch_mdxtools()
env.AddPostAction("libdeps", patch_mdxtools)
env.AddPreAction("buildprog", patch_mdxtools)
