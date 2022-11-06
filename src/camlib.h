// CamLib specific code, not PTP ISO standard definitions
#ifndef PTP_LIB_H
#define PTP_LIB_H

#include <stdint.h>

// Max timeout for response
#define PTP_TIMEOUT 1000

// Optional debug logging
#ifdef VERBOSE
	#define PTPLOG(...) printf(__VA_ARGS__);
#else
	#define PTPLOG(...) /* */
#endif

// Generic IO error, not PTP return codes
enum PtpGeneralError {
	PTP_OK = 0,
	PTP_NO_DEVICE = -1,
	PTP_NO_PERMISSION = -2,
	PTP_OPEN_FAIL = -3,
	PTP_OUT_OF_MEMORY = -4,
	PTP_IO_ERROR = -5,
};

enum PtpConnType {
	PTP_IP,
	PTP_USB,
};

// Holds vital lib info - passed to most functions
struct PtpRuntime {
	int transaction; // transaction ID
	int session; // session ID
    uint8_t *data;
    int data_length;
	int max_packet_size;
	int active_connection;
};

// Generic command structure - order of data isn't important.
// Meant to be a bridge to the other packet types.
struct PtpCommand {
	int code;

	uint32_t params[5];
	int param_length;

	int data_length;
};

// Helper packet reader functions
uint16_t ptp_read_uint16(void **dat);
uint16_t ptp_read_uint32(void **dat);
void ptp_read_string(void **dat, char *string, int max);
int ptp_read_uint16_array(void **dat, uint16_t *buf, int max);
int ptp_read_uint32_array(void **dat, uint16_t *buf, int max);
int ptp_wide_string(char *buffer, int max, char *input);

// Packet builder functions
// A command packet is sent first (cmd), followed by a data packet
int ptp_new_data_packet(struct PtpRuntime *r, struct PtpCommand *cmd);
int ptp_new_cmd_packet(struct PtpRuntime *r, struct PtpCommand *cmd);

// Gets return code directly from runtime data
int ptp_get_return_code(struct PtpRuntime *r);

// Packets start with a uint32 representing the length of the packet.
// In some cases, we want to append more data to the packet, so the length
// value must be updated
void ptp_update_data_length(struct PtpRuntime *r, int length);

// Used only for open session
void ptp_update_transaction(struct PtpRuntime *r, int t);

// Actual data packet returned by GetStorageIds
struct PtpStorageIds {
	uint32_t length;
	uint32_t data[8];
};

// To store unpacked device info data, after parsing
struct PtpDeviceInfo {
	uint16_t standard_version;
	uint32_t vendor_ext_id;
	uint16_t version;
	char extensions[128];
	uint16_t functional_mode;

	int ops_supported_length;
	uint16_t ops_supported[256];

	int events_supported_length;
	uint16_t events_supported[128];

	int props_supported_length;
	uint16_t props_supported[64];

	int capture_formats_length;
	uint16_t capture_formats[16];

	int playback_formats_length;
	uint16_t playback_formats[16];

	char manufacturer[128];
	char model[128];
	char device_version[64];
	char serial_number[128];
};

struct ObjectRequest {
	uint32_t storage_id; // left zero to get all
	uint32_t object_format;
	uint32_t object_handle;
	uint8_t all_storage_ids;
	uint8_t all_formats;
	uint8_t in_root;
};

int ptp_parse_device_info(struct PtpRuntime *r, struct PtpDeviceInfo *di);
int ptp_device_info_json(struct PtpDeviceInfo *di, char *buffer, int max);

#endif
