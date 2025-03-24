#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/i2s.h"

static const char *TAG = "PDM_RECORDER";

// Pin assignments
#define GPIO_PDM_CLK   26  // PDM CLK output
#define GPIO_PDM_DATA  34  // PDM DATA input

// Recording config
#define SAMPLE_RATE       16000
#define RECORD_SECONDS    5
#define TOTAL_SAMPLES     (SAMPLE_RATE * RECORD_SECONDS)

// Buffer for recorded samples (16 bits each)
static int16_t s_audio_buffer[TOTAL_SAMPLES] = {0};

void app_main(void)
{
    ESP_LOGI(TAG, "Starting PDM microphone recording example...");

    // 1. Configure I2S for PDM mode
    i2s_config_t i2s_config = {
        .mode = I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_PDM,
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,  // or ONLY_RIGHT if needed
        .communication_format = I2S_COMM_FORMAT_I2S_MSB,
        .intr_alloc_flags = 0,    // default interrupt priority
        .dma_buf_count = 4,
        .dma_buf_len = 1024,
        .use_apll = false,
        .tx_desc_auto_clear = false,
        .fixed_mclk = 0
    };

    i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_PIN_NO_CHANGE,     // not used in PDM mode
        .ws_io_num = GPIO_PDM_CLK,           // PDM clock
        .data_out_num = I2S_PIN_NO_CHANGE,   // not used for recording
        .data_in_num = GPIO_PDM_DATA         // PDM data in
    };

    // 2. Install I2S driver
    ESP_ERROR_CHECK(i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL));
    ESP_ERROR_CHECK(
        i2s_set_pin(I2S_NUM_0, &pin_config)
    );

    // 3. Explicitly configure the clock
    ESP_ERROR_CHECK(
        i2s_set_clk(I2S_NUM_0, SAMPLE_RATE, I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_MONO)
    );

    ESP_LOGI(TAG, "Recording for %d seconds...", RECORD_SECONDS);

    size_t bytes_read = 0;
    size_t total_bytes_read = 0;
    size_t bytes_to_read = TOTAL_SAMPLES * sizeof(int16_t);

    // We'll read into a uint8_t pointer, which points to our int16_t buffer
    uint8_t *buf_ptr = (uint8_t *)s_audio_buffer;

    // 4. Block until we fill up our entire 5-second buffer
    while (total_bytes_read < bytes_to_read) {
        i2s_read(I2S_NUM_0,
                 buf_ptr + total_bytes_read,
                 bytes_to_read - total_bytes_read,
                 &bytes_read,
                 portMAX_DELAY);
        total_bytes_read += bytes_read;
    }

    ESP_LOGI(TAG, "Done recording. Total bytes read: %d", total_bytes_read);

    // 5. Uninstall I2S to free resources
    i2s_driver_uninstall(I2S_NUM_0);

    // 6. Output recorded data over UART
    ESP_LOGI(TAG, "Sending audio data over UART...");
    for (int i = 0; i < TOTAL_SAMPLES; i++) {
        // Print each 16-bit sample in decimal
        printf("%d\n", s_audio_buffer[i]);
    }

    ESP_LOGI(TAG, "All samples transmitted, done!");

    // 7. Do NOT restart; stay idle (so it won't loop again).
    //    We can just park this task here indefinitely:
    while (true) {
        vTaskDelay(portMAX_DELAY); // Let FreeRTOS idle forever
    }
}