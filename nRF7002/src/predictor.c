#include "predictor.h"

/*
 * Coefficients extracted from Python enhanced autoregressive LinearRegression.
 * Feature order:
 *  0: temperature
 *  1: humidity
 *  2: pressure
 *  3: light
 *  4: d_temperature
 *  5: d_humidity
 *  6: d_pressure
 *  7: d_light
 *  8: temperature_x_humidity
 *  9: temperature_x_light
 * 10: humidity_x_light
 * 11: pressure_x_d_pressure
 * 12: temperature_x_d_temperature
 * 13: humidity_x_d_humidity
 */
#define MODEL_FEATURE_COUNT 14
#define MODEL_TARGET_COUNT 4

enum model_target {
	MODEL_TARGET_TEMPERATURE = 0,
	MODEL_TARGET_HUMIDITY = 1,
	MODEL_TARGET_PRESSURE = 2,
	MODEL_TARGET_LIGHT = 3,
};

static const double MODEL_INTERCEPTS[MODEL_TARGET_COUNT] = {
	-1.720639284928443,
	14.280460877491450,
	1.379540682153674,
	-175.346078994297670,
};

static const double MODEL_COEFFICIENTS[MODEL_TARGET_COUNT][MODEL_FEATURE_COUNT] = {
	/* temperature */
	{
		1.015296147130739,
		0.005501353383169,
		0.001227800632550,
		0.000757657812520,
		0.463634953871822,
		-0.003107043383648,
		-4.805325997317104,
		0.003313699234934,
		-0.000192083964887,
		-0.000066368351915,
		0.000002795553045,
		0.004829387765556,
		0.010113084349866,
		-0.000002304352503,
	},
	/* humidity */
	{
		0.008251896251512,
		0.963424221796342,
		-0.011103180060257,
		0.002328743280228,
		-0.519678575721802,
		0.371249126452561,
		17.927125913252787,
		-0.011824974170195,
		0.000001748646873,
		-0.000178470757071,
		-0.000070322593087,
		-0.018531427346264,
		-0.023494368942682,
		-0.001519626080353,
	},
	/* pressure */
	{
		0.003861564703678,
		-0.000418103816897,
		0.998678035854438,
		-0.000062061309717,
		-0.008491314203599,
		-0.022763132712412,
		2.322644147641014,
		-0.000678649286298,
		-0.000014085489260,
		-0.000014008110462,
		-0.000003470841892,
		-0.001598190281629,
		0.000696613181394,
		0.000253148890287,
	},
	/* light */
	{
		-0.110180901905289,
		-0.308072513171242,
		0.211580874605123,
		0.892046233923216,
		12.147046181889365,
		-0.775556963546797,
		-403.217600940145473,
		0.474010698395031,
		0.005230670042366,
		-0.004497901006388,
		-0.000641383405718,
		0.415580374916440,
		0.573595378421750,
		-0.005013259718054,
	},
};

void weather_predict_next_sample(const struct sensor_sample *previous,
				 const struct sensor_sample *current,
				 struct sensor_sample *predicted_out)
{
	double features[MODEL_FEATURE_COUNT];
	double d_temperature;
	double d_humidity;
	double d_pressure;
	double d_light;
	double outputs[MODEL_TARGET_COUNT];
	size_t target_idx;
	size_t feature_idx;

	if (previous == NULL || current == NULL || predicted_out == NULL) {
		return;
	}

	d_temperature = current->temperature - previous->temperature;
	d_humidity = current->humidity - previous->humidity;
	d_pressure = current->pressure - previous->pressure;
	d_light = current->light - previous->light;

	features[0] = current->temperature;
	features[1] = current->humidity;
	features[2] = current->pressure;
	features[3] = current->light;
	features[4] = d_temperature;
	features[5] = d_humidity;
	features[6] = d_pressure;
	features[7] = d_light;
	features[8] = current->temperature * current->humidity;
	features[9] = current->temperature * current->light;
	features[10] = current->humidity * current->light;
	features[11] = current->pressure * d_pressure;
	features[12] = current->temperature * d_temperature;
	features[13] = current->humidity * d_humidity;

	for (target_idx = 0U; target_idx < MODEL_TARGET_COUNT; target_idx++) {
		double value = MODEL_INTERCEPTS[target_idx];

		for (feature_idx = 0U; feature_idx < MODEL_FEATURE_COUNT; feature_idx++) {
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
}
