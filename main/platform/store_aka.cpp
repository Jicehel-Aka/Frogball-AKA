// Sauvegarde : un petit fichier binaire sur la carte SD. Format : magic "FRG1" + SaveData.
#include "platform.h"
#include <stdio.h>
#include <string.h>

namespace platform {

static const char* kPath = "/sdcard/frogball/save.bin";
static constexpr uint32_t kMagic = 0x31475246;   // "FRG1"

static void sd_load(frog::SaveData& out) {
    FILE* f = fopen(kPath, "rb");
    if (!f) return;
    uint32_t magic = 0;
    frog::SaveData tmp;
    if (fread(&magic, sizeof(magic), 1, f) == 1 && magic == kMagic && fread(&tmp, sizeof(tmp), 1, f) == 1) {
        for (int i = 0; i < frog::TOP_N; i++) tmp.top[i].tri[3] = 0;   // trigramme toujours termine
        out = tmp;
    }
    fclose(f);
}

static void sd_save(const frog::SaveData& in) {
    FILE* f = fopen(kPath, "wb");
    if (!f) return;
    fwrite(&kMagic, sizeof(kMagic), 1, f);
    fwrite(&in, sizeof(in), 1, f);
    fclose(f);
}

frog::SaveStore store() {
    frog::SaveStore s;
    s.load = sd_load;
    s.save = sd_save;
    return s;
}

} // namespace platform
