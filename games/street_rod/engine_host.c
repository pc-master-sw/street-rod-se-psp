#include "engine_adapter.h"
#include "host.h"
#include "mem.h"
#include "port_common.h"
#include "port_keyboard.h"
#include "symbols.h"

#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

enum { FRAME_W = 320, FRAME_H = 200, AUDIO_RATE = 44100, NS_PER_S = 1000000000 };
#ifdef STREET_ROD_2
#include "game/race_int.h"
enum { SOURCE_W = HOST_FRAME_MAX_W, DS_TEXT_BACKSPACE = DS_kbd_xlat + 0x0e };
#define DS_demo_active DS_demo
static int frame_width = FRAME_W;
static uint64_t exit_deadline;
static bool opl_enabled;
#else
enum { DS_TEXT_BACKSPACE = 0x4774, DS_DASHBOARD_SPLIT = 0x824a };
enum { SOURCE_W = FRAME_W };
#endif

static const char *game_dir;
static bool initialized, suspended, realtime = true;
static void (*tick_handler)(void);
static void (*kbd_handler)(u8);
static void (*focus_lost_handler)(void);
static bool (*frame_source)(u32 *);
static u32 frame[SOURCE_W * FRAME_H], palette[64];
static u8 indexed[FRAME_W * FRAME_H];
static uint64_t clock_start, ticks, last_present;
static u32 divisor = PIT_DIV_GAME;
static PortKeyboard editor;
static bool editing, redraw = true, queued_enter;
static unsigned text_index;
static uint32_t sent_buttons, blocked_buttons;
#if defined(STREET_ROD_SE) || defined(STREET_ROD_2)
static u8 click_scan = 0x39;
#endif

static uint64_t now_ns(void)
{
    return port_platform_ticks_us() * 1000;
}

static void sleep_ns(uint64_t ns)
{
    port_platform_sleep_us((uint32_t)((ns + 999) / 1000));
}

static uint64_t tick_due(uint64_t tick)
{
    uint64_t cycles = tick * divisor;
    return clock_start + cycles / PIT_HZ * NS_PER_S + cycles % PIT_HZ * NS_PER_S / PIT_HZ;
}

bool host_init(const char *dir, int scale, bool fullscreen)
{
    (void)scale;
    (void)fullscreen;
    const PortConfig config = {SR_GAME_TITLE, FRAME_W, FRAME_H, true, dir, true};
    if (!port_platform_init(&config))
        return false;
    initialized = true;
    game_dir = dir;
    clock_start = now_ns();
    for (unsigned color = 0; color < 64; ++color)
        palette[color] = (color >> 4) * UINT32_C(0x55000000) |
                         (color >> 2 & 3) * UINT32_C(0x550000) |
                         (color & 3) * UINT32_C(0x5500) | 255u;
#ifndef __PSP__
    const char *dump = getenv("SR_AUDIO_DUMP");
    if (dump && !host_audio_dump_begin(dump))
        host_fatal("could not open the audio dump");
#endif
    return true;
}

void host_shutdown(void)
{
    if (!initialized)
        return;
    initialized = false;
    host_audio_dump_end();
#ifdef STREET_ROD_2
    sr_opl_close();
#endif
    port_platform_shutdown();
}

void host_set_tick_handler(void (*handler)(void)) { tick_handler = handler; }
void host_set_kbd_handler(void (*handler)(u8)) { kbd_handler = handler; }
void host_set_focus_lost_handler(void (*handler)(void)) { focus_lost_handler = handler; }
#ifdef STREET_ROD_2
bool sr_is_driving(void) { return DSW(DS_split_row) != 0 || DSB(DS_racing) != 0; }

void sr_start_timeout(unsigned milliseconds)
{
    exit_deadline = now_ns() + (uint64_t)milliseconds * 1000000u;
}
#else
bool sr_is_driving(void) { return DSB(DS_DASHBOARD_SPLIT) != 0; }
#endif

void host_set_frame_source(bool (*compose)(u32 *), int width, int height)
{
    if ((width != FRAME_W && width != SOURCE_W) || height != FRAME_H)
        host_fatal("unsupported frame size %d x %d", width, height);
#ifdef STREET_ROD_2
    frame_width = width;
#endif
    frame_source = compose;
    redraw = true;
}

