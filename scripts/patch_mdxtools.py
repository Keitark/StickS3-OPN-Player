Import("env")

from pathlib import Path
import shutil
import math
import re


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


def _patch_adpcm_pcm_mix_driver(path: Path) -> None:
    if not path.exists():
        return
    try:
        text = path.read_text(encoding="utf-8")
    except OSError:
        return
    # Canonicalize deinit/estimate to be safe with NULL resampler.
    deinit_start = text.find("void adpcm_pcm_mix_driver_deinit")
    estimate_start = text.find("int adpcm_pcm_mix_driver_estimate", deinit_start + 1)
    if deinit_start != -1 and estimate_start != -1:
        deinit_block = (
            "void adpcm_pcm_mix_driver_deinit(struct adpcm_pcm_mix_driver *driver) {\n"
            "\tif(driver->output_resampler) {\n"
            "\t\tspeex_resampler_destroy(driver->output_resampler);\n"
            "\t\tdriver->output_resampler = NULL;\n"
            "\t}\n\n"
            "\t// no ADPCM status deinit\n\n"
            "\tfor(int i = 0; i < 4; i++) {\n"
            "\t\tfixed_resampler_deinit(&driver->resamplers[i]);\n"
            "\t}\n\n"
            "\tfor(int i = 0; i < 8; i++) {\n"
            "\t\tadpcm_mix_driver_channel_deinit(&driver->channels[i]);\n"
            "\t}\n"
            "}\n\n"
        )
        text = text[:deinit_start] + deinit_block + text[estimate_start:]

    estimate_start = text.find("int adpcm_pcm_mix_driver_estimate")
    run_start = text.find("int adpcm_pcm_mix_driver_run", estimate_start + 1)
    if estimate_start != -1 and run_start != -1:
        estimate_block = (
            "int adpcm_pcm_mix_driver_estimate(struct adpcm_pcm_mix_driver *driver, int buf_size) {\n"
            "\tif(!driver->output_resampler) return buf_size;\n"
            "\tspx_uint32_t in_len = 1, out_len = buf_size;\n"
            "\tspeex_resampler_estimate(driver->output_resampler, 0, &in_len, &out_len);\n"
            "\treturn out_len;\n"
            "}\n\n"
        )
        text = text[:estimate_start] + estimate_block + text[run_start:]

    if "PATCH_FAST_PATH" not in text:
        text = text.replace(
            "SPEEX_RESAMPLER_QUALITY_DEFAULT",
            "SPEEX_RESAMPLER_QUALITY_MIN",
        )

        text = re.sub(
            r"(adpcm_pcm_mix_driver_alloc_buffers\(driver, buf_size\);\n\n)(\tint err = RESAMPLER_ERR_SUCCESS;)",
            r"\1\t// PATCH_FAST_PATH: skip speex when output rate matches ADPCM base.\n"
            r"\tif(sample_rate == 15625) {\n"
            r"\t\tdriver->output_resampler = NULL;\n"
            r"\t\treturn 0;\n"
            r"\t}\n\n"
            r"\2",
            text,
            count=1,
        )

        text = re.sub(
            r"(int r = adpcm_pcm_mix_driver_alloc_buffers\(driver, buf_size\);\n\tif\(r != 0\)\n\t\treturn r;\n\n)",
            r"\1\t// PATCH_FAST_PATH: skip mixing when no ADPCM channels active.\n"
            r"\tint active = 0;\n"
            r"\tfor(int i = 0; i < 8; i++) {\n"
            r"\t\tif(adpcm_mix_driver_channel_is_active(&driver->channels[i])) { active = 1; break; }\n"
            r"\t}\n"
            r"\tif(!active) {\n"
            r"\t\tmemset(buf_l, 0, buf_size * sizeof(*buf_l));\n"
            r"\t\tmemset(buf_r, 0, buf_size * sizeof(*buf_r));\n"
            r"\t\treturn 0;\n"
            r"\t}\n\n",
            text,
            count=1,
        )

        text = re.sub(
            r"\tspeex_resampler_set_input_stride\(driver->output_resampler, 1\);\n"
            r"\tspeex_resampler_set_output_stride\(driver->output_resampler, 1\);\n"
            r"\tspx_uint32_t in_len = buf_size, out_len = buf_size;\n"
            r"\tspeex_resampler_estimate\(driver->output_resampler, 0, &in_len, &out_len\);\n",
            r"\tspx_uint32_t in_len = buf_size, out_len = buf_size;\n"
            r"\tif(driver->output_resampler) {\n"
            r"\t\tspeex_resampler_set_input_stride(driver->output_resampler, 1);\n"
            r"\t\tspeex_resampler_set_output_stride(driver->output_resampler, 1);\n"
            r"\t\tspeex_resampler_estimate(driver->output_resampler, 0, &in_len, &out_len);\n"
            r"\t}\n",
            text,
            count=1,
        )

        text = re.sub(
            r"\tspx_uint32_t in_len_l = buf_size;\n",
            r"\tif(!driver->output_resampler) {\n"
            r"\t\tmemcpy(buf_l, driver->mix_buf_l, buf_size * sizeof(*buf_l));\n"
            r"\t\tmemcpy(buf_r, driver->mix_buf_r, buf_size * sizeof(*buf_r));\n"
            r"\t\treturn 0;\n"
            r"\t}\n\n"
            r"\tspx_uint32_t in_len_l = buf_size;\n",
            text,
            count=1,
        )

    if "PATCH_PCM_FREQ_LIST" not in text:
        run_start = text.find("int adpcm_pcm_mix_driver_run")
        if run_start != -1:
            brace_start = text.find("{", run_start)
            if brace_start != -1:
                depth = 0
                run_end = -1
                for i in range(brace_start, len(text)):
                    if text[i] == "{":
                        depth += 1
                    elif text[i] == "}":
                        depth -= 1
                        if depth == 0:
                            run_end = i + 1
                            break
                if run_end != -1:
                    new_run = (
                    "int adpcm_pcm_mix_driver_run(struct adpcm_pcm_mix_driver *driver, stream_sample_t *buf_l, stream_sample_t *buf_r, int buf_size) {\n"
                    "\tint r = adpcm_pcm_mix_driver_alloc_buffers(driver, buf_size);\n"
                    "\tif(r != 0)\n"
                    "\t\treturn r;\n\n"
                    "\t// PATCH_PCM_FREQ_LIST: precompute active channels per frequency.\n"
                    "\tint freq_counts[5] = {0};\n"
                    "\tint freq_ch[5][8];\n"
                    "\tfor(int j = 0; j < 8; j++) {\n"
                    "\t\tif(!adpcm_mix_driver_channel_is_active(&driver->channels[j]))\n"
                    "\t\t\tcontinue;\n"
                    "\t\tint f = driver->channels[j].freq_num;\n"
                    "\t\tif(f < 0 || f > 4)\n"
                    "\t\t\tf = 4;\n"
                    "\t\tfreq_ch[f][freq_counts[f]++] = j;\n"
                    "\t}\n"
                    "\tint active = 0;\n"
                    "\tfor(int f = 0; f < 5; f++) {\n"
                    "\t\tif(freq_counts[f] > 0) { active = 1; break; }\n"
                    "\t}\n"
                    "\tif(!active) {\n"
                    "\t\tmemset(buf_l, 0, buf_size * sizeof(*buf_l));\n"
                    "\t\tmemset(buf_r, 0, buf_size * sizeof(*buf_r));\n"
                    "\t\treturn 0;\n"
                    "\t}\n\n"
                    "\tspx_uint32_t in_len = buf_size, out_len = buf_size;\n"
                    "\tif(driver->output_resampler) {\n"
                    "\t\tspeex_resampler_set_input_stride(driver->output_resampler, 1);\n"
                    "\t\tspeex_resampler_set_output_stride(driver->output_resampler, 1);\n"
                    "\t\tspeex_resampler_estimate(driver->output_resampler, 0, &in_len, &out_len);\n"
                    "\t}\n\n"
                    "\tmemset(driver->mix_buf_l, 0, in_len * sizeof(*driver->mix_buf_l));\n"
                    "\tmemset(driver->mix_buf_r, 0, in_len * sizeof(*driver->mix_buf_r));\n\n"
                    "\tstream_sample_t samp;\n"
                    "\tconst int pan_l = driver->adpcm_driver.pan & 0x01;\n"
                    "\tconst int pan_r = driver->adpcm_driver.pan & 0x02;\n\n"
                    "\tfor(int i = 0; i < 5; i++) {\n"
                    "\t\tint ch_count = freq_counts[i];\n"
                    "\t\tif(ch_count == 0)\n"
                    "\t\t\tcontinue;\n"
                    "\t\tif(i < 4) {\n"
                    "\t\t\tint estimated_in_len = fixed_resampler_estimate(&driver->resamplers[i], in_len);\n"
                    "\t\t\tint fixed_in_len = estimated_in_len;\n"
                    "\t\t\tint fixed_out_len = in_len;\n"
                    "\t\t\tif(estimated_in_len > 0)\n"
                    "\t\t\t\tmemset(driver->decode_buf, 0, estimated_in_len * sizeof(*driver->decode_buf));\n"
                    "\t\t\tfor(int idx = 0; idx < ch_count; idx++) {\n"
                    "\t\t\t\tstruct adpcm_mix_driver_channel *chan = &driver->channels[freq_ch[i][idx]];\n"
                    "\t\t\t\tfor(int k = 0; k < estimated_in_len; k++) {\n"
                    "\t\t\t\t\tsamp = adpcm_mix_driver_channel_get_sample(chan);\n"
                    "\t\t\t\t\tdriver->decode_buf[k] += samp;\n"
                    "\t\t\t\t}\n"
                    "\t\t\t}\n"
                    "\t\t\tfixed_resampler_resample(&driver->resamplers[i], driver->decode_buf, &fixed_in_len, driver->decode_resample_buf, &fixed_out_len);\n"
                    "\t\t\tif(pan_l) {\n"
                    "\t\t\t\tfor(int k = 0; k < in_len; k++)\n"
                    "\t\t\t\t\tdriver->mix_buf_l[k] += driver->decode_resample_buf[k];\n"
                    "\t\t\t}\n"
                    "\t\t\tif(pan_r) {\n"
                    "\t\t\t\tfor(int k = 0; k < in_len; k++)\n"
                    "\t\t\t\t\tdriver->mix_buf_r[k] += driver->decode_resample_buf[k];\n"
                    "\t\t\t}\n"
                    "\t\t} else {\n"
                    "\t\t\tfor(int idx = 0; idx < ch_count; idx++) {\n"
                    "\t\t\t\tstruct adpcm_mix_driver_channel *chan = &driver->channels[freq_ch[i][idx]];\n"
                    "\t\t\t\tfor(int k = 0; k < in_len; k++) {\n"
                    "\t\t\t\t\tsamp = adpcm_mix_driver_channel_get_sample(chan);\n"
                    "\t\t\t\t\tif(pan_l) driver->mix_buf_l[k] += samp;\n"
                    "\t\t\t\t\tif(pan_r) driver->mix_buf_r[k] += samp;\n"
                    "\t\t\t\t}\n"
                    "\t\t\t}\n"
                    "\t\t}\n"
                    "\t}\n\n"
                    "\tif(!driver->output_resampler) {\n"
                    "\t\tmemcpy(buf_l, driver->mix_buf_l, buf_size * sizeof(*buf_l));\n"
                    "\t\tmemcpy(buf_r, driver->mix_buf_r, buf_size * sizeof(*buf_r));\n"
                    "\t\treturn 0;\n"
                    "\t}\n\n"
                    "\tspx_uint32_t in_len_l = buf_size;\n"
                    "\tspx_uint32_t out_len_l = buf_size;\n"
                    "\tspeex_resampler_process_int(driver->output_resampler, 0, driver->mix_buf_l, &in_len_l, buf_l, &out_len_l);\n\n"
                    "\tspx_uint32_t in_len_r = buf_size;\n"
                    "\tspx_uint32_t out_len_r = buf_size;\n"
                    "\tspeex_resampler_process_int(driver->output_resampler, 1, driver->mix_buf_r, &in_len_r, buf_r, &out_len_r);\n\n"
                    "\treturn 0;\n"
                    "}\n"
                )
                    text = text[:run_start] + new_run + text[run_end:]

    path.write_text(text, encoding="utf-8")


