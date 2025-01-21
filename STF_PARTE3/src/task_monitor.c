#include "config.h"
#include "therm.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/ringbuf.h>
#include <esp_log.h>
#include <string.h>

static const char *TAG = "STF_P1:task_monitor";

// Tarea MONITOR
SYSTEM_TASK(TASK_MONITOR) {
    TASK_BEGIN();
    ESP_LOGI(TAG, "Task Monitor running");

    // Recibe los argumentos de configuración de la tarea y los desempaqueta
    task_monitor_args_t* ptr_args = (task_monitor_args_t*)TASK_ARGS;
    RingbufHandle_t* rbuf = ptr_args->rbuf; 

    // Variables para reutilizar en el bucle
    size_t length;
    void *ptr;
    uint16_t result;
    float temperature;

    // Loop
    TASK_LOOP() {
        // Bloquea en espera de datos del RingBuffer
        ptr = xRingbufferReceive(*rbuf, &length, pdMS_TO_TICKS(1000));

        if (ptr != NULL) {
            if (length == sizeof(result)) {
                // Copia el dato en la variable resultante
                memcpy(&result, ptr, sizeof(result));

                // Convierte el valor crudo a temperatura en Celsius
                temperature = _therm_lsb2v(result);
                temperature = _therm_v2t(temperature);


                switch (__task->system->sys_state) {
                    case ALL_SENSORS_OK:
                        ESP_LOGI(TAG, "Temperatura resultante: %.2f °C", temperature);
                        break;

                    case ONE_SENSOR_FAIL:
                        ESP_LOGW(TAG, "Un sensor fallando. Temperatura mayoritaria: %.2f °C", temperature);
                        break;

                    case CRITICAL_ERROR:
                        ESP_LOGE(TAG, "Error crítico. Deteniendo el sistema.");
                        vTaskDelay(pdMS_TO_TICKS(1000)); // Delay to allow log message to be printed
                        esp_restart(); // Reinicia el sistema
                        break;

                    default:
                        ESP_LOGI(TAG, "Estado desconocido.");
                        break;
                }
            } else {
                ESP_LOGE(TAG, "Datos inesperados recibidos: %d bytes", length);
            }

            // Devuelve el elemento al RingBuffer
            vRingbufferReturnItem(*rbuf, ptr);
        } else {
            ESP_LOGW(TAG, "Esperando datos ...");
        }
    }

    ESP_LOGI(TAG, "Deteniendo la tarea ...");
    TASK_END();
}