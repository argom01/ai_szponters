#ifndef PREDICTOR_H_
#define PREDICTOR_H_

#include <stdbool.h>
#include <stddef.h>

#include "data.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Number of history samples required by the predictor (oldest..newest). */
#define WEATHER_MODEL_HISTORY_STEPS 8U

/*
 * Enhanced autoregressive linear model trained in Python on Open-Meteo data.
 * It predicts next sample (t+1) from a history window of
 * WEATHER_MODEL_HISTORY_STEPS samples.
 *
 * Returns false if history_len is smaller than WEATHER_MODEL_HISTORY_STEPS
 * or if any pointer argument is NULL.
 */
bool weather_predict_next_sample_from_history(const struct sensor_sample *history,
					      size_t history_len,
					      struct sensor_sample *predicted_out);

#ifdef __cplusplus
}
#endif

#endif /* PREDICTOR_H_ */
