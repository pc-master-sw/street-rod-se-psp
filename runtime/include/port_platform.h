#ifndef PORT_PLATFORM_H
#define PORT_PLATFORM_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
enum { PORT_UP=1, PORT_DOWN=2, PORT_LEFT=4, PORT_RIGHT=8,
       PORT_FIRE=16, PORT_ALT=32, PORT_START=64, PORT_SELECT=128,
       PORT_ACTION3=256, PORT_ACTION4=512, PORT_LEFT_TRIGGER=1024, PORT_RIGHT_TRIGGER=2048 };
typedef struct { const char *title; int width, height; bool aspect_4_3;
                 const char *asset_root; bool enable_audio; } PortConfig;
typedef struct { uint32_t held, pressed, released; bool quit, suspended; } PortInput;
typedef struct { uint64_t presented, audio_accepted, audio_played, audio_underruns;
                 size_t audio_queued; } PortStats;
/* Single main-thread caller. Init is transactional; shutdown is idempotent.
 * RGBA is 0xRRGGBBAA. Pixels/palette are borrowed only until return.
 * PCM: stereo interleaved S16 at 44100 Hz. Nonblocking push returns accepted
 * frames; caller retains/retries the rest. Capacity: 8192 frames.
 * Read-only assets use normalized relative paths, no '..' or device names.
 */
bool port_platform_init(const PortConfig *config);
void port_platform_shutdown(void);
bool port_platform_poll_input(PortInput *out);
bool port_platform_present_indexed(const uint8_t *pixels, int width, int height,
                                  int pitch, const uint32_t *palette_rgba,
                                  int palette_size);
size_t port_platform_audio_push_s16(const int16_t *samples, size_t frames);
uint64_t port_platform_ticks_us(void);
void port_platform_sleep_us(uint32_t us);
bool port_platform_read_asset(const char *relative, void *data, size_t capacity,
                              size_t *size);
void port_platform_get_stats(PortStats *out);
/* Diagnostic displayed RGB (0xRRGGBB). Buffered emulator GPU backends may
 * leave guest VRAM stale; strict GE readback tests use PPSSPP software GE. */
bool port_platform_read_pixel(int x, int y, uint32_t *rgb);
#ifdef __cplusplus
}
#endif
#endif
