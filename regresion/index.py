"""Enhanced next-step regression on open weather data.

Model objective:
- predict next sample (t+1) for:
	- temperature
	- humidity
	- pressure
	- light (shortwave radiation)
- using a longer autoregressive history window

Install dependencies:
	pip install requests pandas scikit-learn
"""

from __future__ import annotations

from datetime import date, timedelta

import pandas as pd
import requests
from sklearn.linear_model import LinearRegression
from sklearn.metrics import mean_absolute_error, r2_score, root_mean_squared_error
from sklearn.model_selection import train_test_split


OPEN_METEO_ARCHIVE_URL = "https://archive-api.open-meteo.com/v1/archive"
TARGET_COLUMNS = ["temperature", "humidity", "pressure", "light"]
HISTORY_STEPS = 8


def build_feature_columns() -> list[str]:
	"""Build deterministic feature order for both Python and C."""
	columns: list[str] = []

	# lag0 is newest sample, lag7 is the oldest sample in 8-step window
	for lag in range(HISTORY_STEPS):
		for base_name in TARGET_COLUMNS:
			columns.append(f"{base_name}_lag{lag}")

	for base_name in TARGET_COLUMNS:
		columns.append(f"d_{base_name}")

	for base_name in TARGET_COLUMNS:
		columns.append(f"mean_{base_name}")

	return columns


FEATURE_COLUMNS = build_feature_columns()


def fetch_open_meteo_hourly_data(
	latitude: float,
	longitude: float,
	start_date: date,
	end_date: date,
) -> pd.DataFrame:
	"""Download hourly weather data from Open-Meteo archive API."""
	params = {
		"latitude": latitude,
		"longitude": longitude,
		"start_date": start_date.isoformat(),
		"end_date": end_date.isoformat(),
		"hourly": [
			"shortwave_radiation",
			"temperature_2m",
			"relative_humidity_2m",
			"surface_pressure",
		],
		"timezone": "auto",
	}

	response = requests.get(OPEN_METEO_ARCHIVE_URL, params=params, timeout=30)
	response.raise_for_status()
	payload = response.json()

	if "hourly" not in payload:
		raise RuntimeError("API response does not include hourly weather data.")

	hourly = payload["hourly"]
	required_keys = [
		"time",
		"shortwave_radiation",
		"temperature_2m",
		"relative_humidity_2m",
		"surface_pressure",
	]
	missing = [key for key in required_keys if key not in hourly]
	if missing:
		raise RuntimeError(f"Missing expected fields in API response: {missing}")

	data = pd.DataFrame(
		{
			"time": pd.to_datetime(hourly["time"]),
			"light": hourly["shortwave_radiation"],
			"temperature": hourly["temperature_2m"],
			"humidity": hourly["relative_humidity_2m"],
			"pressure": hourly["surface_pressure"],
		}
	)

	return data.dropna().reset_index(drop=True)


def make_feature_row(window: pd.DataFrame) -> dict[str, float]:
	"""Build one feature row from the history window."""
	latest = window.iloc[-1]
	previous = window.iloc[-2]
	features: dict[str, float] = {}

	for lag in range(HISTORY_STEPS):
		sample = window.iloc[HISTORY_STEPS - 1 - lag]
		for base_name in TARGET_COLUMNS:
			features[f"{base_name}_lag{lag}"] = float(sample[base_name])

	for base_name in TARGET_COLUMNS:
		features[f"d_{base_name}"] = float(latest[base_name] - previous[base_name])

	for base_name in TARGET_COLUMNS:
		features[f"mean_{base_name}"] = float(window[base_name].mean())

	return features


def build_next_step_dataset(df: pd.DataFrame) -> tuple[pd.DataFrame, pd.DataFrame]:
	"""Create autoregressive dataset: history(t-7..t) -> state(t+1)."""
	if len(df) < HISTORY_STEPS + 1:
		raise RuntimeError(
			f"Need at least {HISTORY_STEPS + 1} samples to build dataset."
		)

	feature_rows: list[dict[str, float]] = []
	target_rows: list[dict[str, float]] = []
	base_df = df[TARGET_COLUMNS]

	for end_idx in range(HISTORY_STEPS - 1, len(base_df) - 1):
		window = base_df.iloc[end_idx - HISTORY_STEPS + 1 : end_idx + 1]
		next_state = base_df.iloc[end_idx + 1]

		feature_rows.append(make_feature_row(window))
		target_rows.append({name: float(next_state[name]) for name in TARGET_COLUMNS})

	x = pd.DataFrame(feature_rows, columns=FEATURE_COLUMNS)
	y = pd.DataFrame(target_rows, columns=TARGET_COLUMNS)
	return x, y