void host_set_pit_divisor(u16 value)
{
    u32 next = value ? value : 65536u;
    if (next != divisor) {
        clock_start = tick_due(ticks);
        ticks = 0;
        divisor = next;
    }
}

static void scan(u8 byte)
{
    if (kbd_handler)
        kbd_handler(byte);
}

static u8 text_scan(char key)
{
    static const u8 letters[] = {
        0x1e, 0x30, 0x2e, 0x20, 0x12, 0x21, 0x22, 0x23, 0x17, 0x24, 0x25, 0x26, 0x32,
        0x31, 0x18, 0x19, 0x10, 0x13, 0x1f, 0x14, 0x16, 0x2f, 0x11, 0x2d, 0x15, 0x2c
    };
    if (key >= 'A' && key <= 'Z')
        return letters[key - 'A'];
    if (key >= '1' && key <= '9')
        return (u8)(2 + key - '1');
    switch (key) {
    case '0': return 0x0b;
    case '-': return 0x0c;
    case '.': return 0x34;
    case ' ': return 0x39;
    default: return 0;
    }
}

static void controller_keys(uint32_t held)
{
    static const struct { uint32_t button; u8 scan; } keys[] = {
        {PORT_UP, 0x48}, {PORT_DOWN, 0x50}, {PORT_LEFT, 0x4b}, {PORT_RIGHT, 0x4d},
        {PORT_FIRE, 0x39}, {PORT_ALT, 0x01}, {PORT_START, 0x1c}, {PORT_SELECT, 0x0e},
        {PORT_LEFT_TRIGGER, 0x2c}, {PORT_RIGHT_TRIGGER, 0x1e}, {PORT_ACTION4, 0x32}
    };
    uint32_t changed = held ^ sent_buttons;
#if defined(STREET_ROD_SE) || defined(STREET_ROD_2)
    for (unsigned index = 0; index < sizeof(keys) / sizeof(*keys); ++index) {
        uint32_t button = keys[index].button;
        if (!(changed & button))
            continue;
        u8 code = keys[index].scan;
        bool pressed = (held & button) != 0;
        if (button == PORT_FIRE) {
            /* Text fields translate Space to a character. The alternate
             * scancode still clicks buttons on the license and save screens. */
            if (pressed)
                click_scan = DSB(DS_TEXT_BACKSPACE) == 8 ? 0x55 : code;
            code = click_scan;
        }
        scan((u8)(code | (pressed ? 0 : 0x80)));
    }
#else
    for (unsigned index = 0; index < sizeof(keys) / sizeof(*keys); ++index)
        if (changed & keys[index].button)
            scan((u8)(keys[index].scan | (held & keys[index].button ? 0 : 0x80)));
#endif
    sent_buttons = held;
}

static void poll_input(void)
{
#ifdef STREET_ROD_2
    if (exit_deadline && now_ns() >= exit_deadline)
        exit(0);
#endif
    PortInput input;
    if (!port_platform_poll_input(&input) || input.quit)
        exit(0);
    if (input.suspended != suspended) {
        suspended = input.suspended;
        if (suspended && focus_lost_handler)
            focus_lost_handler();
        clock_start = now_ns();
        ticks = 0;
        redraw = true;
        sent_buttons = 0;
        blocked_buttons = input.held;
    }
    if (suspended)
        return;
    uint32_t raw = input.held;
    input.held = port_input_after_modal(input.held, &blocked_buttons);
    input.pressed &= input.held;
    if (editing) {
        if (input.pressed) {
            port_keyboard_update(&editor, &input);
            redraw = true;
        }
        if (editor.result != PORT_KEYBOARD_EDITING) {
            editing = false;
            blocked_buttons = raw;
            if (editor.result == PORT_KEYBOARD_ACCEPTED) {
                text_index = 0;
                queued_enter = DSB(DS_TEXT_BACKSPACE) == 8;
            }
        }
        return;
    }
    if ((input.pressed & PORT_ACTION3) && kbd_handler && !sr_is_driving() && !DSW(DS_demo_active)) {
        const bool text_dialog = DSB(DS_TEXT_BACKSPACE) == 8;
        editing = port_keyboard_init(&editor, "", text_dialog ? 15 : 1);
        text_index = editor.length;
        queued_enter = false;
        blocked_buttons = raw;
        controller_keys(0);
        redraw = true;
        return;
    }
    uint32_t held = input.held;
    if (!sr_is_driving())
        held &= ~(PORT_LEFT_TRIGGER | PORT_RIGHT_TRIGGER);
    controller_keys(held);
    if (!DSB(DS_key) && text_index < editor.length) {
        u8 code = text_scan(editor.text[text_index++]);
        if (code) {
            scan(code);
            scan((u8)(code | 0x80));
        }
    } else if (!DSB(DS_key) && queued_enter) {
        queued_enter = false;
        scan(0x1c);
        scan(0x9c);
    }
}

