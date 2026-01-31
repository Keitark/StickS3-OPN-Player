Import("env")

from pathlib import Path


def patch_mdxtools():
    project_dir = Path(env["PROJECT_DIR"])
    libdeps_dir = project_dir / ".pio" / "libdeps"
    if not libdeps_dir.exists():
        return

    for env_dir in libdeps_dir.iterdir():
        mdx_dir = env_dir / "mdxtools"
        if not mdx_dir.exists():
            continue

        stream_h = mdx_dir / "stream.h"
        if stream_h.exists():
            renamed = mdx_dir / "stream_mdxtools.h"
            try:
                stream_h.rename(renamed)
            except OSError:
                pass


patch_mdxtools()
