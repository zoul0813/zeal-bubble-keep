#include <stdint.h>
#include <stddef.h>
#include <zvb_gfx.h>
#include "game.h"
#include "assets.h"
#include <zgdk/tileset.h>

zar_file_t pack;

/* ZAR provides page-safe reads; ZGDK handles chunk decoding and VRAM uploads. */
static zos_err_t read_tileset(void *entry, void *buffer, uint16_t *size) {
    return zar_entry_read(&pack, (zar_entry_t *)entry, buffer, size);
}

/*
 * The tileset and palette live in bubblekeep.zar (cmake/assets.cmake). The tileset streams
 * to VRAM one LZ chunk at a time through the music pattern buffers, which are free until the first track
 * plays, so it needs no buffer of its own.
 */

zos_err_t load_palette(gfx_context* ctx) {
    uint16_t size;
    void *buf = bgm_scratch(&size);
    zos_err_t err = zar_entry_load(&pack, "tiles.ztp", buf, &size);
    if (err != ERR_SUCCESS) return err;
    return gfx_palette_load(ctx, buf, size, 0) == GFX_SUCCESS ? ERR_SUCCESS : ERR_FAILURE;
}

zos_err_t load_tileset(gfx_context* ctx, gfx_tileset_options* options) {
    zar_entry_t entry;
    zos_err_t err;
    uint16_t size;
    void *buf = bgm_scratch(&size);
    if (size < TILE_CHUNK_MAX) return ERR_NO_MORE_MEMORY;
    err = zar_entry_from_name(&pack, "tiles.tch", &entry);
    if (err != ERR_SUCCESS) return err;
    return tileset_load_chunks(ctx, read_tileset, &entry, gfx_tileset_load_lz,
                               buf, size, options);
}
