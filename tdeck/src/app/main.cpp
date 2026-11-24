// Setup Rush as the whole T-Deck firmware: it powers on into the game's intro, then the menu, then a round.
// Nothing else runs: no LVGL, no radio (the SX1262 is held in reset), no mesh. Only the game (src/setup_rush),
// the screen, touch, trackball, keyboard, speaker, and the GPS for the date.
// Build: platformio run -e setup-rush. The lilyshark firmware is env:t-deck.
#include <Arduino.h>
#include <Preferences.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <Wire.h>
#include <driver/i2s.h>
#include <esp_heap_caps.h>

#include <cstring>
#include <ctime>

#include "lilyshark/device/hardware_status.h"
#include "lilyshark/device/tdeck_display_init.h"
#include "lilyshark/device/touch.h"
#include "lilyshark/tdeck.h"
#include "setup_rush/srush_platform.h"

namespace tdeck = lilyshark::tdeck;

namespace {

extern "C" const uint8_t srush_pack_start[] asm("_binary_data_setup_rush_srpack_bin_start");
extern "C" const uint8_t srush_pack_end[] asm("_binary_data_setup_rush_srpack_bin_end");

TFT_eSPI display;
lilyshark::TDeckTouch touch;
lilyshark::TDeckHardwareStatus gps_clock{};
bool keyboard_ready = false;

uint16_t *fb = nullptr;
uint16_t *shown = nullptr;     // what the panel is showing, to send only what changed
bool shown_valid = false;
bool dump_frame = false;       // USB test hook: byte 0x02 asks for the next frame
uint32_t frames = 0;
bool audio_on = false;
bool touch_was = false;

portMUX_TYPE trackball_mux = portMUX_INITIALIZER_UNLOCKED;
volatile uint32_t trackball_pending = 0;
void IRAM_ATTR latch_edge(uint32_t bit)
{
    portENTER_CRITICAL_ISR(&trackball_mux);
    trackball_pending |= bit;
    portEXIT_CRITICAL_ISR(&trackball_mux);
}
void IRAM_ATTR on_up() { latch_edge(1U << 0U); }
void IRAM_ATTR on_down() { latch_edge(1U << 1U); }
void IRAM_ATTR on_left() { latch_edge(1U << 2U); }
void IRAM_ATTR on_right() { latch_edge(1U << 3U); }

// The backlight steps down one of 16 levels per low pulse; full is a plain high.
void backlight_full() { digitalWrite(tdeck::backlight_pin, HIGH); }

// ---- sound: the synth (srush_audio.c) is pulled a block at a time into whatever room the I2S buffer has, without
// waiting, so a slow frame costs a gap in the sound and never a stall in the round.
void audio_begin()
{
    i2s_config_t config{};
    config.mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_TX);
    config.sample_rate = SR_AUDIO_RATE;
    config.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
    config.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;
    config.communication_format = I2S_COMM_FORMAT_STAND_I2S;
    config.dma_buf_count = 8;
    config.dma_buf_len = 256;
    config.tx_desc_auto_clear = true;
    if(i2s_driver_install(I2S_NUM_0, &config, 0, nullptr) != ESP_OK) return;
    i2s_pin_config_t pins{};
    pins.mck_io_num = I2S_PIN_NO_CHANGE;
    pins.bck_io_num = tdeck::i2s_bck_pin;
    pins.ws_io_num = tdeck::i2s_ws_pin;
    pins.data_out_num = tdeck::i2s_dout_pin;
    pins.data_in_num = I2S_PIN_NO_CHANGE;
    if(i2s_set_pin(I2S_NUM_0, &pins) != ESP_OK) {
        (void)i2s_driver_uninstall(I2S_NUM_0);
        return;
    }
    audio_on = true;
}

void audio_pump()
{
    if(!audio_on) return;
    static int16_t block[256];
    static size_t offset = 256;
    for(int rounds = 0; rounds < 12; ++rounds) {
        if(offset == 256U) {
            sr_audio_render(block, 256);
            offset = 0;
        }
        size_t written = 0;
        const size_t want = (256U - offset) * sizeof(int16_t);
        if(i2s_write(I2S_NUM_0, block + offset, want, &written, 0) != ESP_OK) return;
        offset += written / sizeof(int16_t);
        if(written < want) return;  // the buffer is full
    }
}

