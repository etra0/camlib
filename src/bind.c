// Generic text bindings to PTP functions
// - The only function that is exposed is bind_run(), which returns
// valid JSON from a generic text like request
// - This is not part of the core library, will use malloc()
// Copyright 2022 by Daniel C (https://github.com/petabyt/camlib)

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <ptp.h>
#include <camlib.h>
#include <operations.h>
#include <backend.h>
#include <enum.h>

#define BIND_MAX_PARAM 32

static int connected = 0;
static int initialized = 0;

struct BindResp {
	char *buffer;
	int params[BIND_MAX_PARAM];
	int params_length;
	int max;
	char string[32];
	int string_length;
};

struct RouteMap {
	char *name;
	int (*call)(struct BindResp *, struct PtpRuntime *);
};

int bind_status(struct BindResp *bind, struct PtpRuntime *r) {
	return sprintf(bind->buffer, "{\"error\": 0, \"initialized\": %d, \"connected\": %d}", initialized, connected);
}

int bind_init(struct BindResp *bind, struct PtpRuntime *r) {
	if (initialized) {
		free(r->data);
		if (r->di != NULL) free(r->di);
	}

	memset(r, 0, sizeof(struct PtpRuntime));
	r->data = malloc(CAMLIB_DEFAULT_SIZE);
	r->data_length = CAMLIB_DEFAULT_SIZE;
	r->di = NULL;

	if (connected) {
		ptp_close_session(r);
		//ptp_device_close(r);
		connected = 0;
	}	
	return sprintf(bind->buffer, "{\"error\": %d}", 0);
}

int bind_connect(struct BindResp *bind, struct PtpRuntime *r) {
	// Check if uninitialized
	if (r->data_length != CAMLIB_DEFAULT_SIZE) {
		return sprintf(bind->buffer, "{\"error\": %d}", PTP_OUT_OF_MEM);
	}

	r->transaction = 0;
	r->session = 0;

	int x = ptp_device_init(r);
	if (!x) connected = 1;
	return sprintf(bind->buffer, "{\"error\": %d}", x);
}

int bind_disconnect(struct BindResp *bind, struct PtpRuntime *r) {
	int x = ptp_device_close(r);
	if (!x) connected = 0;
	return sprintf(bind->buffer, "{\"error\": %d}", x);
}

int bind_open_session(struct BindResp *bind, struct PtpRuntime *r) {
	int x = ptp_open_session(r);
	return sprintf(bind->buffer, "{\"error\": %d}", x);
}

int bind_close_session(struct BindResp *bind, struct PtpRuntime *r) {
	int x = ptp_close_session(r);
	return sprintf(bind->buffer, "{\"error\": %d}", x);
}

int bind_get_device_info(struct BindResp *bind, struct PtpRuntime *r) {
	if (r->di == NULL) {
		r->di = malloc(sizeof(struct PtpDeviceInfo));
	}

	int x = ptp_get_device_info(r, r->di);
	if (x) {
		return sprintf(bind->buffer, "{\"error\": %d}", x);
	}

	ptp_device_info_json(r->di, (char*)r->data, r->data_length);
	return snprintf(bind->buffer, bind->max, "{\"error\": %d, \"resp\": %s}", x, (char*)r->data);
}

int bind_get_storage_ids(struct BindResp *bind, struct PtpRuntime *r) {
	struct UintArray *arr;
	int x = ptp_get_storage_ids(r, &arr);
	if (x) return sprintf(bind->buffer, "{\"error\": %d}", x);

	int len = sprintf(bind->buffer, "{\"error\": %d, \"resp\": [", x);
	for (int i = 0; i < arr->length; i++) {
		char *comma = "";
		if (i) comma = ",";
		len += sprintf(bind->buffer + len, "%s%u", comma, arr->data[i]);
	}

	len += sprintf(bind->buffer + len, "]}");
	return len;
}

int bind_get_storage_info(struct BindResp *bind, struct PtpRuntime *r) {
	struct PtpStorageInfo so;
	int x = ptp_get_storage_info(r, bind->params[0], &so);
	if (x) return sprintf(bind->buffer, "{\"error\": %d}", x);

	int len = sprintf(bind->buffer, "{\"error\": %d, \"resp\": {", x);
	len += sprintf(bind->buffer + len, "\"storage_type\": %u,", so.storage_type);
	len += sprintf(bind->buffer + len, "\"fs_type\": %u,", so.fs_type);
	len += sprintf(bind->buffer + len, "\"max_capacity\": %lu,", so.max_capacity);
	len += sprintf(bind->buffer + len, "\"free_space\": %lu", so.free_space);

	len += sprintf(bind->buffer + len, "}}");
	return len;
}

