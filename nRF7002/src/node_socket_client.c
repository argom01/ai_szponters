#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/socket.h>
#include <zephyr/sys/printk.h>

#include "node_socket_client.h"
#include "data.h"

LOG_MODULE_REGISTER(node_socket_client, CONFIG_HTTP_SERVER_SAMPLE_LOG_LEVEL);

#if IS_ENABLED(CONFIG_APP_NODE_SOCKET_CLIENT)

#define NODE_SOCKET_CLIENT_PRIORITY K_PRIO_COOP(CONFIG_NUM_COOP_PRIORITIES - 2)

K_THREAD_STACK_DEFINE(node_socket_client_stack, CONFIG_APP_NODE_SOCKET_CLIENT_STACK_SIZE);
static struct k_thread node_socket_client_thread;
static bool node_socket_client_started;

static int send_all(int sock, const uint8_t *buf, size_t len)
{
	size_t sent = 0U;

	while (sent < len) {
		int ret = send(sock, buf + sent, len - sent, 0);

		if (ret <= 0) {
			return -errno;
		}

		sent += (size_t)ret;
	}

	return 0;
}

static void node_socket_client_thread_fn(void *arg1, void *arg2, void *arg3)
{
	ARG_UNUSED(arg1);
	ARG_UNUSED(arg2);
	ARG_UNUSED(arg3);

	while (true) {
		int sock;
		int ret;
		double tx_payload[4];
		char rx_buf[96] = {0};
		struct sensor_sample sample;
		struct sockaddr_in server = {
			.sin_family = AF_INET,
			.sin_port = htons(CONFIG_APP_NODE_SOCKET_SERVER_PORT),
		};
		struct zsock_timeval timeout = {
			.tv_sec = 4,
			.tv_usec = 0,
		};

		ret = zsock_inet_pton(AF_INET, CONFIG_APP_NODE_SOCKET_SERVER_IPV4,
					    &server.sin_addr);
		if (ret != 1) {
			LOG_ERR("Invalid IPv4 address: %s", CONFIG_APP_NODE_SOCKET_SERVER_IPV4);
			k_sleep(K_SECONDS(CONFIG_APP_NODE_SOCKET_CLIENT_INTERVAL_SEC));
			continue;
		}

		sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (sock < 0) {
			LOG_ERR("socket() failed: %d", -errno);
			k_sleep(K_SECONDS(CONFIG_APP_NODE_SOCKET_CLIENT_INTERVAL_SEC));
			continue;
		}

		(void)setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
		(void)setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

		ret = connect(sock, (struct sockaddr *)&server, sizeof(server));
		if (ret < 0) {
			LOG_WRN("connect(%s:%d) failed: %d",
				CONFIG_APP_NODE_SOCKET_SERVER_IPV4,
				CONFIG_APP_NODE_SOCKET_SERVER_PORT,
				-errno);
			(void)zsock_close(sock);
			k_sleep(K_SECONDS(CONFIG_APP_NODE_SOCKET_CLIENT_INTERVAL_SEC));
			continue;
		}

		if (!sensor_ringbuffer_get_latest(&sensor_data, 0U, &sample)) {
			LOG_WRN("No sensor sample available for TCP forward");
			(void)zsock_close(sock);
			k_sleep(K_SECONDS(CONFIG_APP_NODE_SOCKET_CLIENT_INTERVAL_SEC));
			continue;
		}

		tx_payload[0] = sample.temperature;
		tx_payload[1] = sample.humidity;
		tx_payload[2] = sample.pressure;
		tx_payload[3] = sample.light;

		ret = send_all(sock, (const uint8_t *)tx_payload, sizeof(tx_payload));
		if (ret < 0) {
			LOG_WRN("send(sensor doubles) failed: %d", -ret);
			(void)zsock_close(sock);
			k_sleep(K_SECONDS(CONFIG_APP_NODE_SOCKET_CLIENT_INTERVAL_SEC));
			continue;
		}

		LOG_INF("Forwarded sample T=%.2f H=%.2f P=%.2f L=%.2f",
			sample.temperature,
			sample.humidity,
			sample.pressure,
			sample.light);

		ret = recv(sock, rx_buf, sizeof(rx_buf) - 1, 0);
		if (ret < 0) {
			LOG_WRN("recv() failed: %d", -errno);
		} else {
			rx_buf[ret] = '\0';
			LOG_INF("Node ACK: %s", rx_buf);
		}

		(void)zsock_close(sock);
		k_sleep(K_SECONDS(CONFIG_APP_NODE_SOCKET_CLIENT_INTERVAL_SEC));
	}
}

void node_socket_client_start(void)
{
	if (node_socket_client_started) {
		return;
	}

	node_socket_client_started = true;

	(void)k_thread_create(&node_socket_client_thread,
				node_socket_client_stack,
				K_THREAD_STACK_SIZEOF(node_socket_client_stack),
				node_socket_client_thread_fn,
				NULL, NULL, NULL,
				NODE_SOCKET_CLIENT_PRIORITY,
				0, K_NO_WAIT);

	LOG_INF("Node socket client started: %s:%d", CONFIG_APP_NODE_SOCKET_SERVER_IPV4,
		CONFIG_APP_NODE_SOCKET_SERVER_PORT);
}

#else

void node_socket_client_start(void)
{
}

#endif /* CONFIG_APP_NODE_SOCKET_CLIENT */