// ---- the screen: only what changed goes to the panel. A whole frame is 31 ms of SPI at 40 MHz; a round changes
// about a tenth of the pixels per frame, so each row sends just its changed span, and runs of rows whose spans nearly
// coincide share one window.
void present(const uint16_t *frame)
{
    audio_pump();
    display.startWrite();
    if(shown == nullptr || !shown_valid) {
        display.setAddrWindow(0, 0, 320, 240);
        display.pushColors(const_cast<uint16_t *>(frame), 320U * 240U, true);
        if(shown != nullptr) {
            std::memcpy(shown, frame, 320U * 240U * 2U);
            shown_valid = true;
        }
    } else {
        int run_y = -1, run_l = 0, run_r = 0, run_rows = 0;
        long run_changed = 0;
        const auto flush_run = [&]() {
            if(run_y < 0) return;
            const int width = run_r - run_l + 1;
            display.setAddrWindow(run_l, run_y, width, run_rows);
            for(int row = 0; row < run_rows; ++row) {
                display.pushColors(const_cast<uint16_t *>(frame) + (run_y + row) * 320 + run_l,
                                   static_cast<uint32_t>(width), true);
            }
            run_y = -1;
        };
        for(int y = 0; y < 240; ++y) {
            const uint16_t *now_row = frame + y * 320;
            uint16_t *old_row = shown + y * 320;
            int l = 0;
            while(l < 320 && now_row[l] == old_row[l]) ++l;
            if(l == 320) {
                flush_run();
                continue;
            }
            int r = 319;
            while(now_row[r] == old_row[r]) --r;
            std::memcpy(old_row + l, now_row + l, static_cast<size_t>(r - l + 1) * 2U);
            if(run_y >= 0) {
                const int nl = l < run_l ? l : run_l, nr = r > run_r ? r : run_r;
                const long area = static_cast<long>(nr - nl + 1) * (run_rows + 1);
                // Widening the window resends pixels that did not change; allow it while that stays under 48 pixels
                // a row, which is about what a new window's commands cost.
                if(area - (run_changed + (r - l + 1)) <= 48L * (run_rows + 1)) {
                    run_l = nl; run_r = nr; ++run_rows; run_changed += r - l + 1;
                    continue;
                }
                flush_run();
            }
            run_y = y; run_l = l; run_r = r; run_rows = 1; run_changed = r - l + 1;
        }
        flush_run();
    }
    display.endWrite();
    audio_pump();
    ++frames;
    if(dump_frame) {
        // The frame as drawn, for checking the game from a computer: "SRFB", millis, the frame count, then 320x240
        // RGB565 little-endian.
        dump_frame = false;
        const uint32_t header[2] = {millis(), frames};
        Serial.write(reinterpret_cast<const uint8_t *>("SRFB"), 4);
        Serial.write(reinterpret_cast<const uint8_t *>(header), sizeof(header));
        const uint8_t *bytes = reinterpret_cast<const uint8_t *>(frame);
        for(size_t sent = 0; sent < 320U * 240U * 2U; sent += 1024U) {
            Serial.write(bytes + sent, 1024U);
            audio_pump();
        }
        Serial.flush();
    }
    delay(0);  // let the idle task and the USB stack breathe between frames
}

// ---- the keyboard. LilyGo's keyboard firmware (since June 2025) has a raw mode (command 0x03) whose reads are the
// whole matrix, 5 column bytes with a bit per held row key; older firmware ignores the command and keeps sending one
// character per press. Raw mode is trusted only after one held key reads the same in three reads in a row, which a
// character stream never does; a byte with its top bit set (no matrix has an 8th row) rules it out for good.
enum class RawState : uint8_t { Unknown, Yes, No };
RawState raw_state = RawState::Unknown;
uint8_t raw_last[5] = {0}, raw_same = 0, raw_held = 0;
// matrix position (column, row) of each key the game reads, from the keyboard firmware's keymap
struct RawKey { uint8_t col, row; char c; };
constexpr RawKey kRawKeys[] = {{0, 0, 'q'}, {0, 1, 'w'}, {0, 3, 'a'}, {0, 5, ' '}, {1, 1, 's'}, {1, 2, 'd'},
                               {3, 1, 'h'}, {3, 6, 'j'}, {4, 6, 'k'}, {4, 1, 'l'}, {3, 3, '\r'}, {4, 3, 8}};