static uint64_t timeline, tick_time, ns_remainder;
static bool in_tick;
static u8 speaker_port;
static u32 pending_count = 65536, speaker_count = 65536;
static bool output_high = true;
static uint64_t transition, position, sample_end = PIT_HZ, high_time;
static int dc_input, dc_output, low_pass;
static s16 pcm[2048 * 2];
static unsigned pcm_frames;
static FILE *wave;
static u32 wave_samples;

static void flush_audio(void)
{
    if (!pcm_frames)
        return;
    if (wave) {
        for (unsigned sample = 0; sample < pcm_frames; ++sample)
            fwrite(&pcm[sample * 2], sizeof(s16), 1, wave);
        wave_samples += pcm_frames;
    }
    port_platform_audio_push_s16(pcm, pcm_frames);
    pcm_frames = 0;
}

static void emit_sample(void)
{
    int input = (int)(high_time * 10000 / PIT_HZ);
    dc_output = input - dc_input + dc_output * 32675 / 32768;
    dc_input = input;
    low_pass += (dc_output - low_pass) * 24914 / 32768;
#ifdef STREET_ROD_2
    int mixed = low_pass;
    if (opl_enabled)
        mixed += sr_opl_sample();
    pcm[2 * pcm_frames] = pcm[2 * pcm_frames + 1] =
        (s16)(mixed < INT16_MIN ? INT16_MIN : mixed > INT16_MAX ? INT16_MAX : mixed);
#else
    pcm[2 * pcm_frames] = pcm[2 * pcm_frames + 1] = (s16)low_pass;
#endif
    if (++pcm_frames == sizeof(pcm) / sizeof(*pcm) / 2)
        flush_audio();
}

static u32 half_period(void)
{
    u32 period = (speaker_count + output_high) / 2;
    return period ? period : 1;
}

static void render_audio(uint64_t target_clock)
{
    uint64_t target = target_clock * AUDIO_RATE;
    while (position < target) {
        const bool gate = speaker_port & 1;
        uint64_t edge = transition * AUDIO_RATE;
        uint64_t end = target < sample_end ? target : sample_end;
        if (gate && edge < end)
            end = edge;
        if ((speaker_port & 2) && (!gate || output_high))
            high_time += end - position;
        position = end;
        if (gate && position == edge) {
            output_high = !output_high;
            speaker_count = pending_count;
            transition += half_period();
        }
        if (position == sample_end) {
            emit_sample();
            high_time = 0;
            sample_end += PIT_HZ;
        }
    }
}

static void run_tick(void)
{
    ++ticks;
    tick_time += divisor;
    if (timeline < tick_time)
        timeline = tick_time;
    tick_time = timeline;
    render_audio(timeline);
    flush_audio();
    in_tick = true;
    if (tick_handler)
        tick_handler();
    in_tick = false;
}

#ifdef STREET_ROD_2
void host_opl_enable(bool on)
{
    render_audio(timeline);
    if (on && !opl_enabled && !sr_opl_open(AUDIO_RATE))
        host_fatal("could not initialize AdLib audio");
    if (!on)
        sr_opl_close();
    opl_enabled = on;
}

void host_opl_write(u8 reg, u8 value)
{
    if (!opl_enabled)
        return;
    render_audio(timeline);
    sr_opl_write(reg, value);
}
#endif

static void pace_until(uint64_t due)
{
    uint64_t now;
    while ((now = now_ns()) < due) {
        poll_input();
        if (suspended)
            return;
        uint64_t remaining = due - now;
        sleep_ns(remaining < 1000000 ? remaining : 1000000);
    }
}

