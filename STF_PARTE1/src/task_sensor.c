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

// // libc 
// #include <time.h>
// #include <stdio.h>
// #include <math.h>
// #include <sys/time.h>
// #include <string.h>

// // freertos
// #include <freertos/FreeRTOS.h>
// #include <freertos/semphr.h>

// // espidf
// #include <esp_system.h>
// #include <esp_event.h>
// #include <esp_log.h>
// #include <esp_timer.h>
// #include <esp_adc/adc_oneshot.h>

// // propias
// #include "config.h"

// static const char *TAG = "STF_P1:task_sensor";

// // esta tarea temporiza la lectura de un termistor. 
// // Para implementar el periodo de muestreo, se utiliza un semáforo declarado de forma global 
// // para poder ser utilizado desde esta rutina de expiración del timer.
// // Cada vez que el temporizador expira, libera el semáforo
// // para que la tarea realice una iteración. 
// static SemaphoreHandle_t semSample = NULL;
// static void tmrSampleCallback(void* arg)
// {
// 	xSemaphoreGive(semSample);
// }

// // ESP32 tiene unidad de conversión analógica digital, que debe configurarse 
// // previamente a las lecturas mediante esta estructura manejador. Se define global
// // porque únicamente usaremos un ADC. Se configura en la tarea antes del loop
// adc_oneshot_unit_handle_t adc_hdlr; 


// // Funciones de lectura del sensor.

// // Se trata de un termistor, un sensor analógico activo (require alimentación)
// // cuya salida es voltaje. El circuito está en la cabecera de este fichero. 
// //
// // Pasos para adquirir un valor: 
// // 1) El ADC del SoC devuelve un valor binario crudo (LSB), que corresponde a un valor
// // de voltaje en la entrada del canal.

// // 2) Convertir este valor en voltaje. Para esto, es necesario conocer el número 
// // de bits que tiene el ADC (para conocer el máximo valor binario que
// // se puede obtener) y el voltaje de referencia (el máximo voltaje que es 
// // capaz de medir)

// // LSB crudo
// int read_adc() 
// {
//     int raw_value = 0;
//     ESP_ERROR_CHECK(adc_oneshot_read(adc_hdlr, THERMISTOR_ADC_CHANNEL, &raw_value));
//     return raw_value;
// }

// // LSB -> V
// // 12 bits -> 2^12 = 4096 -> rango de 0 a 4095. 
// // V de referencia interno del ESP32 -> 3.3V
// #define lsb_to_v(x) (float) ((x) * 3.3f / 4095.0f)

// // V -> Temperatura en C
// float v_to_temperature(float v)
// {
// 	// resistencia del termistor, obtenida por el voltaje medido en el adc. 
//     float r_ntc = SERIES_RESISTANCE * (3.3 - v) / v;

// 	// Ecuación de Steinhart-Hart, que relaciona la resistencia que ofrece un material semiconductor 
// 	// con la variación de la temperatura en Kelvin, de acuerdo a unos coeficientes que caracterizan
// 	// al semiconductor en cuestión (están definidos en config.h) 
// 	float t_kelvin = 1.0f / (1.0f / NOMINAL_TEMPERATURE + (1.0f / BETA_COEFFICIENT) * log(r_ntc / NOMINAL_RESISTANCE));

// 	// Resultado en grados centígrados
//     return(t_kelvin - 273.15f);  
// }

// // Tarea SENSOR
// SYSTEM_TASK(TASK_SENSOR)
// {	
// 	TASK_BEGIN();
// 	ESP_LOGI(TAG,"Task Sensor running");

// 	// Recibe los argumentos de configuración de la tarea y los desempaqueta
// 	task_sensor_args_t* ptr_args = (task_sensor_args_t*) TASK_ARGS;
// 	RingbufHandle_t* rbuf = ptr_args->rbuf; 
// 	uint8_t frequency = ptr_args->freq;
// 	uint64_t period_us = 1000000 / frequency;

// 	// Estructura de datos para configurar la unidad ADC
//     adc_oneshot_unit_init_cfg_t unit_cfg = {
//         .unit_id = THERMISTOR_ADC_UNIT,
//         .clk_src = ADC_RTC_CLK_SRC_DEFAULT,
//     };
// 	// Establecimiento de la configuracón en ADC
//     ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit_cfg, &adc_hdlr));

// 	// Estructura de datos para configurar el canal de la unidad ADC
//     adc_oneshot_chan_cfg_t channel_cfg = {
//         .atten = ADC_ATTEN_DB_11, // Rango de 0 a 3.3V
//         .bitwidth = ADC_BITWIDTH_12, // Resolución de 12 bits
//     };
// 	// Establecimiento de la configuración del canal
//     ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_hdlr, THERMISTOR_ADC_CHANNEL, &channel_cfg));

// 	// Inicializa el semásforo (la estructura del manejador se definió globalmente)
// 	semSample = xSemaphoreCreateBinary();

// 	// Crea y establece una estructura de configuración para el temporizador
// 	const esp_timer_create_args_t tmrSampleArgs = {
// 		.callback = &tmrSampleCallback,
// 		.name = "Timer Configuration"
// 	};

// 	// Lanza el temporizador, conel periodo de muestreo recibido como parámetro
// 	esp_timer_handle_t tmrSample;
// 	ESP_ERROR_CHECK(esp_timer_create(&tmrSampleArgs, &tmrSample));
// 	ESP_ERROR_CHECK(esp_timer_start_periodic(tmrSample, period_us));
	
