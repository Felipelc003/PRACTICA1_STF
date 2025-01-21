#include "config.h"
#include <freertos/FreeRTOS.h>
#include <freertos/ringbuf.h>
#include <esp_log.h>
#include <string.h>

static const char* TAG = "STF_P1:task_votador";
#define N_PERIODS 10

SYSTEM_TASK(TASK_VOTADOR) {
    TASK_BEGIN();
    ESP_LOGI(TAG, "Task Votador running");

    task_votador_args_t* ptr_args = (task_votador_args_t*)TASK_ARGS;
    RingbufHandle_t* rbuf_sensor = ptr_args->rbuf_sensor;
    RingbufHandle_t* rbuf_monitor = ptr_args->rbuf_monitor;
    uint32_t mask = ptr_args->mask; // Declara y asigna la máscara

    size_t length;
    void *ptr;
    uint16_t temperatures[3];
    uint16_t result;
    int period_count = 0;

    TASK_LOOP() {
        // Bloquea en espera de datos del RingBuffer de sensores
        ptr = xRingbufferReceive(*rbuf_sensor, &length, pdMS_TO_TICKS(1000));

        if (ptr != NULL) {
            if (length == sizeof(temperatures)) {
                // Copia los datos en el arreglo de valores crudos
                memcpy(temperatures, ptr, sizeof(temperatures));

                // Aplica la máscara a los valores crudos
                temperatures[0] &= mask;
                temperatures[1] &= mask;
                temperatures[2] &= mask;


                // Implementa la lógica de votación
                result = (temperatures[0] & temperatures[1]) | (temperatures[0] & temperatures[2]) | (temperatures[1] & temperatures[2]);


                // Enviar el valor resultante al buffer del monitor
                if (xRingbufferSend(*rbuf_monitor, &result, sizeof(result), pdMS_TO_TICKS(100)) != pdTRUE) {
                    ESP_LOGW(TAG, "Buffer lleno. No se puede enviar el valor resultante.");
                } 
                 
                // Comprobar discrepancias cada N periodos
                if (++period_count >= N_PERIODS) {
                    period_count = 0;

                    if ((abs(temperatures[0] - temperatures[1]) <= 2 && abs(temperatures[0] - temperatures[2]) <= 2 && abs(temperatures[1] - temperatures[2]) <= 2)) {
                        // Todos los sensores funcionan correctamente
                        SWITCH_ST_FROM_TASK(ALL_SENSORS_OK);
                    }
                    else if ((abs(temperatures[0] - temperatures[1]) >= 2 && abs(temperatures[0] - temperatures[2]) >= 2 && abs(temperatures[1] - temperatures[2]) >= 2)) {
                        // Todos los sensores tienen discrepancias significativas
                        SWITCH_ST_FROM_TASK(CRITICAL_ERROR);

                    }  else {  
                        // Un sensor tiene discrepancias significativas
                        SWITCH_ST_FROM_TASK(ONE_SENSOR_FAIL);      
                    }
                }
            } else {
                ESP_LOGE(TAG, "Datos inesperados recibidos: %d bytes", length);
            }

            // Devuelve el elemento al RingBuffer
            vRingbufferReturnItem(*rbuf_sensor, ptr);
        } else {
            ESP_LOGW(TAG, "Esperando datos ...");
        }
    }

    ESP_LOGI(TAG, "Deteniendo la tarea ...");
    TASK_END();
}