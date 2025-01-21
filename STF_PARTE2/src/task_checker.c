#include "config.h"
#include <freertos/FreeRTOS.h>
#include <freertos/ringbuf.h>
#include <esp_log.h>
#include <math.h>
#include <string.h>

static const char* TAG = "STF_P1:task_checker";

extern system_t sys_stf_p1;

SYSTEM_TASK(TASK_CHECKER) {
    TASK_BEGIN();
    ESP_LOGI(TAG, "Task Checker running");

    // Argumentos de la tarea
    task_checker_args_t* args = (task_checker_args_t*)TASK_ARGS;
    RingbufHandle_t* checker_rbuf = args->checker_rbuf;
    RingbufHandle_t* monitor_rbuf = args->monitor_rbuf;

    // Variables locales
    size_t length;
    void* ptr;
    float temp1, temp2, desviacion;

    TASK_LOOP() {
        // Recibir T1 y T2 del Sensor
        ptr = xRingbufferReceive(*checker_rbuf, &length, pdMS_TO_TICKS(1000));
        if (ptr != NULL) {
            
            temp1 = *((float*)ptr);
            temp2 = *(((float*)ptr) + 1);

            // Calcular desviación
            desviacion = fabs(100 * (temp1 - temp2) / temp1);

            // Enviar desviación al Monitor
            message_monitor_t message = { .id_source = 2, .data = desviacion}; 
            xRingbufferSend(*monitor_rbuf, &message, sizeof(message_monitor_t), pdMS_TO_TICKS(100));
            
            // Liberar el buffer recibido
            vRingbufferReturnItem(*checker_rbuf, ptr);

             // Cambiar el estado de la máquina según la desviación
            if (desviacion > 10.0 && desviacion < 20.0) {
                SWITCH_ST_FROM_TASK(DEGRADED_MODE);
            } else if (desviacion >= 20.0) {
                SWITCH_ST_FROM_TASK(ERROR);
            } else if (desviacion <= 10.0) {
                SWITCH_ST_FROM_TASK(NORMAL_MODE);
            }
        }
    }

    TASK_END();
}