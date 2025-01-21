/******************************************************************************
* FILENAME : main.c
*
* DESCRIPTION : Sistema de monitorización para múltiples sensores.
*
* PUBLIC LICENSE :
* Este código es de uso público y libre de modificar bajo los términos de la
* Licencia Pública General GNU (GPL v3) o posterior. Se proporciona "tal cual",
* sin garantías de ningún tipo.
*
* AUTHOR :   Dr. Fernando Leon (fernando.leon@uco.es) University of Cordoba
******************************************************************************/

// libc
#include <stdio.h>
#include <assert.h>

// freertos
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/ringbuf.h>
#include <freertos/semphr.h>

// esp-idf
#include <esp_log.h>
#include <esp_event.h>
#include <nvs_flash.h>

// propias
#include "config.h"
#include "system.h"

static const char *TAG = "STF_P1:main";

// Punto de entrada
void app_main(void)
{
    // Crea una instancia de máquina de estados (ver system.h).
    system_t sys_stf_p1;
    ESP_LOGI(TAG, "Starting STF_P1 system");
    system_create(&sys_stf_p1, SYS_NAME);
    system_register_state(&sys_stf_p1, INIT);
    //system_register_state(&sys_stf_p1, SENSOR_LOOP);
    system_register_state(&sys_stf_p1, ALL_SENSORS_OK);
    system_register_state(&sys_stf_p1, ONE_SENSOR_FAIL);
    system_register_state(&sys_stf_p1, CRITICAL_ERROR);
    system_set_default_state(&sys_stf_p1, INIT);

    // Define manejadores de tareas
    system_task_t task_sensor;
    system_task_t task_monitor;
    system_task_t task_votador;// manejador de la tarea votador

    // Define y crea un buffer cíclico (RingBuffer) para múltiples sensores.
    RingbufHandle_t rbuf_sensor;
    rbuf_sensor = xRingbufferCreate(BUFFER_SIZE, BUFFER_TYPE);
    if (rbuf_sensor == NULL) {
        ESP_LOGE(TAG, "Failed to create RingBuffer for sensor");
        return;
    }

    // Define y crea un buffer cíclico (RingBuffer) para el monitor.
    RingbufHandle_t rbuf_monitor;
    rbuf_monitor = xRingbufferCreate(BUFFER_SIZE, BUFFER_TYPE);
    if (rbuf_monitor == NULL) {
        ESP_LOGE(TAG, "Failed to create RingBuffer for monitor");
        return;
    }

    // Variable para códigos de retorno
    esp_err_t ret;

    // Configuración de la máquina de estados
    STATE_MACHINE(sys_stf_p1)
    {
        STATE_MACHINE_BEGIN();
        STATE(INIT)
        {
            STATE_BEGIN();
            ESP_LOGI(TAG, "State: INIT");

            // Inicialización de memoria no volátil
            ret = nvs_flash_init();
            if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
                ESP_ERROR_CHECK(nvs_flash_erase());
                ESP_ERROR_CHECK(nvs_flash_init());
            }

            // Configuración y arranque de la tarea SENSOR
            ESP_LOGI(TAG, "Starting sensor task...");
            task_sensor_args_t task_sensor_args = {&rbuf_sensor, 2}; // Maneja 2 sensores
            system_task_start_in_core(&sys_stf_p1, &task_sensor, TASK_SENSOR, "TASK_SENSOR", TASK_SENSOR_STACK_SIZE, &task_sensor_args, 0, CORE0);
            ESP_LOGI(TAG, "Sensor task started");

            // Delay para evitar conflictos de inicialización
            vTaskDelay(pdMS_TO_TICKS(1000));

             // Configuración y arranque de la tarea VOTADOR
            ESP_LOGI(TAG, "Starting votador task...");
            task_votador_args_t task_votador_args = {&rbuf_sensor, &rbuf_monitor, 0xFF80}; // Máscara de votación, cuantos más bits menos significativos se quiten, más precisión pierde
            system_task_start_in_core(&sys_stf_p1, &task_votador, TASK_VOTADOR, "TASK_VOTADOR", TASK_VOTADOR_STACK_SIZE, &task_votador_args, 0, CORE0);
            ESP_LOGI(TAG, "Votador task started");

            // Delay para evitar conflictos de inicialización
            vTaskDelay(pdMS_TO_TICKS(1000));

            // Configuración y arranque de la tarea MONITOR
            ESP_LOGI(TAG, "Starting monitor task...");
            task_monitor_args_t task_monitor_args = {&rbuf_monitor};
            system_task_start_in_core(&sys_stf_p1, &task_monitor, TASK_MONITOR, "TASK_MONITOR", TASK_MONITOR_STACK_SIZE, &task_monitor_args, 0, CORE1);
            ESP_LOGI(TAG, "Monitor task started");

            // Cambia al estado SENSOR_LOOP
            SWITCH_ST(&sys_stf_p1, ALL_SENSORS_OK);
            STATE_END();
        }
        
        STATE(ALL_SENSORS_OK)
        {
            STATE_BEGIN();
            ESP_LOGI(TAG, "State: ALL_SENSORS_OK");
            STATE_END();
        }
        STATE(ONE_SENSOR_FAIL)
        {
            STATE_BEGIN();
            ESP_LOGI(TAG, "State: ONE_SENSOR_FAIL");
            STATE_END();
        }
        STATE(CRITICAL_ERROR)
        {
            STATE_BEGIN();
            ESP_LOGI(TAG, "State: CRITICAL_ERROR");
            STATE_END();
        }
        STATE_MACHINE_END();
    }
}