def train_and_evaluate_linear_regression(
	x: pd.DataFrame,
	y: pd.DataFrame,
) -> tuple[LinearRegression, pd.DataFrame, pd.DataFrame, pd.DataFrame, pd.DataFrame, pd.DataFrame, pd.DataFrame]:
	"""Train multi-output linear regression and return model + evaluation artifacts."""
	x_train, x_test, y_train, y_test = train_test_split(
		x,
		y,
		test_size=0.2,
		random_state=42,
		shuffle=True,
	)

	model = LinearRegression()
	model.fit(x_train, y_train)

	y_pred = pd.DataFrame(model.predict(x_test), columns=y.columns, index=y_test.index)

	metrics = []
	for column in y.columns:
		metrics.append(
			{
				"parameter": column,
				"r2": r2_score(y_test[column], y_pred[column]),
				"mae": mean_absolute_error(y_test[column], y_pred[column]),
				"rmse": root_mean_squared_error(y_test[column], y_pred[column]),
			}
		)

	metrics_df = pd.DataFrame(metrics)
	return model, x_train, x_test, y_train, y_test, y_pred, metrics_df


def print_c_export(model: LinearRegression) -> None:
	"""Print C-friendly coefficients for easy copy/paste."""
	print("\nC export (long-history autoregressive model)")
	print(f"#define WEATHER_MODEL_HISTORY_STEPS {HISTORY_STEPS}U")
	print(f"#define MODEL_FEATURE_COUNT {len(FEATURE_COLUMNS)}")
	print(f"#define MODEL_TARGET_COUNT {len(TARGET_COLUMNS)}")

	intercepts = ", ".join(f"{value:.15f}" for value in model.intercept_)
	print(f"static const double MODEL_INTERCEPTS[MODEL_TARGET_COUNT] = {{{intercepts}}};")

	print("static const double MODEL_COEFFICIENTS[MODEL_TARGET_COUNT][MODEL_FEATURE_COUNT] = {")
	for target_name, coef_row in zip(TARGET_COLUMNS, model.coef_):
		joined = ", ".join(f"{value:.15f}" for value in coef_row)
		print(f"\t/* {target_name} */ {{{joined}}},")
	print("};")

	print("\nFeature order for C:")
	for idx, name in enumerate(FEATURE_COLUMNS):
		print(f"{idx:2d}: {name}")


def main() -> None:
	# Example location: Warsaw, Poland
	latitude = 52.2297
	longitude = 21.0122

	# Use the last 180 days to have enough training examples.
	end = date.today() - timedelta(days=2)
	start = end - timedelta(days=180)

	print("Downloading open-source weather data from Open-Meteo...")
	df = fetch_open_meteo_hourly_data(latitude, longitude, start, end)

	print(f"Rows after cleaning: {len(df)}")
	print(f"Date range: {df['time'].min()} -> {df['time'].max()}")

	x, y = build_next_step_dataset(df)
	model, x_train, x_test, y_train, y_test, y_pred, metrics = train_and_evaluate_linear_regression(x, y)

	print(f"History steps: {HISTORY_STEPS}")
	print(f"Feature count: {len(FEATURE_COLUMNS)}")
	print(f"Train rows: {len(x_train)}")
	print(f"Test rows:  {len(x_test)}")

	print("\nEnhanced regression quality (next-step prediction)")
	for _, row in metrics.iterrows():
		print(
			f"- {row['parameter']:12s} | "
			f"R^2={row['r2']:.4f} "
			f"MAE={row['mae']:.4f} "
			f"RMSE={row['rmse']:.4f}"
		)

	preview = pd.DataFrame(
		{
			"actual_temperature": y_test["temperature"].head(5).values,
			"pred_temperature": y_pred["temperature"].head(5).values,
			"actual_humidity": y_test["humidity"].head(5).values,
			"pred_humidity": y_pred["humidity"].head(5).values,
			"actual_pressure": y_test["pressure"].head(5).values,
			"pred_pressure": y_pred["pressure"].head(5).values,
			"actual_light": y_test["light"].head(5).values,
			"pred_light": y_pred["light"].head(5).values,
		}
	)

	print("\nSample predictions (test set, first 5 rows)")
	print(preview.to_string(index=False, float_format=lambda v: f"{v:8.3f}"))

	print_c_export(model)


if __name__ == "__main__":
	main()
