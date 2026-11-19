#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <stdint.h>
#include <stdbool.h>

/**
 * WiFi SoftAP Hotspot & Web Configuration Server for PAD2PS3
 * Runs asynchronously on Core 0 with Bluetooth RF coexistence priority enabled.
 */

void web_server_init();
void web_server_print_status();
bool web_server_is_running();

// Action hook consumed by main loop for temporary PS button pulses triggered from Web UI
bool web_server_consume_ps_pulse(unsigned long& outDurationMs);

#endif // WEB_SERVER_H
