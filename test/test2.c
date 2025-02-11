#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <camlib.h>
#include <ptp.h>

int main() {
	struct PtpRuntime r;
	ptp_generic_init(&r);

	if (ptp_device_init(&r)) {
		puts("Device connection error");
		return 0;
	}

	ptp_open_session(&r);

	struct PtpDeviceInfo di;

	ptp_get_device_info(&r, &di);
	ptp_device_info_json(&di, (char*)r.data, r.data_length);
	puts((char *)r.data);

	char buffer[50000];

	ptp_eos_set_remote_mode(&r, 1);
	ptp_eos_set_event_mode(&r, 1);

	// Poll EOS events in a loop
	while (1) {
		ptp_eos_get_event(&r);
		ptp_dump(&r);
		ptp_eos_events_json(&r, buffer, 50000);
		puts(buffer);
		usleep(2000000);
	}

	ptp_close_session(&r);
	ptp_device_close(&r);

	free(r.data);

	return 0;
}