// 	// variables para reutilizar en el bucle
// 	void *ptr;
// 	float v;

// 	// Loop
// 	TASK_LOOP()
// 	{
// 		// Se bloquea a la espera del semáforo. Si el periodo establecido se retrasa un 20%
// 		// el sistema se reinicia por seguridad. Este mecanismo de watchdog software es útil
// 		// en tareas periódicas cuyo periodo es conocido. 
// 		if(xSemaphoreTake(semSample, ((1000/frequency)*1.2)/portTICK_PERIOD_MS))
// 		{	
// 			// lectura del sensor. Se obtiene el valor en grados centígrados
// 			v = v_to_temperature(lsb_to_v(read_adc()));
// 			ESP_LOGI(TAG, "valor medido (pre buffer): %f", v);


// 			// Uso del buffer cíclico entre la tarea monitor y sensor. Ver documentación en ESP-IDF
// 			// Pide al RingBuffer espacio para escribir un float. 
// 			if (xRingbufferSendAcquire(*rbuf, &ptr, sizeof(float), pdMS_TO_TICKS(100)) != pdTRUE)
// 			{
// 				// Si falla la reserva de memoria, notifica la pérdida del dato. Esto ocurre cuando 
// 				// una tarea productora es mucho más rápida que la tarea consumidora. Aquí no debe ocurrir.
// 				ESP_LOGI(TAG,"Buffer lleno. Espacio disponible: %d", xRingbufferGetCurFreeSize(*rbuf));
// 			}
// 			else 
// 			{
// 				// Si xRingbufferSendAcquire tiene éxito, podemos escribir el número de bytes solicitados
// 				// en el puntero ptr. El espacio asignado estará bloqueado para su lectura hasta que 
// 				// se notifique que se ha completado la escritura
// 				memcpy(ptr,&v, sizeof(float));

// 				// Se notifica que la escritura ha completado. 
// 				xRingbufferSendComplete(*rbuf, ptr);
// 			}
// 		}
// 		else
// 		{
// 			ESP_LOGI(TAG,"Watchdog (soft) failed");
// 			esp_restart();
// 		}
// 	}
	
// 	ESP_LOGI(TAG,"Deteniendo la tarea...");
// 	// detención controlada de las estructuras que ha levantado la tarea
// 	ESP_ERROR_CHECK(esp_timer_stop(tmrSample));
// 	ESP_ERROR_CHECK(esp_timer_delete(tmrSample));
// 	TASK_END();
// }

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

// Implementación de la tarea
SYSTEM_TASK(TASK_SENSOR) {
    TASK_BEGIN();
    ESP_LOGI(TAG, "Task Sensor running");

    // Argumentos de la tarea
    task_sensor_args_t* ptr_args = (task_sensor_args_t*)TASK_ARGS;
    RingbufHandle_t* rbuf = ptr_args->rbuf;  // Buffer para enviar datos
    uint8_t frequency = ptr_args->freq;     // Frecuencia de muestreo
    uint64_t period_us = 1000000 / frequency; // Periodo en microsegundos

    // Configuración del ADC y del termistor
    adc_oneshot_unit_handle_t adc_hdlr;
    adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id = THERMISTOR_ADC_UNIT,       // Unidad de ADC definida en config.h
        .clk_src = ADC_RTC_CLK_SRC_DEFAULT,  // Fuente de reloj por defecto
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit_cfg, &adc_hdlr));

    // Inicialización del termistor
    therm_t thermistor;
    ESP_ERROR_CHECK(therm_config(&thermistor, adc_hdlr, THERMISTOR_ADC_CHANNEL));

    // Inicialización del semáforo para sincronización
    semSample = xSemaphoreCreateBinary();
	
    // Configuración y lanzamiento del temporizador
    const esp_timer_create_args_t tmrSampleArgs = {
        .callback = &tmrSampleCallback,
        .arg = NULL,
        .name = "SensorTimer",
    };
    esp_timer_handle_t tmrSample;
    ESP_ERROR_CHECK(esp_timer_create(&tmrSampleArgs, &tmrSample));
    ESP_ERROR_CHECK(esp_timer_start_periodic(tmrSample, period_us));

    // Bucle principal de la tarea
    void* ptr;  // Puntero para manejar datos en el buffer
    float temperature;  // Almacena la temperatura leída

    TASK_LOOP() {
        // Espera a que el semáforo se libere
        if (xSemaphoreTake(semSample, pdMS_TO_TICKS(1000 / frequency))) {
            // Lee la temperatura del termistor
            temperature = therm_read_t(thermistor);
            ESP_LOGI(TAG, "Temperatura medida: %.2f °C", temperature);

            // Reserva espacio en el buffer para enviar la lectura
            if (xRingbufferSendAcquire(*rbuf, &ptr, sizeof(float), pdMS_TO_TICKS(100)) != pdTRUE) {
                ESP_LOGW(TAG, "Buffer lleno. No se puede enviar la muestra.");
            } else {
                memcpy(ptr, &temperature, sizeof(float));  // Copia la lectura en el buffer
                xRingbufferSendComplete(*rbuf, ptr);      // Marca la operación como completa
            }
        } else {
            ESP_LOGE(TAG, "Timeout al esperar el semáforo.");
        }
    }

    // Limpieza al finalizar la tarea
    ESP_ERROR_CHECK(esp_timer_stop(tmrSample));
    ESP_ERROR_CHECK(esp_timer_delete(tmrSample));
    vSemaphoreDelete(semSample);
    TASK_END();
}