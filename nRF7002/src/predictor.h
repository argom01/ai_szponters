#ifndef PREDICTOR_H_
#define PREDICTOR_H_

#include "data.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Enhanced autoregressive linear model trained in Python on Open-Meteo data.
 * It predicts next sample (t+1) using:
 * - previous sample (t-1)
 * - current sample (t)
 * for temperature, humidity, pressure and light.
 */
void weather_predict_next_sample(const struct sensor_sample *previous,
				 const struct sensor_sample *current,
				 struct sensor_sample *predicted_out);

#ifdef __cplusplus
}
#endif

#endif /* PREDICTOR_H_ */
