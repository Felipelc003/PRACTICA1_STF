#ifndef _THERM_H_
#define _THERM_H_

#include <esp_adc/adc_oneshot.h>
#include <hal/adc_types.h>

// Parámetros de configuración del termistor
#define SERIES_RESISTANCE      10000       // Resistencia en serie (10kΩ)
#define NOMINAL_RESISTANCE     10000       // Resistencia nominal a 25°C (10kΩ)
#define NOMINAL_TEMPERATURE    298.15      // 25°C en Kelvin
#define BETA_COEFFICIENT       3950        // Coeficiente B del termistor

// Parámetros de configuración del ADC
#define REFERENCE_VOLTAGE      3.3         // Voltaje de referencia del ADC
#define ADC_MAX_VALUE          4095.0      // Valor máximo del ADC (12 bits)
#define THERMISTOR_ADC_UNIT    ADC_UNIT_1  // Unidad del ADC

// Estructura para representar un termistor
typedef struct therm_conf_t {
    adc_oneshot_unit_handle_t adc_hdlr;  // Manejador del ADC
    adc_channel_t adc_channel;           // Canal del ADC
} therm_t;

// Funciones para configurar y leer el termistor
esp_err_t therm_config(therm_t* thermistor, adc_oneshot_unit_handle_t adc, adc_channel_t channel);
uint16_t therm_read_lsb(therm_t thermistor);
float therm_read_v(therm_t thermistor);
float therm_read_t(therm_t thermistor);

// Funciones privadas de conversión
float _therm_lsb2v(uint16_t lsb);
float _therm_v2t(float v);

#endif