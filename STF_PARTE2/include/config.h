/******************************************************************************
* FILENAME : config.h
*
* DESCRIPTION : 
*
* PUBLIC LICENSE :
* Este código es de uso público y libre de modificar bajo los términos de la
* Licencia Pública General GNU (GPL v3) o posterior. Se proporciona "tal cual",
* sin garantías de ningún tipo.
*
* AUTHOR :   Dr. Fernando Leon (fernando.leon@uco.es) University of Cordoba
******************************************************************************/

#ifndef __CONFIG_H__
#define __CONFIG_H__

// freertos
#include <freertos/FreeRTOS.h>
#include <freertos/ringbuf.h>

// esp
#include <hal/adc_types.h>

// propias
#include "system.h"

// Abstracciones para facilitar la legibilidad
#define CORE0 0
#define CORE1 1

// Configuraciones y constantes

// Nombre y estados de la máquina
#define SYS_NAME "STF P1 System"
enum{
    INIT,
    //SENSOR_LOOP,
    NORMAL_MODE,
    DEGRADED_MODE,
    ERROR
};

// Configuración del termistor
// Ahora la configuración del ADC se maneja automáticamente en el módulo term.c, 
// por lo que no es necesario definirla aquí.
#define SERIES_RESISTANCE 10000       // 10K ohms
#define NOMINAL_RESISTANCE 10000      // 10K ohms
#define NOMINAL_TEMPERATURE 298.15    // 25°C en Kelvin
#define BETA_COEFFICIENT 3950         // Constante B (ajustar según el termistor)
#define THERMISTOR_ADC_CHANNEL ADC_CHANNEL_6

// Configuración del buffer cíclico
#define BUFFER_SIZE  2048
#define BUFFER_TYPE  RINGBUF_TYPE_NOSPLIT

// Configuración de las tareas

// SENSOR
// Tarea sensor
SYSTEM_TASK(TASK_SENSOR);
// definición de los argumentos que requiere la tarea
typedef struct 
{
    RingbufHandle_t* rbuf; // puntero al buffer
    RingbufHandle_t* checker_rbuf; // Buffer hacia Comprobador (NUEVO)
    uint8_t freq;          // frecuencia de muestreo
    uint8_t N;
    // ...
}task_sensor_args_t;

// Timeout de la tarea (ver system_task_stop)
#define TASK_SENSOR_TIMEOUT_MS 2000 
// Tamaño de la pila de la tarea
#define TASK_SENSOR_STACK_SIZE 4096

//---------------------------------------------------------

// CHECKER
SYSTEM_TASK(TASK_CHECKER);
// definición de los argumentos que requiere la tarea
typedef struct 
{
    RingbufHandle_t* checker_rbuf; // puntero al buffer del comprobador
    RingbufHandle_t* monitor_rbuf; // puntero al buffer del monitor
    // ...
}task_checker_args_t;

// Tamaño de la pila de la tarea
#define TASK_CHECKER_STACK_SIZE 4096

// Timeout de la tarea (ver system_task_stop)
#define TASK_CHECKER_TIMEOUT_MS 2000

//---------------------------------------------------------

// MONITOR
SYSTEM_TASK(TASK_MONITOR);
// definición de los argumentos que requiere la tarea
typedef struct 
{
    RingbufHandle_t* rbuf; // puntero al buffer 
    // ...
}task_monitor_args_t;

// Tamaño de la pila de la tarea
#define TASK_MONITOR_STACK_SIZE 4096

// Estructura para los mensajes
typedef struct {
    uint8_t id_source;      // Origen del mensaje
    float data;             // Dato adicional (desviación)
} message_monitor_t;


// Timeout de la tarea (ver system_task_stop)
#define TASK_MONITOR_TIMEOUT_MS 2000 
// Tamaño de la pila de la tarea
#define TASK_MONITOR_STACK_SIZE 4096

#define SECOND_THERMISTOR_ADC_CHANNEL ADC_CHANNEL_7 // Canal ADC del segundo termistor

// Configuración del número de periodos para activar el comprobador
//#define SENSOR_CHECK_PERIOD 5 // Número de periodos (N)

// Configuración del buffer entre Sensor y Comprobador
#define BUFFER_CHECKER_SIZE  1024
#define BUFFER_CHECKER_TYPE  RINGBUF_TYPE_NOSPLIT

#endif // __CONFIG_H__