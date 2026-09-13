// ID3 tag reading, deliberately small.
//
// Handles ID3v2.3/2.4 TIT2+TPE1 (the only frames anyone cares about here) and
// falls back to the 128-byte ID3v1 block at the end of the file. Unsynchronised
// frames and compressed frames are skipped rather than mis-parsed: showing the
// filename is a much better failure than showing mojibake.
#include "storage/library.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "esp_log.h"

static uint32_t syncsafe(const uint8_t *b) {
    return ((uint32_t)(b[0] & 0x7F) << 21) | ((uint32_t)(b[1] & 0x7F) << 14) |
           ((uint32_t)(b[2] & 0x7F) << 7)  |  (uint32_t)(b[3] & 0x7F);
}

// ID3 text frames are prefixed with an encoding byte. UTF-16 gets flattened to
// its low bytes, which is wrong for anything outside Latin-1 but keeps the
// panel font from drawing garbage.
static void copy_text(const uint8_t *src, size_t len, char *dst, size_t dn) {
    if (len == 0 || dn == 0) { if (dn) dst[0] = 0; return; }
    uint8_t enc = src[0];
    src++; len--;
    size_t o = 0;
    if (enc == 1 || enc == 2) {                      // UTF-16
        size_t i = (len >= 2 && (src[0] == 0xFF || src[0] == 0xFE)) ? 2 : 0;
        for (; i + 1 < len && o < dn - 1; i += 2) {
            uint8_t c = src[0] == 0xFE ? src[i + 1] : src[i];
            if (c >= 0x20 && c < 0x7F) dst[o++] = (char)c;
        }
    } else {
        for (size_t i = 0; i < len && o < dn - 1; i++)
            if (src[i] >= 0x20) dst[o++] = (char)src[i];
    }
    while (o && dst[o - 1] == ' ') o--;
    dst[o] = 0;
}

static const uint16_t kBitrateV1L3[16] = {
    0,32,40,48,56,64,80,96,112,128,160,192,224,256,320,0
};
static const uint32_t kRateV1[4] = { 44100, 48000, 32000, 0 };

// Duration from the first valid frame header. Constant-bitrate only; VBR files
// report long. Good enough for a progress bar, and cheap - no full scan.
static uint32_t estimate_duration(FILE *f, uint32_t audio_bytes) {
    uint8_t b[4];
    for (int tries = 0; tries < 4096; tries++) {
        if (fread(b, 1, 4, f) != 4) return 0;
        if (b[0] == 0xFF && (b[1] & 0xE0) == 0xE0) {
            int ver = (b[1] >> 3) & 3, layer = (b[1] >> 1) & 3;
            int bri = (b[2] >> 4) & 0xF, sri = (b[2] >> 2) & 3;
            if (ver == 3 && layer == 1 && kBitrateV1L3[bri] && kRateV1[sri]) {
                uint32_t kbps = kBitrateV1L3[bri];
                return audio_bytes / (kbps * 125);      // bytes / (kbps*1000/8)
            }
        }
        fseek(f, -3, SEEK_CUR);
    }
    return 0;
}

esp_err_t id3_read(const char *path, char *title, size_t tn,
                   char *artist, size_t an, uint32_t *dur) {
    if (tn) title[0] = 0;
    if (an) artist[0] = 0;
    if (dur) *dur = 0;

    FILE *f = fopen(path, "rb");
    if (!f) return ESP_FAIL;

    uint8_t hdr[10];
    uint32_t tag_size = 0;
    if (fread(hdr, 1, 10, f) == 10 && !memcmp(hdr, "ID3", 3) && hdr[3] >= 3) {
        tag_size = syncsafe(&hdr[6]);
        uint32_t pos = 0;
        uint8_t fh[10];
        uint8_t *buf = malloc(512);
        while (buf && pos + 10 < tag_size) {
            if (fread(fh, 1, 10, f) != 10) break;
            if (fh[0] == 0) break;                       // padding
            uint32_t fsz = (hdr[3] == 4) ? syncsafe(&fh[4])
                         : ((uint32_t)fh[4] << 24 | (uint32_t)fh[5] << 16 |
                            (uint32_t)fh[6] << 8  | fh[7]);
            bool want = !memcmp(fh, "TIT2", 4) || !memcmp(fh, "TPE1", 4);
            bool odd  = (fh[9] & 0x0C) != 0;             // compressed/encrypted
            if (want && !odd && fsz > 0 && fsz < 512) {
                if (fread(buf, 1, fsz, f) != fsz) break;
                if (!memcmp(fh, "TIT2", 4)) copy_text(buf, fsz, title, tn);
                else                        copy_text(buf, fsz, artist, an);
            } else {
                fseek(f, (long)fsz, SEEK_CUR);
            }
            pos += 10 + fsz;
        }
        free(buf);
    }

    if ((tn && !title[0]) || (an && !artist[0])) {       // ID3v1 fallback
        if (fseek(f, -128, SEEK_END) == 0) {
            uint8_t v1[128];
            if (fread(v1, 1, 128, f) == 128 && !memcmp(v1, "TAG", 3)) {
                char tmp[31];
                if (tn && !title[0])  { memcpy(tmp, v1 + 3,  30); tmp[30] = 0;
                                        for (int i=29;i>=0&&(tmp[i]==' '||!tmp[i]);i--) tmp[i]=0;
                                        strlcpy(title, tmp, tn); }
                if (an && !artist[0]) { memcpy(tmp, v1 + 33, 30); tmp[30] = 0;
                                        for (int i=29;i>=0&&(tmp[i]==' '||!tmp[i]);i--) tmp[i]=0;
                                        strlcpy(artist, tmp, an); }
            }
        }
    }

    if (dur) {
        fseek(f, 0, SEEK_END);
        long end = ftell(f);
        fseek(f, (long)(tag_size + 10), SEEK_SET);
        *dur = estimate_duration(f, (uint32_t)(end - (long)tag_size - 10));
    }
    fclose(f);
    return ESP_OK;
}