int bind_get_object_handles(struct BindResp *bind, struct PtpRuntime *r) {
	struct UintArray *arr;	
	printf("IN root: %d\n", bind->params[1]);
	int x = ptp_get_object_handles(r, bind->params[0], 0, bind->params[1], &arr);
	if (x) return sprintf(bind->buffer, "{\"error\": %d}", x);

	int len = sprintf(bind->buffer, "{\"error\": %d, \"resp\": [", x);
	for (int i = 0; i < arr->length; i++) {
		char *comma = "";
		if (i) comma = ",";
		len += sprintf(bind->buffer + len, "%s%u", comma, arr->data[i]);
	}

	len += sprintf(bind->buffer + len, "]}");
	return len;
}

int bind_get_object_info(struct BindResp *bind, struct PtpRuntime *r) {
	struct PtpObjectInfo oi;
	int x = ptp_get_object_info(r, bind->params[0], &oi);
	if (x) return sprintf(bind->buffer, "{\"error\": %d}", x);

	int len = sprintf(bind->buffer, "{\"error\": %d, \"resp\": ", x);

	len += ptp_object_info_json(&oi, bind->buffer + len, 1000);

	len += sprintf(bind->buffer + len, "}");
	return len;
}

int bind_custom_cmd(struct BindResp *bind, struct PtpRuntime *r) {
	struct PtpCommand cmd;
	cmd.code = PTP_OC_CloseSession;
	cmd.param_length = bind->params_length - 1;
	for (int i = 0; i < cmd.param_length; i++) {
		cmd.params[i] = bind->params[i];
	}

	int x = ptp_generic_send(r, &cmd);
	return sprintf(bind->buffer, "{\"error\": %d, \"resp\": %X}", x, ptp_get_return_code(r));
}

int bind_drive_lens(struct BindResp *bind, struct PtpRuntime *r) {
	printf("Drive lens %d\n", bind->params[0]);
	int x = ptp_eos_drive_lens(r, bind->params[0]);
	return sprintf(bind->buffer, "{\"error\": %d}", x);
}

int bind_get_liveview_frame(struct BindResp *bind, struct PtpRuntime *r) {
	char *lv = malloc(ptp_liveview_size(r));
	int x = ptp_liveview_frame(r, lv);

	int err = x;
	if (x > 0) {
		err = 0;
	}

	char *inc = bind->buffer + sprintf(bind->buffer, "{\"error\": %d, \"resp\": [", err);

	for (int i = 0; i < x; i++) {
		if (inc - bind->buffer >= bind->max) {
			return 0;
		}

		if (i > x - 2) {
			inc += sprintf(inc, "%u", (uint8_t)lv[i]);
		} else {
			inc += sprintf(inc, "%u,", (uint8_t)lv[i]);
		}
	}

	inc += sprintf(inc, "]}");

	free(lv);
	return inc - bind->buffer;
}

int bind_set_property(struct BindResp *bind, struct PtpRuntime *r) {
	int dev = ptp_device_type(r);
	int x = 0;

	if (bind->string_length == 0) {
		if (ptp_check_prop(r, bind->params[0])) {
			x = ptp_set_prop_value(r, bind->params[0], bind->params[1]);
		} else {
			x = ptp_eos_set_prop_value(r, bind->params[0], bind->params[1]);
		}
		return sprintf(bind->buffer, "{\"error\": %d}", x);
	}

	int value = bind->params[0];

	if (!strcmp(bind->string, "aperture")) {
		if (dev == PTP_DEV_EOS) {
			x = ptp_set_prop_value(r, PTP_PC_EOS_Aperture, ptp_eos_get_aperture(value, 1));
		}
	} else 	if (!strcmp(bind->string, "iso")) {
		if (dev == PTP_DEV_EOS) {
			x = ptp_set_prop_value(r, PTP_PC_EOS_ISOSpeed, ptp_eos_get_iso(value, 1));
		}
	} else 	if (!strcmp(bind->string, "shutter speed")) {
		if (dev == PTP_DEV_EOS) {
			x = ptp_set_prop_value(r, PTP_PC_EOS_ShutterSpeed, ptp_eos_get_shutter(value, 1));
		}
	} else 	if (!strcmp(bind->string, "image format")) {
		if (dev == PTP_DEV_EOS) {
			x = ptp_set_prop_value(r, PTP_PC_EOS_ImageFormat, ptp_eos_get_imgformat(value, 1));
		}
	} else {
		return sprintf(bind->buffer, "{\"error\": %d}", PTP_CAM_ERR);
	}

	return sprintf(bind->buffer, "{\"error\": %d}", x);
}

