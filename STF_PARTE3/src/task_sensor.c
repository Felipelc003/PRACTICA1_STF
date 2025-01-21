#include "therm.h"
#include "config.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/ringbuf.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <string.h>

static const char* TAG = "STF_P1:task_sensor";
static SemaphoreHandle_t semSample = NULL;

static void tmrSampleCallback(void* arg) {
    xSemaphoreGive(semSample);
}

SYSTEM_TASK(TASK_SENSOR) {
    TASK_BEGIN();
    ESP_LOGI(TAG, "Task Sensor running");

    task_sensor_args_t* ptr_args = (task_sensor_args_t*)TASK_ARGS;
    RingbufHandle_t* rbuf = ptr_args->rbuf;  
    uint8_t frequency = ptr_args->freq;     
    uint64_t period_us = 1000000 / frequency;

    adc_oneshot_unit_handle_t adc_hdlr;
    adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id = THERMISTOR_ADC_UNIT,
        .clk_src = ADC_RTC_CLK_SRC_DEFAULT,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit_cfg, &adc_hdlr));

    // Configurar ambos termistores
    therm_t thermistor1, thermistor2, thermistor3;
    ESP_ERROR_CHECK(therm_config(&thermistor1, adc_hdlr, THERMISTOR_ADC_CHANNEL));
    ESP_ERROR_CHECK(therm_config(&thermistor2, adc_hdlr, SECOND_THERMISTOR_ADC_CHANNEL));
    ESP_ERROR_CHECK(therm_config(&thermistor3, adc_hdlr, ADC_CHANNEL_4)); // G32 corresponde a ADC_CHANNERL_4 en ESP32

    semSample = xSemaphoreCreateBinary();

    const esp_timer_create_args_t tmrSampleArgs = {
        .callback = &tmrSampleCallback,
        .arg = NULL,
        .name = "SensorTimer",
    };
    esp_timer_handle_t tmrSample;
    ESP_ERROR_CHECK(esp_timer_create(&tmrSampleArgs, &tmrSample));
    ESP_ERROR_CHECK(esp_timer_start_periodic(tmrSample, period_us));

    void* ptr;
    uint16_t temperatures[3];

    TASK_LOOP() {
        if (xSemaphoreTake(semSample, pdMS_TO_TICKS(1000 / frequency))) {
            // Leer valores crudos de los termistores
            temperatures[0] = therm_read_lsb(thermistor1);
            temperatures[1] = therm_read_lsb(thermistor2);
            temperatures[2] = therm_read_lsb(thermistor3);


            if (xRingbufferSendAcquire(*rbuf, &ptr, sizeof(temperatures), pdMS_TO_TICKS(100)) != pdTRUE) {
                ESP_LOGW(TAG, "Buffer lleno. No se pueden enviar las muestras.");
            } else {
                memcpy(ptr, temperatures, sizeof(temperatures));
                xRingbufferSendComplete(*rbuf, ptr);
            }
        } else {
            ESP_LOGE(TAG, "Timeout al esperar el semáforo.");
        }
    }

    ESP_ERROR_CHECK(esp_timer_stop(tmrSample));
    ESP_ERROR_CHECK(esp_timer_delete(tmrSample));
    vSemaphoreDelete(semSample);
    TASK_END();
}