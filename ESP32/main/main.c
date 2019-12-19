/* Simple HTTP Server Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <esp_wifi.h>
#include <esp_event_loop.h>
#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <sys/param.h>

#include <esp_http_server.h>
#include <esp_ota_ops.h>
//#include <esp_flash_partitions.h>
//#include <esp_partition.h>
#include "Arduino.h"
//#include <TFT_eSPI.h>
//#include <SPI.h>
//#include <Button2.h>


/* A simple example that demonstrates how to create GET and POST
 * handlers for the web server.
 * The examples use simple WiFi configuration that you can set via
 * 'make menuconfig'.
 * If you'd rather not, just change the below entries to strings
 * with the config you want -
 * ie. #define EXAMPLE_WIFI_SSID "mywifissid"
*/
#define EXAMPLE_WIFI_SSID CONFIG_WIFI_SSID
#define EXAMPLE_WIFI_PASS CONFIG_WIFI_PASSWORD

static const char *TAG="APP";

//TFT_eSPI tft = TFT_eSPI(135, 240);

/* An HTTP GET handler */
esp_err_t hello_get_handler(httpd_req_t *req)
{
    char*  buf;
    size_t buf_len;

    /* Get header value string length and allocate memory for length + 1,
     * extra byte for null termination */
    buf_len = httpd_req_get_hdr_value_len(req, "Host") + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        /* Copy null terminated value string into buffer */
        if (httpd_req_get_hdr_value_str(req, "Host", buf, buf_len) == ESP_OK) {
            ESP_LOGI(TAG, "Found header => Host: %s", buf);
        }
        free(buf);
    }

    buf_len = httpd_req_get_hdr_value_len(req, "Test-Header-2") + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        if (httpd_req_get_hdr_value_str(req, "Test-Header-2", buf, buf_len) == ESP_OK) {
            ESP_LOGI(TAG, "Found header => Test-Header-2: %s", buf);
        }
        free(buf);
    }

    buf_len = httpd_req_get_hdr_value_len(req, "Test-Header-1") + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        if (httpd_req_get_hdr_value_str(req, "Test-Header-1", buf, buf_len) == ESP_OK) {
            ESP_LOGI(TAG, "Found header => Test-Header-1: %s", buf);
        }
        free(buf);
    }

    /* Read URL query string length and allocate memory for length + 1,
     * extra byte for null termination */
    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            ESP_LOGI(TAG, "Found URL query => %s", buf);
            char param[32];
            /* Get value of expected key from query string */
            if (httpd_query_key_value(buf, "query1", param, sizeof(param)) == ESP_OK) {
                ESP_LOGI(TAG, "Found URL query parameter => query1=%s", param);
            }
            if (httpd_query_key_value(buf, "query3", param, sizeof(param)) == ESP_OK) {
                ESP_LOGI(TAG, "Found URL query parameter => query3=%s", param);
            }
            if (httpd_query_key_value(buf, "query2", param, sizeof(param)) == ESP_OK) {
                ESP_LOGI(TAG, "Found URL query parameter => query2=%s", param);
            }
        }
        free(buf);
    }

    /* Set some custom headers */
    httpd_resp_set_hdr(req, "Custom-Header-1", "Custom-Value-1");
    httpd_resp_set_hdr(req, "Custom-Header-2", "Custom-Value-2");

    /* Send response with custom headers and body set as the
     * string passed in user context*/
    const char* resp_str = (const char*) req->user_ctx;
    httpd_resp_send(req, resp_str, strlen(resp_str));

    /* After sending the HTTP response the old HTTP request
     * headers are lost. Check if HTTP request headers can be read now. */
    if (httpd_req_get_hdr_value_len(req, "Host") == 0) {
        ESP_LOGI(TAG, "Request headers lost");
    }
    return ESP_OK;
}

httpd_uri_t hello = {
    .uri       = "/hello",
    .method    = HTTP_GET,
    .handler   = hello_get_handler,
    /* Let's pass response string in user
     * context to demonstrate it's usage */
    .user_ctx  = "Hello World!"
};

/* An HTTP POST handler */
esp_err_t echo_post_handler(httpd_req_t *req)
{
    char buf[100];
    int ret, remaining = req->content_len;

    while (remaining > 0) {
        /* Read the data for the request */
        if ((ret = httpd_req_recv(req, buf,
                        MIN(remaining, sizeof(buf)))) <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                /* Retry receiving if timeout occurred */
                continue;
            }
            return ESP_FAIL;
        }

        /* Send back the same data */
        httpd_resp_send_chunk(req, buf, ret);
        remaining -= ret;

        /* Log data received */
        ESP_LOGI(TAG, "=========== RECEIVED DATA ==========");
        ESP_LOGI(TAG, "%.*s", ret, buf);
        ESP_LOGI(TAG, "====================================");
    }

    // End response
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

