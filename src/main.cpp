#include <Arduino.h>
#include "BluetoothA2DPSink.h"
#include "driver/i2s.h"
#include "esp_gap_bt_api.h"

static const char *bluetooth_name = "сссс";

static const int i2s_bclk_pin = 26;
static const int i2s_lr_pin = 25;
static const int i2s_data_pin = 27;

// Audio/Video minor class:
// 0x05 = loudspeaker, 0x08 = car audio, 0x0A = hi-fi audio.
static const uint8_t bluetooth_av_minor_class = 0x08;

BluetoothA2DPSink a2dp_sink;

static portMUX_TYPE metadata_lock = portMUX_INITIALIZER_UNLOCKED;
static volatile bool metadata_dirty = false;

static char track_title[96] = "";
static char track_artist[96] = "";
static char track_album[96] = "";
static char track_genre[48] = "";
static char track_number[16] = "";
static char track_count[16] = "";
static char track_playing_time_ms[16] = "";

static void copy_cstr(char *dest, size_t dest_size, const char *src)
{
    if (dest == nullptr || dest_size == 0)
    {
        return;
    }

    if (src == nullptr)
    {
        src = "";
    }

    size_t i = 0;
    for (; i + 1 < dest_size && src[i] != '\0'; ++i)
    {
        dest[i] = src[i];
    }
    dest[i] = '\0';
}

static void copy_metadata_text(char *dest, size_t dest_size, const uint8_t *text)
{
    copy_cstr(dest, dest_size, reinterpret_cast<const char *>(text));
}

void avrc_metadata_callback(uint8_t id, const uint8_t *text)
{
    portENTER_CRITICAL(&metadata_lock);

    switch (id)
    {
    case ESP_AVRC_MD_ATTR_TITLE:
        copy_metadata_text(track_title, sizeof(track_title), text);
        break;

    case ESP_AVRC_MD_ATTR_ARTIST:
        copy_metadata_text(track_artist, sizeof(track_artist), text);
        break;

    case ESP_AVRC_MD_ATTR_ALBUM:
        copy_metadata_text(track_album, sizeof(track_album), text);
        break;

    case ESP_AVRC_MD_ATTR_GENRE:
        copy_metadata_text(track_genre, sizeof(track_genre), text);
        break;

    case ESP_AVRC_MD_ATTR_TRACK_NUM:
        copy_metadata_text(track_number, sizeof(track_number), text);
        break;

    case ESP_AVRC_MD_ATTR_NUM_TRACKS:
        copy_metadata_text(track_count, sizeof(track_count), text);
        break;

    case ESP_AVRC_MD_ATTR_PLAYING_TIME:
        copy_metadata_text(track_playing_time_ms, sizeof(track_playing_time_ms), text);
        break;

    default:
        break;
    }

    metadata_dirty = true;
    portEXIT_CRITICAL(&metadata_lock);
}

static void print_metadata_if_changed()
{
    if (!metadata_dirty)
    {
        return;
    }

    char title[sizeof(track_title)];
    char artist[sizeof(track_artist)];
    char album[sizeof(track_album)];
    char genre[sizeof(track_genre)];
    char number[sizeof(track_number)];
    char count[sizeof(track_count)];
    char playing_time_ms[sizeof(track_playing_time_ms)];

    portENTER_CRITICAL(&metadata_lock);
    copy_cstr(title, sizeof(title), track_title);
    copy_cstr(artist, sizeof(artist), track_artist);
    copy_cstr(album, sizeof(album), track_album);
    copy_cstr(genre, sizeof(genre), track_genre);
    copy_cstr(number, sizeof(number), track_number);
    copy_cstr(count, sizeof(count), track_count);
    copy_cstr(playing_time_ms, sizeof(playing_time_ms), track_playing_time_ms);
    metadata_dirty = false;
    portEXIT_CRITICAL(&metadata_lock);

    Serial.println();
    Serial.println("Track metadata:");
    Serial.printf("Title:  %s\n", title);
    Serial.printf("Artist: %s\n", artist);
    Serial.printf("Album:  %s\n", album);
    Serial.printf("Genre:  %s\n", genre);
    Serial.printf("Track:  %s / %s\n", number, count);
    Serial.printf("Time:   %s ms\n", playing_time_ms);
}

static void set_bluetooth_device_class()
{
    esp_bt_cod_t cod = {};
    cod.major = ESP_BT_COD_MAJOR_DEV_AV;
    cod.minor = bluetooth_av_minor_class;
    cod.service = ESP_BT_COD_SRVC_RENDERING | ESP_BT_COD_SRVC_AUDIO;

    esp_err_t err = esp_bt_gap_set_cod(cod, ESP_BT_INIT_COD);
    Serial.printf("Bluetooth class: %s\n", err == ESP_OK ? "audio/video OK" : "set failed");
}

void setup()
{
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println("ESP32 boot OK");
    Serial.println("Configuring I2S pins...");

    i2s_pin_config_t pin_config = {};
    pin_config.bck_io_num = i2s_bclk_pin;
    pin_config.ws_io_num = i2s_lr_pin;
    pin_config.data_out_num = i2s_data_pin;
    pin_config.data_in_num = I2S_PIN_NO_CHANGE;

    a2dp_sink.set_pin_config(pin_config);

    a2dp_sink.set_avrc_metadata_attribute_mask(
        ESP_AVRC_MD_ATTR_TITLE |
        ESP_AVRC_MD_ATTR_ARTIST |
        ESP_AVRC_MD_ATTR_ALBUM |
        ESP_AVRC_MD_ATTR_TRACK_NUM |
        ESP_AVRC_MD_ATTR_NUM_TRACKS |
        ESP_AVRC_MD_ATTR_GENRE |
        ESP_AVRC_MD_ATTR_PLAYING_TIME);
    a2dp_sink.set_avrc_metadata_callback(avrc_metadata_callback);

    Serial.println("Starting Bluetooth A2DP...");
    a2dp_sink.start(bluetooth_name);
    set_bluetooth_device_class();

    Serial.println("Bluetooth started");
    Serial.println("Connect to: ESP32_AUX_PCM5102A");
}

void loop()
{
    print_metadata_if_changed();
    delay(200);
}
