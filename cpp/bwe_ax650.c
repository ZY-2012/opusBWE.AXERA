/* OpusBWE.AXERA — AX650 16 kHz -> 48 kHz bandwidth extension demo.
 *
 * Uses the Opus OSCE-BWE DSP (upstream, BSD) with the feature net running on the
 * AX650 NPU (axmodel). usage:
 *   bwe_ax650 <bwe_feature_net.axmodel> <in_16k.wav> <out_48k.wav>
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "osce.h"
#include "osce_structs.h"
#include "cpu_support.h"
#include "ax_bwe.h"

#define FRAME16 320
#define FRAME48 (3 * FRAME16)

static int16_t *read_pcm16(const char *path, long *n_samples, int *sample_rate)
{
    FILE *f = fopen(path, "rb");
    unsigned char hdr[12];
    int16_t *data = NULL;
    if (!f) {
        fprintf(stderr, "cannot open %s\n", path);
        return NULL;
    }
    if (fread(hdr, 1, 12, f) != 12 || memcmp(hdr, "RIFF", 4) != 0 || memcmp(hdr + 8, "WAVE", 4) != 0) {
        fprintf(stderr, "not a wav: %s\n", path);
        fclose(f);
        return NULL;
    }
    while (1) {
        unsigned char chunk[8];
        uint32_t size;
        if (fread(chunk, 1, 8, f) != 8) break;
        memcpy(&size, chunk + 4, 4);
        if (memcmp(chunk, "fmt ", 4) == 0) {
            unsigned char fmt[16];
            if (size < 16 || fread(fmt, 1, 16, f) != 16) break;
            memcpy(sample_rate, fmt + 4, 4);
            if (size > 16) fseek(f, size - 16, SEEK_CUR);
        } else if (memcmp(chunk, "data", 4) == 0) {
            long n = (long)(size / 2);
            data = (int16_t *)malloc((size_t)size);
            if (!data) break;
            if (fread(data, 2, (size_t)n, f) != (size_t)n) {
                free(data);
                data = NULL;
                break;
            }
            *n_samples = n;
            break;
        } else {
            fseek(f, (long)size, SEEK_CUR);
        }
    }
    fclose(f);
    return data;
}

static int write_pcm16(const char *path, const int16_t *data, long n_samples, int sample_rate)
{
    FILE *f = fopen(path, "wb");
    uint32_t data_bytes, riff_size, byte_rate;
    uint16_t channels = 1, bits = 16, block_align = 2, fmt = 1;
    uint32_t fmtsz = 16;
    if (!f) return -1;
    data_bytes = (uint32_t)(n_samples * 2);
    riff_size = 36 + data_bytes;
    byte_rate = (uint32_t)sample_rate * 2;
    fwrite("RIFF", 1, 4, f);
    fwrite(&riff_size, 4, 1, f);
    fwrite("WAVEfmt ", 1, 8, f);
    fwrite(&fmtsz, 4, 1, f);
    fwrite(&fmt, 2, 1, f);
    fwrite(&channels, 2, 1, f);
    fwrite(&sample_rate, 4, 1, f);
    fwrite(&byte_rate, 4, 1, f);
    fwrite(&block_align, 2, 1, f);
    fwrite(&bits, 2, 1, f);
    fwrite("data", 1, 4, f);
    fwrite(&data_bytes, 4, 1, f);
    fwrite(data, 2, (size_t)n_samples, f);
    fclose(f);
    return 0;
}

int main(int argc, char **argv)
{
    const char *model, *in_path, *out_path;
    int16_t *in;
    long n_in = 0;
    int sr = 0;
    int arch;
    OSCEModel *osce;
    silk_OSCE_BWE_struct *bwe;
    long pos;
    long n_out = 0;
    int16_t *out;

    if (argc != 4) {
        fprintf(stderr, "usage: %s <bwe_feature_net.axmodel> <in_16k.wav> <out_48k.wav>\n", argv[0]);
        return 1;
    }
    model = argv[1];
    in_path = argv[2];
    out_path = argv[3];

    in = read_pcm16(in_path, &n_in, &sr);
    if (!in) return 2;
    if (sr != 16000) {
        fprintf(stderr, "input must be 16 kHz (got %d)\n", sr);
        free(in);
        return 2;
    }
    if (ax_bwe_init(model) != 0) {
        fprintf(stderr, "failed to init NPU model %s\n", model);
        free(in);
        return 3;
    }

    arch = opus_select_arch();
    osce = (OSCEModel *)calloc(1, sizeof(*osce));
    bwe = (silk_OSCE_BWE_struct *)calloc(1, sizeof(*bwe));
    if (!osce || !bwe) return 4;
    if (osce_load_models(osce, NULL, arch) != 0) {
        fprintf(stderr, "osce_load_models failed\n");
        return 4;
    }
    osce_bwe_reset(bwe);

    n_out = (n_in / FRAME16) * FRAME48;
    out = (int16_t *)malloc((size_t)n_out * sizeof(int16_t));
    if (!out) return 5;

    for (pos = 0; pos + FRAME16 <= n_in; pos += FRAME16) {
        osce_bwe(osce, bwe, out + (pos / FRAME16) * FRAME48, in + pos, FRAME16, arch);
    }
    /* the C implementation delays the output by OSCE_BWE_OUTPUT_DELAY samples
       (cross-frame continuity); remove the delay for a file-based tool */
    if (n_out > OSCE_BWE_OUTPUT_DELAY) {
        memmove(out, out + OSCE_BWE_OUTPUT_DELAY,
                (size_t)(n_out - OSCE_BWE_OUTPUT_DELAY) * sizeof(int16_t));
        memset(out + n_out - OSCE_BWE_OUTPUT_DELAY, 0,
               (size_t)OSCE_BWE_OUTPUT_DELAY * sizeof(int16_t));
    }
    write_pcm16(out_path, out, n_out, 48000);
    printf("[output] %s  %.2fs @ 48000 Hz\n", out_path, (double)n_out / 48000.0);

    free(out);
    free(in);
    free(osce);
    free(bwe);
    ax_bwe_deinit();
    return 0;
}