httpd_uri_t echo = {
    .uri       = "/echo",
    .method    = HTTP_POST,
    .handler   = echo_post_handler,
    .user_ctx  = NULL
};

/* An HTTP PUT handler. This demonstrates realtime
 * registration and deregistration of URI handlers
 */
esp_err_t ctrl_put_handler(httpd_req_t *req)
{
    char buf;
    int ret;

    if ((ret = httpd_req_recv(req, &buf, 1)) <= 0) {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            httpd_resp_send_408(req);
        }
        return ESP_FAIL;
    }

    if (buf == '0') {
        /* Handler can be unregistered using the uri string */
        ESP_LOGI(TAG, "Unregistering /hello and /echo URIs");
        httpd_unregister_uri(req->handle, "/hello");
        httpd_unregister_uri(req->handle, "/echo");
    }
    else {
        ESP_LOGI(TAG, "Registering /hello and /echo URIs");
        httpd_register_uri_handler(req->handle, &hello);
        httpd_register_uri_handler(req->handle, &echo);
    }

    /* Respond with empty body */
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

httpd_uri_t ctrl = {
    .uri       = "/ctrl",
    .method    = HTTP_PUT,
    .handler   = ctrl_put_handler,
    .user_ctx  = NULL
};
// =============== FIRMWARE UPDATE ==================
static char ota_write_data[1024 + 1];
esp_err_t firmware_post_handler(httpd_req_t *req) {
	int ret, remaining = req->content_len;
    int binary_size = remaining;
	int tempoffs = 0;
	
	const esp_partition_t *configured = esp_ota_get_boot_partition();
	const esp_partition_t *running = esp_ota_get_running_partition();
	const esp_partition_t *update = esp_ota_get_next_update_partition(NULL);
	esp_ota_handle_t update_handle = 0;
	esp_app_desc_t new_app_info;
	
	bool image_header_was_checked = false;
	
	ESP_LOGI(TAG, "Running partition: %s at 0x%08x", running->label, running->address);
	ESP_LOGI(TAG, "Configured partition: %s at 0x%08x", configured->label, configured->address);
	
	ESP_LOGI(TAG, "Writing to partition subtype %d at offset 0x%x",
		update->subtype,
		update->address);
	assert(update != NULL);	
	
	ESP_LOGI(TAG, "Payload size: %d bytes", req->content_len);

	while (remaining > 0) {
		/* Read the data for the request */
		if ((ret = httpd_req_recv(req, ota_write_data + tempoffs, MIN(remaining, sizeof(ota_write_data) - tempoffs - 1))) <= 0) {
			if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
				/* Retry receiving if timeout occurred */
				continue;
			}
			return ESP_FAIL;
		} else if (ret > 0) {
//			ESP_LOGI(TAG, "Received %i bytes", ret);
//			tempoffs += ret;
			if (image_header_was_checked == false) {
				if (ret > sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t) + sizeof(esp_app_desc_t)) {
					// check current version with downloading
					memcpy(&new_app_info, &ota_write_data[sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t)], sizeof(esp_app_desc_t));
					ESP_LOGI(TAG, "New firmware version: %s", new_app_info.version);
					ESP_LOGI(TAG, "Magic: %#010x", new_app_info.magic_word);
					ESP_LOGI(TAG, "App version: %s", new_app_info.version);
					
					esp_app_desc_t running_app_info;
					if (esp_ota_get_partition_description(running, &running_app_info) == ESP_OK) {
						ESP_LOGI(TAG, "Running firmware version: %s", running_app_info.version);
					}

					const esp_partition_t* last_invalid_app = esp_ota_get_last_invalid_partition();
					esp_app_desc_t invalid_app_info;
					if (esp_ota_get_partition_description(last_invalid_app, &invalid_app_info) == ESP_OK) {
						ESP_LOGI(TAG, "Last invalid firmware version: %s", invalid_app_info.version);
					}

					// check current version with last invalid partition
					if(last_invalid_app != NULL) {
						if (memcmp(invalid_app_info.version, new_app_info.version, sizeof(new_app_info.version)) == 0) {
							ESP_LOGW(TAG, "New version is the same as invalid version.");
							ESP_LOGW(TAG, "Previously, there was an attempt to launch the firmware with %s version, but it failed.", invalid_app_info.version);
							ESP_LOGW(TAG, "The firmware has been rolled back to the previous version.");
							// afbreken
                        	httpd_resp_send_chunk(req, "New version is the same as invalid version. Not updating.", HTTPD_RESP_USE_STRLEN);	
							httpd_resp_send_chunk(req, NULL, 0);
							return ESP_FAIL;
						}
					}

					if (memcmp(new_app_info.app_elf_sha256, running_app_info.app_elf_sha256, sizeof(new_app_info.app_elf_sha256)) == 0) {
						ESP_LOGW(TAG, "Current running version is the same as a new. We will not continue the update.");
						// afbreken
                    	httpd_resp_send_chunk(req, "App version matches existing. Not updating.", HTTPD_RESP_USE_STRLEN);	
						httpd_resp_send_chunk(req, NULL, 0);
						return ESP_FAIL;
					}

					image_header_was_checked = true;

					esp_err_t err = esp_ota_begin(update, OTA_SIZE_UNKNOWN, &update_handle);
					if (err != ESP_OK) {
						ESP_LOGE(TAG, "esp_ota_begin failed (%s)", esp_err_to_name(err));
						//task_fatal_error();
						// afbreken
						return ESP_FAIL;
					}
					ESP_LOGI(TAG, "esp_ota_begin succeeded");
				}
				else {
					ESP_LOGE(TAG, "received package is not correct length - expected %i, got %i bytes", sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t) + sizeof(esp_app_desc_t), ret);
					//task_fatal_error();
					// afbreken
					return ESP_FAIL;
				}				
			}