void host_pit2_divisor(u16 value)
{
    render_audio(timeline);
    pending_count = value ? value : 65536u;
}

void host_port61(u8 value)
{
    render_audio(timeline);
    if ((value & 1) && !(speaker_port & 1)) {
        output_high = true;
        speaker_count = pending_count;
        transition = timeline + half_period();
    } else if (!(value & 1)) {
        output_high = true;
    }
    speaker_port = value;
}

u8 host_port61_get(void) { return speaker_port; }

void host_speaker(u16 value, bool on)
{
    host_pit2_divisor(value);
    host_port61(on ? (u8)(speaker_port | 3) : (u8)(speaker_port & ~3));
}

void host_busy_wait_ns(uint64_t ns)
{
    uint64_t scaled = ns * PIT_HZ + ns_remainder;
    uint64_t elapsed = scaled / NS_PER_S;
    ns_remainder = scaled % NS_PER_S;
    if (in_tick) {
        timeline += elapsed;
        return;
    }
    while (timeline + elapsed >= tick_time + divisor) {
        uint64_t next = tick_time + divisor;
        if (timeline < next) {
            elapsed -= next - timeline;
            timeline = next;
        }
        if (realtime)
            pace_until(tick_due(ticks + 1));
        run_tick();
    }
    timeline += elapsed;
    if (realtime)
        pace_until(tick_due(ticks) + (timeline - tick_time) * NS_PER_S / PIT_HZ);
}

void host_busy_wait_us(u32 us) { host_busy_wait_ns((uint64_t)us * 1000); }
void host_set_realtime(bool on) { realtime = on; }
void host_run_tick_now(void) { run_tick(); }
uint64_t host_audio_clock(void) { return timeline; }

static void wave_header(u32 samples)
{
    u32 fields[] = {
        UINT32_C(0x46464952), 36 + samples * 2, UINT32_C(0x45564157), UINT32_C(0x20746d66),
        16, UINT32_C(0x00010001), AUDIO_RATE, AUDIO_RATE * 2, UINT32_C(0x00100002),
        UINT32_C(0x61746164), samples * 2
    };
    for (unsigned index = 0; index < sizeof(fields) / sizeof(*fields); ++index)
        for (unsigned byte = 0; byte < 4; ++byte)
            fputc((int)(fields[index] >> (8 * byte) & 0xff), wave);
}

bool host_audio_dump_begin(const char *path)
{
    host_audio_dump_end();
    wave = fopen(path, "wb");
    if (!wave)
        return false;
    wave_samples = 0;
    wave_header(0);
    return true;
}

void host_audio_dump_end(void)
{
    if (wave) {
        flush_audio();
        rewind(wave);
        wave_header(wave_samples);
        fclose(wave);
        wave = NULL;
    }
}

static bool present_frame(void)
{
    const u32 *colors = palette;
    int count = 64;
    bool dirty = frame_source && frame_source(frame);
    if (editing) {
        if (!redraw)
            return false;
        port_keyboard_draw_title(&editor, indexed, FRAME_W, "STREET ROD KEYBOARD");
        colors = port_keyboard_palette();
        count = 4;
    } else {
        if (!dirty && !redraw)
            return false;
        for (unsigned pixel = 0; pixel < sizeof(indexed); ++pixel) {
#ifdef STREET_ROD_2
            /* SR2's built-in text screens use two source columns per PSP pixel. */
            u32 rgb = frame_width == SOURCE_W ? frame[2 * pixel] | frame[2 * pixel + 1] : frame[pixel];
#else
            u32 rgb = frame[pixel];
#endif
            indexed[pixel] = (u8)((rgb >> 18 & 0x30) | (rgb >> 12 & 0x0c) | (rgb >> 6 & 3));
        }
    }
    redraw = false;
    return port_platform_present_indexed(indexed, FRAME_W, FRAME_H, FRAME_W, colors, count);
}

void host_pump(void)
{
    poll_input();
    if (suspended) {
        port_platform_sleep_us(10000);
        return;
    }
    uint64_t now = now_ns();
    unsigned budget = PIT_HZ / 2 / divisor + 1;
    while (tick_due(ticks + 1) <= now && budget) {
        run_tick();
        --budget;
    }
    if (!budget)
        clock_start += now - tick_due(ticks);
    if (now - last_present >= 8000000 && present_frame())
        last_present = now_ns();
    else
        port_platform_sleep_us(1000);
}

