#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <camlib.h>

#define SIZE 300000
int main() {
	struct PtpRuntime r;
	ptp_generic_init(&r);

	struct PtpDeviceInfo di;

	if (ptp_device_init(&r)) {
		puts("Device connection error");
		return 0;
	}

	ptp_open_session(&r);

	// Generate JSON data in packet buffer because I'm too lazy
	// to allocate a buffer (should be around 1-10kb of text)
	ptp_get_device_info(&r, &di);
	ptp_device_info_json(&di, (char*)r.data, r.data_length);
	printf("%s\n", (char*)r.data);

	ptp_close_session(&r);
	ptp_device_close(&r);

	free(r.data);

	return 0;
}

