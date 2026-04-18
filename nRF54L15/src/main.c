#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>

/* Rejestrujemy moduł logowania, żeby widzieć napisy w terminalu */
LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

int main(void)
{
    /* 1. Pobieramy urządzenie z naszego Devicetree (z pliku .overlay) */
    const struct device *const bme280_dev = DEVICE_DT_GET_ANY(bosch_bme280);

    /* 2. Sprawdzamy, czy system widzi czujnik na ustawionych przez Ciebie pinach */
    if (bme280_dev == NULL || !device_is_ready(bme280_dev)) {
        LOG_ERR("Blad! Nie znaleziono czujnika BME280.");
        LOG_ERR("Sprawdz kabelki, zasilanie i wybrane piny P1.10 oraz P1.11!");
        return 0; /* Zatrzymujemy program, jeśli nie ma czujnika */
    }

    LOG_INF("Sukces! BME280 znaleziony. Rozpoczynam odczyt...");

    struct sensor_value temp, press, humidity;

    /* 3. Nieskończona pętla programu */
    while (1) {
        /* Każe czujnikowi wykonać pomiar */
        sensor_sample_fetch(bme280_dev);

        /* Pobiera wyniki z pamięci czujnika do naszych zmiennych */
        sensor_channel_get(bme280_dev, SENSOR_CHAN_AMBIENT_TEMP, &temp);
        sensor_channel_get(bme280_dev, SENSOR_CHAN_PRESS, &press);
        sensor_channel_get(bme280_dev, SENSOR_CHAN_HUMIDITY, &humidity);

        /* Wyświetla wyniki w konsoli */
        LOG_INF("Temperatura: %.2f C | Cisnienie: %.2f hPa | Wilgotnosc: %.2f %%",
                sensor_value_to_double(&temp),
                sensor_value_to_double(&press) * 10.0, /* Zephyr podaje ciśnienie w kPa, mnożymy x10 dla hPa */
                sensor_value_to_double(&humidity));

        /* Czekamy 2 sekundy przed kolejnym pomiarem */
        k_sleep(K_SECONDS(2));
    }
    
    return 0;
}