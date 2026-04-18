#include "predictor.h"

/*
 * Coefficients extracted from Python long-history autoregressive LinearRegression.
 * Feature order:
 *  0: temperature_lag0
 *  1: humidity_lag0
 *  2: pressure_lag0
 *  3: light_lag0
 *  4: temperature_lag1
 *  5: humidity_lag1
 *  6: pressure_lag1
 *  7: light_lag1
 *  8: temperature_lag2
 *  9: humidity_lag2
 * 10: pressure_lag2
 * 11: light_lag2
 * 12: temperature_lag3
 * 13: humidity_lag3
 * 14: pressure_lag3
 * 15: light_lag3
 * 16: temperature_lag4
 * 17: humidity_lag4
 * 18: pressure_lag4
 * 19: light_lag4
 * 20: temperature_lag5
 * 21: humidity_lag5
 * 22: pressure_lag5
 * 23: light_lag5
 * 24: temperature_lag6
 * 25: humidity_lag6
 * 26: pressure_lag6
 * 27: light_lag6
 * 28: temperature_lag7
 * 29: humidity_lag7
 * 30: pressure_lag7
 * 31: light_lag7
 * 32: d_temperature
 * 33: d_humidity
 * 34: d_pressure
 * 35: d_light
 * 36: mean_temperature
 * 37: mean_humidity
 * 38: mean_pressure
 * 39: mean_light
 */
#define MODEL_FEATURE_COUNT 40U
#define MODEL_TARGET_COUNT 4U

enum model_target {
	MODEL_TARGET_TEMPERATURE = 0,
	MODEL_TARGET_HUMIDITY = 1,
	MODEL_TARGET_PRESSURE = 2,
	MODEL_TARGET_LIGHT = 3,
};

static const double MODEL_INTERCEPTS[MODEL_TARGET_COUNT] = {
	-2.318958802875624,
	18.791552671652333,
	2.094142450159438,
	-267.488648470408009,
};

static const double MODEL_COEFFICIENTS[MODEL_TARGET_COUNT][MODEL_FEATURE_COUNT] = {
	/* temperature */
	{
		0.806622579384549, 0.002661638521559, 0.077050037211873, 0.001914414207263,
		0.229255784269510, 0.008145285487418, 0.014527453134898, 0.000200645079402,
		-0.050135981771386, -0.000765386053379, -0.080960122432279, -0.001568697278726,
		-0.102318820453955, -0.010238052813798, -0.012321265191279, 0.000052106616744,
		0.042226697745661, 0.012504963189747, -0.120591965027770, -0.000079055556828,
		-0.013611477723136, -0.007627664512627, -0.056562630184416, 0.000609102655147,
		-0.059165958391803, -0.000482704895230, 0.142441640105991, 0.000558962207614,
		0.031958331327143, -0.001677937488149, 0.038270434966506, -0.001574961789450,
		0.577366795115036, -0.005483646965878, 0.062522584076972, 0.001713769127834,
		0.110603894298322, 0.000315017679440, 0.000231697822940, 0.000014064517643,
	},
	/* humidity */
	{
		-0.100124941150643, 0.724797101856519, -0.391264454608473, -0.008516184715615,
		0.327266265571161, 0.241066576193612, -0.225986266918780, -0.002766333315330,
		-0.049099169314635, 0.005109791179165, 0.441363875929059, 0.010405789674774,
		-0.064275267412432, -0.100687196533368, 0.325721008824175, -0.003192960942004,
		0.003254758516970, 0.010357688108223, 0.306228992352908, -0.006109956171262,
		-0.015364177315305, 0.006677887469228, 0.173430228933776, 0.005641740141550,
		-0.009168993590474, -0.058296927878873, -0.609953957812357, -0.004595078447252,
		-0.092193724873001, 0.023416593708197, -0.033028964168337, 0.005575364006466,
		-0.427391206721800, 0.483730525662921, -0.165278187689686, -0.005749851400273,
		0.000036843803956, 0.106555189262839, -0.001686192183506, -0.000444702471076,
	},
	/* pressure */
	{
		0.001039937083138, -0.001300416111141, 0.858198488497839, -0.000648102717116,
		-0.003188155372111, 0.001087389126454, 0.283409569214878, -0.000328831233117,
		0.032905048210087, -0.002195746801901, -0.043115518157606, 0.000311838889258,
		-0.018569631283201, 0.003217163630308, -0.166373613279922, -0.000095616555076,
		-0.018304922753782, -0.002970370900251, 0.029656140699386, 0.000572910203723,
		-0.012515212492611, 0.000420902753117, 0.100757121383061, -0.000132146245983,
		0.030021624866788, 0.003084043698723, -0.244966803288297, 0.000443994645438,
		-0.010623676952537, -0.002204435636859, 0.069562509715909, -0.000527060955436,
		0.004228092455250, -0.002387805237595, 0.574788919282960, -0.000319271484010,
		0.000095626413221, -0.000107683780194, 0.110890986848159, -0.000050376746044,
	},
	/* light */
	{
		3.251436816335460, -0.504424609838884, 9.718610369582095, 0.727171353159617,
		-3.832836356836592, 0.394833181770089, 5.833042615854212, 0.266270314690837,
		1.395081137323856, -0.481013207872671, -8.502419365669414, -0.181974370993744,
		-2.764249011337682, 0.271620628940243, -9.042600838189944, -0.047839534263017,
		0.465842644186024, 0.092403516364235, -9.808121119712503, 0.000028934839935,
		-4.332837626448140, 0.084477250794467, -4.126465572662508, 0.044320856734059,
		5.366590366081653, 0.006152443161384, 13.598691361897552, 0.002038617630485,
		0.564734345531526, -0.153580408547322, 2.601098666732941, -0.090048696327222,
		7.084273173171992, -0.899257791609144, 3.885567753727788, 0.460901038468628,
		0.014220289354493, -0.036191400653558, 0.033979514729127, 0.089995934433745,
	},
};