int bind_get_events(struct BindResp *bind, struct PtpRuntime *r) {
	int dev = ptp_device_type(r);

	// TODO:
	//struct PtpEventContainer ec;
	//ptp_get_event(r, &ec);

	if (dev == PTP_DEV_EOS) {
		int x = ptp_eos_get_event(r);
		if (x) return sprintf(bind->buffer, "{\"error\": %d}", x);

		int len = snprintf(bind->buffer, bind->max, "{\"error\": 0, \"resp\": ");
		len += ptp_eos_events_json(r, bind->buffer + len, bind->max - len);
		
		len += snprintf(bind->buffer + len, bind->max - len, "}");

		return len;
	}

	return sprintf(bind->buffer, "{\"error\": %d}", 0);
}

int bind_get_liveview_type(struct BindResp *bind, struct PtpRuntime *r) {
	return sprintf(bind->buffer, "{\"error\": %d, \"resp\": %d}", 0, ptp_liveview_type(r));
}

int bind_get_liveview_frame_jpg(struct BindResp *bind, struct PtpRuntime *r) {
	int x = ptp_liveview_frame(r, bind->buffer);

	if (x < 0) {
		return 0;
	}

	if (x > bind->max) {
		return 0;
	}

	return x;
}

int bind_liveview_init(struct BindResp *bind, struct PtpRuntime *r) {
	return sprintf(bind->buffer, "{\"error\": %d}", ptp_liveview_init(r));
}

int bind_liveview_deinit(struct BindResp *bind, struct PtpRuntime *r) {
	return sprintf(bind->buffer, "{\"error\": %d}", ptp_liveview_deinit(r));
}

int bind_get_device_type(struct BindResp *bind, struct PtpRuntime *r) {
	return sprintf(bind->buffer, "{\"error\": %d, \"resp\": %d}", 0, ptp_device_type(r));
}

int bind_eos_set_remote_mode(struct BindResp *bind, struct PtpRuntime *r) {
	return sprintf(bind->buffer, "{\"error\": %d}", ptp_eos_set_remote_mode(r, bind->params[0]));
}

int bind_eos_set_event_mode(struct BindResp *bind, struct PtpRuntime *r) {
	return sprintf(bind->buffer, "{\"error\": %d}", ptp_eos_set_event_mode(r, bind->params[0]));
}

int bind_hello_world(struct BindResp *bind, struct PtpRuntime *r) {
	return sprintf(bind->buffer, "\"COol beans %d (%d)\"", bind->params[0], bind->params_length);
}

int bind_get_enums(struct BindResp *bind, struct PtpRuntime *r) {
	int x = sprintf(bind->buffer, "{\"error\": %d, \"resp\": [", 0);
	char *comma = "";
	for (int i = 0; i < ptp_enums_length; i++) {
		x += sprintf(bind->buffer + x, "%s{\"type\": %d, \"vendor\": %d, \"name\": \"%s\", \"value\": %d}",
			comma, ptp_enums[i].type, ptp_enums[i].vendor, ptp_enums[i].name, ptp_enums[i].value);
		comma = ",";
	}

	x += sprintf(bind->buffer + x, "]}");
	return x;
}

int bind_get_status(struct BindResp *bind, struct PtpRuntime *r) {
	return sprintf(bind->buffer, "{\"error\": 0, \"connected\": %d}", connected);
}

int bind_shutter_half_press(struct BindResp *bind, struct PtpRuntime *r) {
	int x = 0;
	if (ptp_device_type(r) == PTP_DEV_EOS) {
		x = ptp_eos_remote_release_on(r, 1);
	}

	return sprintf(bind->buffer, "{\"error\": %d}", x);
}

int bind_take_picture(struct BindResp *bind, struct PtpRuntime *r) {
	int x = 0;
	if (ptp_check_opcode(r, PTP_OC_InitiateCapture)) {
		x = ptp_init_capture(r, 0, 0);
	} else if (ptp_device_type(r) == PTP_DEV_EOS) {
		x |= ptp_eos_remote_release_on(r, 2);
		x |= ptp_eos_remote_release_off(r, 2);
		x |= ptp_eos_remote_release_off(r, 1);
	} else {
		x = PTP_UNSUPPORTED;
	}

	return sprintf(bind->buffer, "{\"error\": %d}", x);
}

int bind_bulb_start(struct BindResp *bind, struct PtpRuntime *r) {
	int x = 0;
	if (ptp_device_type(r) == PTP_DEV_EOS) {
		x = ptp_eos_remote_release_on(r, 1);
		x |= ptp_eos_remote_release_on(r, 2);
	} else {
		x = PTP_UNSUPPORTED;
	}

	return sprintf(bind->buffer, "{\"error\": %d}", x);
}

int bind_bulb_stop(struct BindResp *bind, struct PtpRuntime *r) {
	int x = 0;
	if (ptp_device_type(r) == PTP_DEV_EOS) {
		x = ptp_eos_remote_release_off(r, 2);
		x |= ptp_eos_remote_release_off(r, 1);

	} else {
		x = PTP_UNSUPPORTED;
	}

	return sprintf(bind->buffer, "{\"error\": %d}", x);
}