void read_keyboard(SrRaw *r)
{
    if(raw_state == RawState::No) {
        if(Wire.requestFrom(tdeck::keyboard_address, static_cast<uint8_t>(1)) == 1 && Wire.available()) {
            const uint8_t key = Wire.read();
            if(key != 0 && r->nkeys < sizeof(r->keys)) r->keys[r->nkeys++] = static_cast<char>(key);
        }
        return;
    }
    uint8_t b[5] = {0};
    const uint8_t n = Wire.requestFrom(tdeck::keyboard_address, static_cast<uint8_t>(5));
    for(uint8_t i = 0; i < n && i < 5 && Wire.available(); ++i) b[i] = Wire.read();
    while(Wire.available()) (void)Wire.read();
    if(raw_state == RawState::Unknown) {
        bool top = false;
        for(uint8_t i = 1; i < 5; ++i) top |= (b[i] & 0x80U) != 0;
        if(n != 5 || top) {  // a one-character reply padded out: the old firmware
            raw_state = RawState::No;
            if(b[0] != 0 && (b[0] & 0x80U) == 0 && r->nkeys < sizeof(r->keys)) r->keys[r->nkeys++] = static_cast<char>(b[0]);
            return;
        }
        bool any = false;
        for(uint8_t i = 0; i < 5; ++i) any |= b[i] != 0;
        if(any && std::memcmp(b, raw_last, 5) == 0) { if(++raw_same >= 2) raw_state = RawState::Yes; }
        else raw_same = 0;
        // until it is known, a lone first byte may still be a character
        const bool lone = !b[1] && !b[2] && !b[3] && !b[4];
        const bool echoed = b[1] == b[0] && b[2] == b[0] && b[3] == b[0] && b[4] == b[0];  // padded by repeating it
        const bool chr = (b[0] >= 0x20 && b[0] < 0x7f) || b[0] == '\r' || b[0] == 8;
        if(raw_state == RawState::Unknown && b[0] && chr && (lone || echoed) && std::memcmp(b, raw_last, 5) != 0 &&
           r->nkeys < sizeof(r->keys)) r->keys[r->nkeys++] = static_cast<char>(b[0]);
        std::memcpy(raw_last, b, 5);
        if(raw_state != RawState::Yes) return;
    }
    // raw: a key going down is a press (menus step on it); W, SPACE and S are also held
    static uint8_t prev[5] = {0};
    uint8_t held = 0;
    for(const RawKey &k : kRawKeys) {
        const bool down = (b[k.col] >> k.row) & 1U, was = (prev[k.col] >> k.row) & 1U;
        if(down && !was && r->nkeys < sizeof(r->keys)) r->keys[r->nkeys++] = k.c;
        if(down && (k.c == 'w' || k.c == ' ')) held |= 1U;
        if(down && k.c == 's') held |= 2U;
    }
    std::memcpy(prev, b, 5);
    raw_held = held;
}

// ---- input, once per frame
void raw(SrRaw *r)
{
    portENTER_CRITICAL(&trackball_mux);
    const uint32_t pending = trackball_pending;
    trackball_pending = 0;
    portEXIT_CRITICAL(&trackball_mux);
    for(int d = 0; d < 4; ++d) r->roll[d] = (pending & (1U << d)) ? 1U : 0U;
    r->click = digitalRead(tdeck::trackball_press_pin) == LOW;

    if(touch.present() && touch.poll()) {
        const lilyshark::TouchPoint point = touch.point();
        touch_was = point.pressed;
        if(point.pressed) { r->tx = static_cast<int16_t>(point.x); r->ty = static_cast<int16_t>(point.y); }
    } else {
        touch_was = false;
    }
    r->touch = touch_was;

    // USB test hook: characters over serial are keys (w s a d, Enter, Backspace, space); 0x02 dumps a frame.
    while(Serial.available() > 0 && r->nkeys < sizeof(r->keys)) {
        const int c = Serial.read();
        if(c == 0x02) dump_frame = true;
        else if(c > 0) r->keys[r->nkeys++] = static_cast<char>(c);
    }

    static uint32_t next_key_ms = 0;
    const uint32_t now = millis();
    if(keyboard_ready && static_cast<int32_t>(now - next_key_ms) >= 0) {
        next_key_ms = now + 15U;
        read_keyboard(r);
    }
    if(raw_state == RawState::Yes) r->held = static_cast<uint8_t>(0x80U | raw_held);

    // the GPS sets the clock (the daily chart's day); bounded work per call
    static uint32_t next_gps_ms = 0;
    if(static_cast<int32_t>(now - next_gps_ms) >= 0) {
        next_gps_ms = now + 100U;
        gps_clock.poll();
    }
}

uint32_t ms() { return millis(); }
uint32_t unix_time()
{
    const time_t now = time(nullptr);
    return now > 1700000000 ? static_cast<uint32_t>(now) : 0U;  // set only once the GPS has set the clock
}

