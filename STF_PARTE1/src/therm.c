#include "therm.h"
#include <math.h>

// Configura el termistor con un manejador ADC y un canal específico
esp_err_t therm_config(therm_t* thermistor, adc_oneshot_unit_handle_t adc, adc_channel_t channel) {
    if (!thermistor) return ESP_ERR_INVALID_ARG;

    thermistor->adc_hdlr = adc;
    thermistor->adc_channel = channel;

    adc_oneshot_chan_cfg_t channel_cfg = {
        .atten = ADC_ATTEN_DB_11,  // Rango de 0 a 3.3V
        .bitwidth = ADC_BITWIDTH_12  // Resolución de 12 bits
    };
    return adc_oneshot_config_channel(adc, channel, &channel_cfg);
}

// Lee el valor crudo (LSB) del ADC para el termistor
uint16_t therm_read_lsb(therm_t thermistor) {
    int raw_value = 0;
    esp_err_t ret = adc_oneshot_read(thermistor.adc_hdlr, thermistor.adc_channel, &raw_value);
    if (ret != ESP_OK) {
        return 0; // Manejo básico de errores, devolver 0 si falla
    }
    return (uint16_t)raw_value;
}

// Convierte el valor LSB del ADC a voltaje
float therm_read_v(therm_t thermistor) {
    return _therm_lsb2v(therm_read_lsb(thermistor));
}

// Lee la temperatura en grados Celsius
float therm_read_t(therm_t thermistor) {
    return _therm_v2t(therm_read_v(thermistor));
}

// Convierte el voltaje leído en temperatura en Celsius
float _therm_v2t(float v) {
    float r_ntc = SERIES_RESISTANCE * (REFERENCE_VOLTAGE - v) / v;
    float t_kelvin = 1.0f / (1.0f / NOMINAL_TEMPERATURE + (1.0f / BETA_COEFFICIENT) * log(r_ntc / NOMINAL_RESISTANCE));
    return t_kelvin - 273.15f; // Convertir a Celsius
}

// Convierte el valor crudo LSB a voltaje
float _therm_lsb2v(uint16_t lsb) {
    return (float)(lsb * REFERENCE_VOLTAGE / ADC_MAX_VALUE);
}