static void build_features_from_history(const struct sensor_sample *history, double *features)
{
	double sum_temperature = 0.0;
	double sum_humidity = 0.0;
	double sum_pressure = 0.0;
	double sum_light = 0.0;
	size_t feature_idx = 0U;
	const struct sensor_sample *latest = &history[WEATHER_MODEL_HISTORY_STEPS - 1U];
	const struct sensor_sample *previous = &history[WEATHER_MODEL_HISTORY_STEPS - 2U];
	double d_temperature = latest->temperature - previous->temperature;
	double d_humidity = latest->humidity - previous->humidity;
	double d_pressure = latest->pressure - previous->pressure;
	double d_light = latest->light - previous->light;

	for (size_t lag = 0U; lag < WEATHER_MODEL_HISTORY_STEPS; lag++) {
		const struct sensor_sample *sample =
			&history[(WEATHER_MODEL_HISTORY_STEPS - 1U) - lag];

		features[feature_idx++] = sample->temperature;
		features[feature_idx++] = sample->humidity;
		features[feature_idx++] = sample->pressure;
		features[feature_idx++] = sample->light;

		sum_temperature += sample->temperature;
		sum_humidity += sample->humidity;
		sum_pressure += sample->pressure;
		sum_light += sample->light;
	}

	features[feature_idx++] = d_temperature;
	features[feature_idx++] = d_humidity;
	features[feature_idx++] = d_pressure;
	features[feature_idx++] = d_light;

	features[feature_idx++] = sum_temperature / (double)WEATHER_MODEL_HISTORY_STEPS;
	features[feature_idx++] = sum_humidity / (double)WEATHER_MODEL_HISTORY_STEPS;
	features[feature_idx++] = sum_pressure / (double)WEATHER_MODEL_HISTORY_STEPS;
	features[feature_idx++] = sum_light / (double)WEATHER_MODEL_HISTORY_STEPS;
}

bool weather_predict_next_sample_from_history(const struct sensor_sample *history,
					      size_t history_len,
					      struct sensor_sample *predicted_out)
{
	double features[MODEL_FEATURE_COUNT];
	double outputs[MODEL_TARGET_COUNT];

	if (history == NULL || predicted_out == NULL ||
	    history_len < WEATHER_MODEL_HISTORY_STEPS) {
		return false;
	}

	build_features_from_history(history, features);

	for (size_t target_idx = 0U; target_idx < MODEL_TARGET_COUNT; target_idx++) {
		double value = MODEL_INTERCEPTS[target_idx];

		for (size_t feature_idx = 0U; feature_idx < MODEL_FEATURE_COUNT; feature_idx++) {
			value += MODEL_COEFFICIENTS[target_idx][feature_idx] * features[feature_idx];
		}

		outputs[target_idx] = value;
	}

	if (outputs[MODEL_TARGET_HUMIDITY] < 0.0) {
		outputs[MODEL_TARGET_HUMIDITY] = 0.0;
	} else if (outputs[MODEL_TARGET_HUMIDITY] > 100.0) {
		outputs[MODEL_TARGET_HUMIDITY] = 100.0;
	}

	if (outputs[MODEL_TARGET_LIGHT] < 0.0) {
		outputs[MODEL_TARGET_LIGHT] = 0.0;
	}

	predicted_out->temperature = outputs[MODEL_TARGET_TEMPERATURE];
	predicted_out->humidity = outputs[MODEL_TARGET_HUMIDITY];
	predicted_out->pressure = outputs[MODEL_TARGET_PRESSURE];
	predicted_out->light = outputs[MODEL_TARGET_LIGHT];
	return true;
}
