#ifndef STREET_ROD_ENGINE_ADAPTER_H
#define STREET_ROD_ENGINE_ADAPTER_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#if defined(STREET_ROD_2)
#define SR_GAME_TITLE "Street Rod 2"
#define SR_DATA_EXE "SR.EXE"
void sr_start_timeout(unsigned milliseconds);
bool sr_opl_open(int sample_rate);
void sr_opl_close(void);
void sr_opl_write(uint8_t reg, uint8_t value);
int16_t sr_opl_sample(void);
#elif defined(STREET_ROD_SE)
#define SR_GAME_TITLE "Street Rod SE"
#define SR_DATA_EXE "SRSE.EXE"
#else
#define SR_GAME_TITLE "Street Rod"
#define SR_DATA_EXE "SR.EXE"
#endif

extern const char *sr_save_dir;
void *sr_load_file(const char *path, size_t *size);
bool sr_writable_file(const char *name);
bool sr_is_driving(void);

#endif