def _patch_pdx_header(path: Path) -> None:
    if not path.exists():
        return
    try:
        text = path.read_text(encoding="utf-8")
    except OSError:
        return
    if "PDX_BANK_SIZE" in text and "pdx_file_free" in text:
        return

    text = text.replace("#define PDX_NUM_SAMPLES 96", "#define PDX_BANK_SIZE 96")

    text = re.sub(
        r"struct pdx_file \{\n\s*struct pdx_sample samples\[.*?\];\n\s*int num_samples;\n\};",
        "struct pdx_file {\n\tstruct pdx_sample *samples;\n\tint num_samples;\n};",
        text,
        flags=re.DOTALL,
    )

    if "pdx_file_free" not in text:
        text = text.replace(
            "int pdx_file_load(struct pdx_file *pdx, uint8_t *data, int data_len);",
            "void pdx_file_free(struct pdx_file *pdx);\nint pdx_file_load(struct pdx_file *pdx, uint8_t *data, int data_len);",
        )

    path.write_text(text, encoding="utf-8")


def _patch_pdx_source(path: Path) -> None:
    if not path.exists():
        return
    new_text = (
        "#include <stdlib.h>\n"
        "#include <string.h>\n\n"
        "#include \"pdx.h\"\n"
        "#include \"adpcm.h\"\n\n"
        "void pdx_file_free(struct pdx_file *f) {\n"
        "\tif(!f) return;\n"
        "\tif(f->samples) {\n"
        "\t\tfree(f->samples);\n"
        "\t\tf->samples = NULL;\n"
        "\t}\n"
        "\tf->num_samples = 0;\n"
        "}\n\n"
        "// TODO: check out of bounds conditions\n"
        "int pdx_file_load(struct pdx_file *f, uint8_t *data, int len) {\n"
        "\tif(!f || data == 0 || len < 8) return -1;\n\n"
        "\tpdx_file_free(f);\n\n"
        "\tint scan_entries = PDX_BANK_SIZE;\n"
        "\tint max_entries = len / 8;\n"
        "\tif(scan_entries > max_entries) scan_entries = max_entries;\n"
        "\tint min_ofs = 0;\n"
        "\tfor(int i = 0; i < scan_entries; i++) {\n"
        "\t\tint ofs = (data[i * 8] << 24) | (data[i * 8 + 1] << 16) | (data[i * 8 + 2] << 8) | data[i * 8 + 3];\n"
        "\t\tif(ofs > 0 && (min_ofs == 0 || ofs < min_ofs)) {\n"
        "\t\t\tmin_ofs = ofs;\n"
        "\t\t}\n"
        "\t}\n\n"
        "\tint entries = (min_ofs > 0 && (min_ofs % 8) == 0) ? (min_ofs / 8) : PDX_BANK_SIZE;\n"
        "\tif(entries < PDX_BANK_SIZE) entries = PDX_BANK_SIZE;\n"
        "\tif(entries > max_entries) entries = max_entries;\n\n"
        "\tf->samples = (struct pdx_sample *)calloc((size_t)entries, sizeof(*f->samples));\n"
        "\tif(!f->samples) return -1;\n"
        "\tf->num_samples = entries;\n\n"
        "\tint total_samples = 0;\n"
        "\tfor(int i = 0; i < entries; i++) {\n"
        "\t\tint ofs = (data[i * 8] << 24) | (data[i * 8 + 1] << 16) | (data[i * 8 + 2] << 8) | data[i * 8 + 3];\n"
        "\t\tint l = (data[i * 8 + 4] << 24) | (data[i * 8 + 5] << 16) | (data[i * 8 + 6] << 8) | data[i * 8 + 7];\n"
        "\t\tif(ofs < len && l > 0 && ofs + l <= len) {\n"
        "\t\t\tf->samples[i].data = data + ofs;\n"
        "\t\t\tif(ofs + l > len)\n"
        "\t\t\t\tl = len - ofs;\n"
        "\t\t\tf->samples[i].len = l;\n"
        "\t\t\ttotal_samples += l * 2;\n"
        "\t\t} else {\n"
        "\t\t\tf->samples[i].data = 0;\n"
        "\t\t\tf->samples[i].len = 0;\n"
        "\t\t}\n"
        "\t}\n"
        "\t/* PATCH_NO_DECODE: keep raw ADPCM data only to save RAM/CPU. */\n"
        "\tfor(int i = 0; i < entries; i++) {\n"
        "\t\tf->samples[i].decoded_data = 0;\n"
        "\t\tf->samples[i].num_samples = f->samples[i].len * 2;\n"
        "\t}\n\n"
        "\treturn 0;\n"
        "}\n"
    )

    path.write_text(new_text, encoding="utf-8")


