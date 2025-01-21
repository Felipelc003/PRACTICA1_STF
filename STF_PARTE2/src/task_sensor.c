// /************************
// * FILENAME : task_sensor.c       
// *
// * DESCRIPTION : 
// *
// * PUBLIC LICENSE :
// * Este código es de uso público y libre de modificar bajo los términos de la
// * Licencia Pública General GNU (GPL v3) o posterior. Se proporciona "tal cual",
// * sin garantías de ningún tipo.
// *
// * AUTHOR :   Dr. Fernando Leon (fernando.leon@uco.es) University of Cordoba
// **************************/


// /*
// 	Circuito del termistor 1. 

//    3.3V
//      |
//      |
//   [ NTC ]  <-- Termistor 10K 
//      |
//      |-----------> ADC IN (GPIO34 por defecto. Ver config.h)
//      |
//   [ 10K ]  <-- R fija 10K
//      |
//     GND
// **/

#include "therm.h"
#include "config.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/ringbuf.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <string.h>

// Identificador para logging
static const char* TAG = "STF_P1:task_sensor";

// Semáforo global para el control del temporizador
static SemaphoreHandle_t semSample = NULL;

// Callback del temporizador: Despierta la tarea al liberar el semáforo
static void tmrSampleCallback(void* arg) {
    xSemaphoreGive(semSample);
}

// Implementación de la tarea Sensor
SYSTEM_TASK(TASK_SENSOR) {
    TASK_BEGIN();
    ESP_LOGI(TAG, "Task Sensor running");

    // Argumentos de la tarea
    task_sensor_args_t* args = (task_sensor_args_t*)TASK_ARGS;
    RingbufHandle_t* monitor_rbuf = args->rbuf;       // Buffer hacia Monitor
    RingbufHandle_t* checker_rbuf = args->checker_rbuf; // Buffer hacia Comprobador
    uint8_t frequency = args->freq;                  // Frecuencia de muestreo (Hz)
    uint8_t N = args->N;                             // Número de muestras a promediar
    uint64_t period_us = 1000000 / frequency;        // Período en microsegundos

    // Inicializar el manejador ADC
    adc_oneshot_unit_handle_t adc_hdlr;
    adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id = ADC_UNIT_1,
        .clk_src = ADC_RTC_CLK_SRC_DEFAULT,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit_cfg, &adc_hdlr));

    // Configurar los termistores
    therm_t thermistor1, thermistor2;
    
    //ESP_ERROR_CHECK(therm_config(&thermistor1, adc_hdlr, THERMISTOR_ADC_CHANNEL));
    esp_err_t ret = therm_config(&thermistor1, adc_hdlr, THERMISTOR_ADC_CHANNEL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error configurando termistor 1: %s", esp_err_to_name(ret));
        return; // Manejo de error sin reinicio
    }
    
    //ESP_ERROR_CHECK(therm_config(&thermistor2, adc_hdlr, SECOND_THERMISTOR_ADC_CHANNEL));
    ret = therm_config(&thermistor2, adc_hdlr, SECOND_THERMISTOR_ADC_CHANNEL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error configurando termistor 2: %s", esp_err_to_name(ret));
        return; // Manejo de error sin reinicio
    }

    // Configuración del temporizador
    semSample = xSemaphoreCreateBinary();
    const esp_timer_create_args_t tmrSampleArgs = {
        .callback = &tmrSampleCallback,
        .arg = NULL,
        .name = "SensorTimer",
    };
    esp_timer_handle_t tmrSample;
    ESP_ERROR_CHECK(esp_timer_create(&tmrSampleArgs, &tmrSample));
    ESP_ERROR_CHECK(esp_timer_start_periodic(tmrSample, period_us));

    // Estructura para enviar mensajes
    uint8_t i = 0;
    void* ptr_comprobador;
    float temperature1, temperature2;

    // Bucle principal
    TASK_LOOP() {
        // Espera al semáforo del temporizador
        if (xSemaphoreTake(semSample, pdMS_TO_TICKS(1000 / frequency))) {
            // Leer temperatura T1 (termistor 1)
            temperature1 = therm_read_t(thermistor1);

            
            // Enviar T1 al Monitor
            message_monitor_t message = { .id_source = 1, .data = temperature1 };
            xRingbufferSend(*monitor_rbuf, &message, sizeof(message_monitor_t), pdMS_TO_TICKS(100));

            i++;

            if (i % N == 0) {
                
                // Leer temperatura T2 (termistor 2)
                temperature2 = therm_read_t(thermistor2);

                // Enviar T1 y T2 al Comprobador
                
                if (xRingbufferSendAcquire(*checker_rbuf, &ptr_comprobador, sizeof(float) * 2, pdMS_TO_TICKS(100)) == pdTRUE) {
                    memcpy(ptr_comprobador, &temperature1, sizeof(float));
                    memcpy((uint8_t*)ptr_comprobador + sizeof(float), &temperature2, sizeof(float));
                    xRingbufferSendComplete(*checker_rbuf, ptr_comprobador);

                } else {
                    ESP_LOGW(TAG, "Buffer Comprobador lleno. No se puede enviar las muestras.");
                }
            }
        } else {
            ESP_LOGE(TAG, "Timeout al esperar el semáforo.");
        }
    }

    // Limpieza al salir del bucle
    ESP_ERROR_CHECK(esp_timer_stop(tmrSample));
    ESP_ERROR_CHECK(esp_timer_delete(tmrSample));
    vSemaphoreDelete(semSample);
    TASK_END();
}