static uint64_t retrace_phase(void)
{
    return now_ns() % UINT64_C(14266667);
}

bool host_in_vretrace(void) { return retrace_phase() < 1500000; }

void host_wait_vretrace(void)
{
    uint64_t deadline = now_ns() + UINT64_C(14266667) - retrace_phase();
    while (now_ns() < deadline)
        host_pump();
}

void host_mouse_read(s16 *x, s16 *y, u8 *buttons)
{
    if (x) *x = 0;
    if (y) *y = 0;
    if (buttons) *buttons = 0;
}

void host_mouse_motion(s16 *dx, s16 *dy) { host_mouse_read(dx, dy, NULL); }
void host_mouse_mickeys(s16 *dx, s16 *dy) { host_mouse_motion(dx, dy); }
void host_mouse_set_relative(bool on) { (void)on; }
void host_set_pointer_query(bool (*query)(s16 *, s16 *)) { (void)query; }

bool host_joy_read(s16 *x, s16 *y, u8 *buttons)
{
    (void)x;
    (void)y;
    (void)buttons;
    return false;
}

bool sr_writable_file(const char *name)
{
    if (!strcasecmp(name, "hall_dat"))
        return true;
    if (strncasecmp(name, "HOTROD", 6))
        return false;
    const char *slot = name + 6;
    size_t length = strspn(slot, "0123456789");
#ifdef STREET_ROD_2
    return length && length <= 2 && !strcasecmp(slot + length, ".SAV") && atoi(slot) <= 14;
#else
    return length && length <= 2 && !strcasecmp(slot + length, ".SAV") &&
           atoi(slot) >= 1 && atoi(slot) <= 15;
#endif
}

char *host_game_path(const char *name, bool create)
{
    if (!name || !*name || strpbrk(name, "/\\:") || !strcmp(name, ".") || !strcmp(name, ".."))
        return NULL;
    const bool writable = sr_writable_file(name);
    if (create && !writable)
        return NULL;
    const char *directory = writable ? sr_save_dir : game_dir;
    size_t length = strlen(directory) + strlen(name) + 2;
    char *path = malloc(length);
    if (!path)
        return NULL;
    snprintf(path, length, "%s/%s", directory, name);
    for (char *letter = path + strlen(directory) + 1; *letter; ++letter)
        *letter = (char)toupper((unsigned char)*letter);
    struct stat info;
    if (!create && (stat(path, &info) || !S_ISREG(info.st_mode))) {
        free(path);
        return NULL;
    }
    return path;
}

void host_free(void *value) { free(value); }

void *sr_load_file(const char *path, size_t *size)
{
    FILE *stream = fopen(path, "rb");
    if (!stream)
        return NULL;
    void *data = NULL;
    if (fseek(stream, 0, SEEK_END) == 0) {
        long length = ftell(stream);
        if (length <= 0 || length > 1024 * 1024)
            errno = EINVAL;
        if (length > 0 && length <= 1024 * 1024 && fseek(stream, 0, SEEK_SET) == 0) {
            data = malloc((size_t)length);
            if (data && fread(data, 1, (size_t)length, stream) != (size_t)length) {
                free(data);
                data = NULL;
                errno = EIO;
            }
            if (data)
                *size = (size_t)length;
        }
    }
    int error = errno;
    fclose(stream);
    if (!data)
        errno = error;
    return data;
}

_Noreturn void host_fatal(const char *format, ...)
{
    char message[256];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    fprintf(stderr, SR_GAME_TITLE ": %s\n", message);
    if (initialized) {
        memset(indexed, 0, sizeof(indexed));
        port_keyboard_draw_text(indexed, FRAME_W, 12, 48, message, 3);
        port_keyboard_draw_text(indexed, FRAME_W, 12, 72, "CIRCLE BACK", 3);
        port_platform_present_indexed(indexed, FRAME_W, FRAME_H, FRAME_W, port_keyboard_palette(), 4);
        PortInput input;
        while (port_platform_poll_input(&input) && !input.quit && !(input.pressed & PORT_ALT))
            port_platform_sleep_us(10000);
    }
    exit(3);
}