/*			else {
				ret += tempoffs;
				tempoffs = 0;
			}
*/			esp_err_t err = esp_ota_write(update_handle, (const void *)ota_write_data, ret);
			if (err != ESP_OK) {
				ESP_LOGI(TAG, "Write to OTA flash failed");
				return ESP_FAIL;
			}

		}
		remaining -= ret;

		/* Log data received */
		ESP_LOGI(TAG, "Written image length %d", binary_size - remaining);
	}

	// End response
	esp_err_t err = esp_ota_end(update_handle);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "esp_ota_end failed (%s)!", esp_err_to_name(err));
		return ESP_FAIL;
	}
	httpd_resp_send_chunk(req, "Flashed file successfully!", HTTPD_RESP_USE_STRLEN);
	httpd_resp_send_chunk(req, NULL, 0);
	
	err = esp_ota_set_boot_partition(update);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "esp_ota_set_boot_partition failed (%s)!", esp_err_to_name(err));
		return ESP_FAIL;
	}
	ESP_LOGI(TAG, "Prepare to restart system!");
	esp_restart();

	return ESP_OK;	
}

httpd_uri_t firmware = {
	.uri        = "/firmware",
	.method     = HTTP_POST,
	.handler    = firmware_post_handler,
	.user_ctx   = NULL
};

// ==================================================

httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Set URI handlers
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &hello);
        httpd_register_uri_handler(server, &echo);
        httpd_register_uri_handler(server, &ctrl);
	    httpd_register_uri_handler(server, &firmware);
        return server;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return NULL;
}

void stop_webserver(httpd_handle_t server)
{
    // Stop the httpd server
    httpd_stop(server);
}

static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    httpd_handle_t *server = (httpd_handle_t *) ctx;

    switch(event->event_id) {
    case SYSTEM_EVENT_STA_START:
        ESP_LOGI(TAG, "SYSTEM_EVENT_STA_START");
        ESP_ERROR_CHECK(esp_wifi_connect());
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        ESP_LOGI(TAG, "SYSTEM_EVENT_STA_GOT_IP");
        ESP_LOGI(TAG, "Got IP: '%s'",
                ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));

        /* Start the web server */
        if (*server == NULL) {
            *server = start_webserver();
        }
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        ESP_LOGI(TAG, "SYSTEM_EVENT_STA_DISCONNECTED");
        ESP_ERROR_CHECK(esp_wifi_connect());

        /* Stop the web server */
        if (*server) {
            stop_webserver(*server);
            *server = NULL;
        }
        break;
    default:
        break;
    }
    return ESP_OK;
}

static void initialise_wifi(void *arg)
{
    tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_init(event_handler, arg));
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = EXAMPLE_WIFI_SSID,
            .password = EXAMPLE_WIFI_PASS,
        },
    };
    ESP_LOGI(TAG, "Setting WiFi configuration SSID %s...", wifi_config.sta.ssid);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

void lcd_init() {
   	
}

void app_main()
{
    static httpd_handle_t server = NULL;
	
	initArduino();
	ESP_LOGI(TAG, "Hallo");

    ESP_ERROR_CHECK(nvs_flash_init());
    initialise_wifi(&server);
}
