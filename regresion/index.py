"""Enhanced next-step regression on open weather data.

Model objective:
- predict next sample (t+1) for:
	- temperature
	- humidity
	- pressure
	- light (shortwave radiation)
- using an autoregressive feature set built from previous (t-1)
  and current (t) samples

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
FEATURE_COLUMNS = [
	"temperature",
	"humidity",
	"pressure",
	"light",
	"d_temperature",
	"d_humidity",
	"d_pressure",
	"d_light",
	"temperature_x_humidity",
	"temperature_x_light",
	"humidity_x_light",
	"pressure_x_d_pressure",
	"temperature_x_d_temperature",
	"humidity_x_d_humidity",
]


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


def build_next_step_dataset(df: pd.DataFrame) -> tuple[pd.DataFrame, pd.DataFrame]:
	"""Create autoregressive dataset: [state(t), trend(t)] -> state(t+1)."""
	if len(df) < 3:
		raise RuntimeError("Need at least 3 samples to build autoregressive features.")

	prev = df[TARGET_COLUMNS].iloc[:-2].reset_index(drop=True)
	curr = df[TARGET_COLUMNS].iloc[1:-1].reset_index(drop=True)
	next_state = df[TARGET_COLUMNS].iloc[2:].reset_index(drop=True)
	delta = curr - prev

	x = pd.DataFrame(
		{
			"temperature": curr["temperature"],
			"humidity": curr["humidity"],
			"pressure": curr["pressure"],
			"light": curr["light"],
			"d_temperature": delta["temperature"],
			"d_humidity": delta["humidity"],
			"d_pressure": delta["pressure"],
			"d_light": delta["light"],
			"temperature_x_humidity": curr["temperature"] * curr["humidity"],
			"temperature_x_light": curr["temperature"] * curr["light"],
			"humidity_x_light": curr["humidity"] * curr["light"],
			"pressure_x_d_pressure": curr["pressure"] * delta["pressure"],
			"temperature_x_d_temperature": curr["temperature"] * delta["temperature"],
			"humidity_x_d_humidity": curr["humidity"] * delta["humidity"],
		}
	)

	y = next_state.copy()
	return x, y


def train_and_evaluate_linear_regression(
	x: pd.DataFrame,
	y: pd.DataFrame,
) -> tuple[LinearRegression, pd.DataFrame, pd.DataFrame, pd.DataFrame, pd.DataFrame, pd.DataFrame]:
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


def print_c_export(model: LinearRegression, feature_columns: list[str], target_columns: list[str]) -> None:
	"""Print C-friendly coefficients for easy copy/paste."""
	print("\nC export (enhanced autoregressive model)")
	print(f"#define MODEL_FEATURE_COUNT {len(feature_columns)}")
	print(f"#define MODEL_TARGET_COUNT {len(target_columns)}")

	intercepts = ", ".join(f"{value:.15f}" for value in model.intercept_)
	print(f"static const double MODEL_INTERCEPTS[MODEL_TARGET_COUNT] = {{{intercepts}}};")

	print("static const double MODEL_COEFFICIENTS[MODEL_TARGET_COUNT][MODEL_FEATURE_COUNT] = {")
	for target_name, coef_row in zip(target_columns, model.coef_):
		joined = ", ".join(f"{value:.15f}" for value in coef_row)
		print(f"\t/* {target_name} */ {{{joined}}},")
	print("};")

	print("\nFeature order for C:")
	for idx, name in enumerate(feature_columns):
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

	print_c_export(model, FEATURE_COLUMNS, TARGET_COLUMNS)


if __name__ == "__main__":
	main()
