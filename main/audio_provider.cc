#include "audio_provider.h"
#include <cstdlib>
#include <cstring>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/i2s_std.h" // NEW API
#include "ringbuf.h"
#include "micro_model_settings.h"

static const char* TAG = "TF_LITE_AUDIO_PROVIDER";

ringbuf_t* g_audio_capture_buffer;
volatile int32_t g_latest_audio_timestamp = 0;

constexpr int32_t history_samples_to_keep = ((kFeatureDurationMs - kFeatureStrideMs) * (kAudioSampleFrequency / 1000));
constexpr int32_t new_samples_to_get = (kFeatureStrideMs * (kAudioSampleFrequency / 1000));

const int32_t kAudioCaptureBufferSize = 40000;
const int32_t i2s_bytes_to_read = 3200;

namespace {
int16_t g_audio_output_buffer[kMaxAudioSampleSize * 32];
bool g_is_audio_initialized = false;
int16_t g_history_buffer[history_samples_to_keep];
uint8_t g_i2s_read_buffer[i2s_bytes_to_read] = {};

i2s_chan_handle_t rx_handle = NULL;
}
static void i2s_init(void) {
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, NULL, &rx_handle));

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(16000),
        .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = GPIO_NUM_41,
            .ws   = GPIO_NUM_42,
            .dout = I2S_GPIO_UNUSED,
            .din  = GPIO_NUM_2,
            .invert_flags = { .mclk_inv = false, .bclk_inv = false, .ws_inv = false },
        },
    };
    
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_handle, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(rx_handle));
}

static void CaptureSamples(void* arg) {
    i2s_init();
    while (1) {
        size_t bytes_read = 0;
        // In the new API, i2s_channel_read is used
        esp_err_t res = i2s_channel_read(rx_handle, g_i2s_read_buffer, i2s_bytes_to_read, &bytes_read, pdMS_TO_TICKS(100));
        
        if (res == ESP_OK && bytes_read > 0) {
            // Rescale 32-bit to 16-bit
            int16_t* p_out = (int16_t*)g_i2s_read_buffer;
            int32_t* p_in = (int32_t*)g_i2s_read_buffer;
            for (int i = 0; i < bytes_read / 4; ++i) {
                p_out[i] = (int16_t)(p_in[i] >> 14);
            }
            size_t valid_bytes = bytes_read / 2;

            int bytes_written = rb_write(g_audio_capture_buffer, (uint8_t*)g_i2s_read_buffer, valid_bytes, pdMS_TO_TICKS(100));
            g_latest_audio_timestamp += ((1000 * (bytes_written / 2)) / kAudioSampleFrequency);
        }
    }
}

TfLiteStatus InitAudioRecording() {
  g_audio_capture_buffer = rb_init("tf_ringbuffer", kAudioCaptureBufferSize);
  if (!g_audio_capture_buffer) {
    ESP_LOGE(TAG, "Error creating ring buffer");
    return kTfLiteError;
  }
  /* create CaptureSamples Task which will get the i2s_data from mic and fill it
   * in the ring buffer */
  xTaskCreate(CaptureSamples, "CaptureSamples", 1024 * 4, NULL, 10, NULL);
  while (!g_latest_audio_timestamp) {
    vTaskDelay(1); // one tick delay to avoid watchdog
  }
  ESP_LOGI(TAG, "Audio Recording started");
  return kTfLiteOk;
}

TfLiteStatus GetAudioSamples1(int* audio_samples_size, int16_t** audio_samples)
{
  if (!g_is_audio_initialized) {
    TfLiteStatus init_status = InitAudioRecording();
    if (init_status != kTfLiteOk) {
      return init_status;
    }
    g_is_audio_initialized = true;
  }
  int bytes_read =
    rb_read(g_audio_capture_buffer, (uint8_t*)(g_audio_output_buffer), 16000, 1000);
  if (bytes_read < 0) {
    ESP_LOGI(TAG, "Couldn't read data in time");
    bytes_read = 0;
  }
  *audio_samples_size = bytes_read;
  *audio_samples = g_audio_output_buffer;
  return kTfLiteOk;
}

TfLiteStatus GetAudioSamples(int start_ms, int duration_ms,
                             int* audio_samples_size, int16_t** audio_samples) {
  if (!g_is_audio_initialized) {
    TfLiteStatus init_status = InitAudioRecording();
    if (init_status != kTfLiteOk) {
      return init_status;
    }
    g_is_audio_initialized = true;
  }
  /* copy 160 samples (320 bytes) into output_buff from history */
  memcpy((void*)(g_audio_output_buffer), (void*)(g_history_buffer),
         history_samples_to_keep * sizeof(int16_t));

  /* copy 320 samples (640 bytes) from rb at ( int16_t*(g_audio_output_buffer) +
   * 160 ), first 160 samples (320 bytes) will be from history */
  int bytes_read =
      rb_read(g_audio_capture_buffer,
              ((uint8_t*)(g_audio_output_buffer + history_samples_to_keep)),
              new_samples_to_get * sizeof(int16_t), pdMS_TO_TICKS(200));
  if (bytes_read < 0) {
    ESP_LOGE(TAG, " Model Could not read data from Ring Buffer");
  } else if (bytes_read < new_samples_to_get * sizeof(int16_t)) {
    ESP_LOGD(TAG, "RB FILLED RIGHT NOW IS %d",
             rb_filled(g_audio_capture_buffer));
    ESP_LOGD(TAG, " Partial Read of Data by Model ");
    ESP_LOGV(TAG, " Could only read %d bytes when required %d bytes ",
             bytes_read, (int) (new_samples_to_get * sizeof(int16_t)));
  }

  /* copy 320 bytes from output_buff into history */
  memcpy((void*)(g_history_buffer),
         (void*)(g_audio_output_buffer + new_samples_to_get),
         history_samples_to_keep * sizeof(int16_t));

  *audio_samples_size = kMaxAudioSampleSize;
  *audio_samples = g_audio_output_buffer;
  return kTfLiteOk;
}

int32_t LatestAudioTimestamp() { return g_latest_audio_timestamp; }
