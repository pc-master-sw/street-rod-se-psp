#ifndef PORT_COMMON_H
#define PORT_COMMON_H
#include "port_platform.h"
#ifdef __cplusplus
extern "C" {
#endif
#define PORT_AUDIO_CAPACITY 8192
typedef struct { int16_t samples[PORT_AUDIO_CAPACITY*2]; size_t read, count; } PortAudioRing;
typedef struct { int x,y,width,height; } PortViewport;
/* Zero-initialize, including on video reinit. Source is packed 0x??RRGGBB,
 * outside the cache; rgba exposes count opaque 0xRRGGBBAA colors after success.
 * Invalid input leaves the cache unchanged. */
typedef struct { uint32_t source[256], rgba[256]; unsigned count; uint64_t updates; } PortPaletteCache;
bool port_palette_cache_rgb888(PortPaletteCache *cache,const uint32_t *source,unsigned count);
bool port_frame_valid(const uint8_t *pixels, int w, int h, int pitch,
                      const uint32_t *palette, int colors);
uint32_t port_rgba_to_abgr(uint32_t rgba);
PortViewport port_viewport(int sw, int sh, int w, int h, bool aspect_4_3);
void port_input_update(PortInput *out, uint32_t held, uint32_t *previous);
/* Block only buttons held when a modal closes, until each is released. New
 * buttons remain available. Initialize blocked with the modal's last held. */
uint32_t port_input_after_modal(uint32_t held, uint32_t *blocked);
size_t port_ring_push(PortAudioRing *ring, const int16_t *samples, size_t frames);
size_t port_ring_pop(PortAudioRing *ring, int16_t *samples, size_t frames);
bool port_asset_path(char *out, size_t capacity, const char *root, const char *relative);
bool port_asset_read(const char *root, const char *relative, void *data, size_t capacity, size_t *size);
#ifdef __cplusplus
}
#endif
#endif
