Import("env")

from pathlib import Path


def _patch_psram_alloc(path: Path) -> None:
    if not path.exists():
        return
    try:
        text = path.read_text(encoding="utf-8")
    except OSError:
        return

    if "PATCH_PSRAM_ALLOC" in text:
        return

    insert_after = "#include \"mxdrv_context.internal.h\""
    if insert_after in text:
        text = text.replace(
            insert_after,
            insert_after + "\n\n#if defined(ESP32)\n#include <esp_heap_caps.h>\n#endif",
            1,
        )

    target = "context->m_impl = (MxdrvContextImpl *)malloc(allocSizeInBytes);"
    if target in text:
        replacement = (
            "#if defined(ESP32)\n"
            "\tcontext->m_impl = (MxdrvContextImpl *)heap_caps_malloc(allocSizeInBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);\n"
            "\tif (context->m_impl == NULL) {\n"
            "\t\tcontext->m_impl = (MxdrvContextImpl *)malloc(allocSizeInBytes);\n"
            "\t}\n"
            "#else\n"
            "\tcontext->m_impl = (MxdrvContextImpl *)malloc(allocSizeInBytes);\n"
            "#endif\n"
            "\t// PATCH_PSRAM_ALLOC\n"
        )
        text = text.replace(target, replacement, 1)

    path.write_text(text, encoding="utf-8")


def patch_portable_mdx(*_args, **_kwargs):
    project_dir = Path(env["PROJECT_DIR"])
    libdeps_dir = project_dir / ".pio" / "libdeps"
    if not libdeps_dir.exists():
        return

    for env_dir in libdeps_dir.iterdir():
        pm_dir = env_dir / "portable_mdx"
        if not pm_dir.exists():
            continue
        _patch_psram_alloc(pm_dir / "src" / "mxdrv" / "mxdrv_context.cpp")


patch_portable_mdx()
env.AddPostAction("libdeps", patch_portable_mdx)
env.AddPreAction("buildprog", patch_portable_mdx)