def _patch_pdx(path: Path) -> None:
    _patch_pdx_header(path.parent / "pdx.h")
    _patch_pdx_source(path)

def _patch_mdx_driver(path: Path) -> None:
    if not path.exists():
        return
    try:
        text = path.read_text(encoding="utf-8")
    except OSError:
        return
    if "PATCH_KEYON_DELAY_BLOCK" not in text:
        if "key_on_delay_counter == 0 && track->staccato_counter == 0" in text:
            text = text.replace(
                "key_on_delay_counter == 0 && track->staccato_counter == 0",
                "key_on_delay_counter == 0 /* PATCH_KEYON_DELAY */",
            )

        # Ensure key-on delay doesn't shorten notes: pause staccato countdown until delay ends.
        text = re.sub(
            r"\ttrack->staccato_counter--;\n"
            r"\tif\(track->staccato_counter <= 0 && track->key_on_delay_counter == 0\)\n"
            r"\t\tmdx_driver_note_off\(driver, track_num\);\n",
            "\t// PATCH_KEYON_DELAY_BLOCK: staccato countdown starts after key-on delay.\n"
            "\tif(track->key_on_delay_counter == 0) {\n"
            "\t\ttrack->staccato_counter--;\n"
            "\t\tif(track->staccato_counter <= 0)\n"
            "\t\t\tmdx_driver_note_off(driver, track_num);\n"
            "\t}\n",
            text,
            count=1,
        )

    if "PATCH_PCM_BANK" not in text:
        text = text.replace(
            "track->adpcm_freq_num = 4;\n",
            "track->adpcm_freq_num = 4;\n\ttrack->pcm_bank = 0; // PATCH_PCM_BANK\n",
        )

        text = re.sub(
            r"case 0xfd: // Set voice\n\t\tif\(track_num < 8\) \{\n\t\t\ttrack->voice_num = track->data\[track->pos \+ 1\];\n\t\t\tfm_driver_load_voice\(driver->fm_driver, track_num, driver->mdx_file->voices\[track->voice_num\], track->voice_num, track->opm_volume, track->pan\);\n\t\t\}\n\t\ttrack->pos \+= 2;\n\t\tbreak;",
            "case 0xfd: // Set voice\n\t\tif(track_num < 8) {\n\t\t\ttrack->voice_num = track->data[track->pos + 1];\n\t\t\tfm_driver_load_voice(driver->fm_driver, track_num, driver->mdx_file->voices[track->voice_num], track->voice_num, track->opm_volume, track->pan);\n\t\t} else {\n\t\t\ttrack->pcm_bank = track->data[track->pos + 1]; // PATCH_PCM_BANK\n\t\t}\n\t\ttrack->pos += 2;\n\t\tbreak;",
            text,
            count=1,
        )

        text = text.replace(
            "} else if(track->note < 96 && r->pdx_file)\n\t\tadpcm_driver_play(r->adpcm_driver, track_num - 8, r->pdx_file->samples[track->note].data, r->pdx_file->samples[track->note].len, track->adpcm_freq_num, mdx_adpcm_volume_from_opm(track->opm_volume + r->fade_value));",
            "} else if(track->note >= 0 && r->pdx_file) {\n"
            "\t\tint sample_idx = track->note + (track->pcm_bank * PDX_BANK_SIZE);\n"
            "\t\tif(sample_idx >= 0 && sample_idx < r->pdx_file->num_samples) {\n"
            "\t\t\tadpcm_driver_play(r->adpcm_driver, track_num - 8, r->pdx_file->samples[sample_idx].data, r->pdx_file->samples[sample_idx].len, track->adpcm_freq_num, mdx_adpcm_volume_from_opm(track->opm_volume + r->fade_value));\n"
            "\t\t}\n"
            "\t}",
        )

    path.write_text(text, encoding="utf-8")


def _patch_mdx_driver_header(path: Path) -> None:
    if not path.exists():
        return
    try:
        text = path.read_text(encoding="utf-8")
    except OSError:
        return
    if "pcm_bank" in text:
        return
    text = text.replace(
        "int voice_num; // @\n",
        "int voice_num; // @\n\tint pcm_bank; // @@ (PCM bank)\n",
    )
    path.write_text(text, encoding="utf-8")


def patch_mdxtools(*_args, **_kwargs):
    project_dir = Path(env["PROJECT_DIR"])
    libdeps_dir = project_dir / ".pio" / "libdeps"
    if not libdeps_dir.exists():
        return
    # Keep upstream mdxtools intact for baseline behavior.
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

        # Keep mdxtools core sources unmodified for baseline behavior.


# Run once for already-installed libdeps, then hook into common targets.
patch_mdxtools()
env.AddPostAction("libdeps", patch_mdxtools)
env.AddPreAction("buildprog", patch_mdxtools)