int bind_mirror_up(struct BindResp *bind, struct PtpRuntime *r) {
	int x = 0;
	if (ptp_device_type(r) == PTP_DEV_EOS) {
		x = ptp_eos_set_prop_value(r, PTP_PC_EOS_VF_Output, 3);
	} else {
		x = PTP_UNSUPPORTED;
	}

	return sprintf(bind->buffer, "{\"error\": %d}", x);
}

int bind_mirror_down(struct BindResp *bind, struct PtpRuntime *r) {
	int x = 0;
	if (ptp_device_type(r) == PTP_DEV_EOS) {
		x = ptp_eos_set_prop_value(r, PTP_PC_EOS_VF_Output, 0);
	} else {
		x = PTP_UNSUPPORTED;
	}

	return sprintf(bind->buffer, "{\"error\": %d}", x);
}

int bind_get_return_code(struct BindResp *bind, struct PtpRuntime *r) {
	return sprintf(bind->buffer, "{\"error\": 0, \"code\": %d}", ptp_get_return_code(r));
}

int bind_reset(struct BindResp *bind, struct PtpRuntime *r) {
	return sprintf(bind->buffer, "{\"error\": %d}", ptp_device_reset(r));
}

struct RouteMap routes[] = {
	{"ptp_hello_world", bind_hello_world},
	{"ptp_status", bind_status},
	{"ptp_reset", bind_reset},
	{"ptp_init", bind_init},
	{"ptp_connect", bind_connect},
	{"ptp_disconnect", bind_disconnect},
	{"ptp_open_session", bind_open_session},
	{"ptp_close_session", bind_close_session},
	{"ptp_bulb_start", bind_bulb_start},
	{"ptp_bulb_stop", bind_bulb_stop},
	{"ptp_mirror_up", bind_mirror_up},
	{"ptp_mirror_down", bind_mirror_down},
	{"ptp_get_device_info", bind_get_device_info},
	{"ptp_drive_lens", bind_drive_lens},
	{"ptp_get_liveview_frame", bind_get_liveview_frame},
	{"ptp_get_liveview_type", bind_get_liveview_type},
	{"ptp_get_liveview_frame.jpg", bind_get_liveview_frame_jpg},
	{"ptp_liveview_init", bind_liveview_init},
	{"ptp_liveview_deinit", bind_liveview_deinit},
	{"ptp_get_device_type", bind_get_device_type},
	{"ptp_get_events", bind_get_events},
	{"ptp_set_property", bind_set_property},
	{"ptp_eos_set_remote_mode", bind_eos_set_remote_mode},
	{"ptp_eos_set_event_mode", bind_eos_set_event_mode},
	{"ptp_get_enums", bind_get_enums},
	{"ptp_get_status", bind_get_status},
	{"ptp_shutter_half_press", bind_shutter_half_press},
	{"ptp_take_picture", bind_take_picture},
	{"ptp_get_return_code", bind_get_return_code},
	{"ptp_get_storage_ids", bind_get_storage_ids},
	{"ptp_get_storage_info", bind_get_storage_info},
	{"ptp_get_object_handles", bind_get_object_handles},
	{"ptp_get_object_info", bind_get_object_info},
//	{"ptp_custom_send", NULL},
//	{"ptp_custom_cmd", NULL},
};

static void parseParameters(struct BindResp *bind, char *base) {
	while (*base != ';') {
		// Parse string at any place into .string
		if (*base == '"') {
			base++;
			while (*base != '"') {
				bind->string[bind->string_length] = *base;
				base++;
				bind->string_length++;
				if (bind->string_length > 31) break;
			}
			bind->string[bind->string_length] = '\0';
			base++;
		} else {
			bind->params[bind->params_length] = strtol(base, &base, 0);
			bind->params_length++;
		}
		if (*base != ',') break;
		base++;
	}
	
}

// See DOCS.md for documentation
int bind_run(struct PtpRuntime *r, char *req, char *buffer, int max) {
	if (buffer == NULL) {
		return -1;
	}

	struct BindResp bind;
	memset(&bind, 0, sizeof(struct BindResp));
	bind.params_length = 0;
	bind.string_length = 0;
	bind.buffer = buffer;
	bind.max = max;

	// Primitive parameter parsing - See DOCS.md
	for (int i = 0; req[i] != '\0'; i++) {
		if (req[i] == ';') {
			req[i] = '\0'; // tear off for strcmp
			i++;
			char *base = req + i;
			parseParameters(&bind, base);
			break;
		}
	}

	for (int i = 0; i < (int)(sizeof(routes) / sizeof(struct RouteMap)); i++) {
		if (!strcmp(routes[i].name, req)) {
			return routes[i].call(&bind, r);
		}
	}

	return -1;
}
