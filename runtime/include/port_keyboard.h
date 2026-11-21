#ifndef PORT_KEYBOARD_H
#define PORT_KEYBOARD_H
#include "port_platform.h"
#ifdef __cplusplus
extern "C" {
#endif
enum { PORT_KEYBOARD_COLUMNS=6, PORT_KEYBOARD_CELLS=42, PORT_KEYBOARD_MAX=31 };
typedef enum { PORT_KEYBOARD_EDITING, PORT_KEYBOARD_ACCEPTED, PORT_KEYBOARD_CANCELLED } PortKeyboardResult;
typedef struct { char text[PORT_KEYBOARD_MAX+1]; unsigned length, limit, selected;
                PortKeyboardResult result; bool invalid_ascii; } PortKeyboard;
bool port_keyboard_init(PortKeyboard *state,const char *initial,unsigned limit);
void port_keyboard_update(PortKeyboard *state,const PortInput *input);
/* Own 5x7 font/palette; no game assets. Target is the standard 320x200 frame. */
bool port_keyboard_draw(const PortKeyboard *state,uint8_t *frame,int pitch);
bool port_keyboard_draw_title(const PortKeyboard *state,uint8_t *frame,int pitch,const char *title);
/* Draw transparent 5x7 ASCII into a 320x200 indexed frame. */
bool port_keyboard_draw_text(uint8_t *frame,int pitch,int x,int y,const char *text,uint8_t color);
const uint32_t *port_keyboard_palette(void);
const char *port_keyboard_cells(void);
#ifdef __cplusplus
}
#endif
#endif