bool save_load(uint8_t *buf, size_t cap, size_t *len)
{
    Preferences prefs;
    if(!prefs.begin("srush", true)) return false;
    const size_t n = prefs.getBytesLength("save");
    *len = n <= cap ? prefs.getBytes("save", buf, n) : 0U;
    prefs.end();
    return *len != 0U;
}

bool save_store(const uint8_t *buf, size_t len)
{
    Preferences prefs;
    if(!prefs.begin("srush", false)) return false;
    const bool ok = prefs.putBytes("save", buf, len) == len;
    prefs.end();
    return ok;
}

} // namespace

void setup()
{
    Serial.begin(115200);
    pinMode(tdeck::power_enable_pin, OUTPUT);
    digitalWrite(tdeck::power_enable_pin, HIGH);
    // every SPI device deselected; the radio held in reset (unused, and quiet)
    pinMode(tdeck::sd_cs_pin, OUTPUT);
    pinMode(tdeck::radio_cs_pin, OUTPUT);
    pinMode(tdeck::display_cs_pin, OUTPUT);
    pinMode(tdeck::radio_reset_pin, OUTPUT);
    pinMode(tdeck::backlight_pin, OUTPUT);
    digitalWrite(tdeck::sd_cs_pin, HIGH);
    digitalWrite(tdeck::radio_cs_pin, HIGH);
    digitalWrite(tdeck::display_cs_pin, HIGH);
    digitalWrite(tdeck::radio_reset_pin, LOW);
    digitalWrite(tdeck::backlight_pin, LOW);

    SPI.begin(tdeck::spi_sck_pin, tdeck::spi_miso_pin, tdeck::spi_mosi_pin);
    display.begin();
    lilyshark::applyTDeckDisplayInit(display);
    display.setRotation(1);
    display.fillScreen(TFT_BLACK);

    // the keyboard controller and touch share I2C; give the keyboard a moment after power-up
    delay(200);
    Wire.begin(tdeck::i2c_sda_pin, tdeck::i2c_scl_pin, tdeck::i2c_frequency_hz);
    Wire.beginTransmission(tdeck::keyboard_address);
    Wire.write(0x01);  // keyboard backlight level (the firmware's 0x01 falls through and reads a second byte)
    Wire.write(127);
    Wire.write(127);
    keyboard_ready = Wire.endTransmission() == 0U;
    if(keyboard_ready) {  // ask for the raw matrix, so keys can be held (older keyboard firmware ignores this)
        Wire.beginTransmission(tdeck::keyboard_address);
        Wire.write(0x03);
        (void)Wire.endTransmission();
    }
    (void)touch.begin(Wire);

    for(uint8_t pin : {tdeck::trackball_up_pin, tdeck::trackball_down_pin, tdeck::trackball_left_pin,
                       tdeck::trackball_right_pin, tdeck::trackball_press_pin}) {
        pinMode(pin, INPUT_PULLUP);
    }
    attachInterrupt(tdeck::trackball_up_pin, on_up, FALLING);
    attachInterrupt(tdeck::trackball_down_pin, on_down, FALLING);
    attachInterrupt(tdeck::trackball_left_pin, on_left, FALLING);
    attachInterrupt(tdeck::trackball_right_pin, on_right, FALLING);

    gps_clock.begin(true);

    // The game redraws every pixel each frame: one buffer in internal RAM (faster to draw), the panel's copy in PSRAM.
    fb = static_cast<uint16_t *>(heap_caps_malloc(320U * 240U * 2U, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    if(fb == nullptr) fb = static_cast<uint16_t *>(heap_caps_malloc(320U * 240U * 2U, MALLOC_CAP_SPIRAM));
    shown = static_cast<uint16_t *>(heap_caps_malloc(320U * 240U * 2U, MALLOC_CAP_SPIRAM));

    sr = SrPlatform{};
    sr.fb = fb;
    sr.present = present;
    sr.raw = raw;
    sr.millis = ms;
    sr.unix_time = unix_time;
    sr.pack = srush_pack_start;
    sr.pack_len = static_cast<size_t>(srush_pack_end - srush_pack_start);
    sr.save_load = save_load;
    sr.save_store = save_store;
    audio_begin();
    Serial.println("Setup Rush ready");
    backlight_full();
}

void loop()
{
    if(fb == nullptr) {
        delay(1000);
        return;
    }
    // The game runs until DEL on its intro; then it starts over at the intro.
    shown_valid = false;
    (void)srush_main();
}
