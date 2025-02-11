# camlib
This is a portable PTP/USB library written in C99. This isn't a fork of gphoto2, libptp, or libmtp.  
This is a complete rewrite from the ground up, and corrects some mistakes made in the design of older libraries.  

This library is written primarily for my [CamControl](https://camcontrol.danielc.dev/) Android app.

## Design
- Data parsing, packet building, and packet sending/recieving is all done in a single buffer
- Core library will perform next no memory allocation, to avoid memory leaks
- No platform specific code at the core
- No macros, only clean C API - everything is a function that can be accessed from other languages

## Checklist
- [x] Working Linux, Android, and Windows backends
- [x] Bindings to other languages
- [x] Real time camera previews (EOS)
- [x] Complete most EOS/Canon vendor OCs
- [x] Take pictures, perform bulb, set properties, get properties, poll events
- [x] Basic filesystem functionality
- [x] Finish basic Canon functions
- [x] PTP/IP Implementation
- [ ] Dummy reciever (a virtual camera for test communication, fuzz testing)
- [ ] Basic Nikon, Sony, Fuji support

## Sample
```
#include <camlib.h>
...
struct PtpRuntime r;
ptp_generic_init(&r);

if (ptp_device_init(&r)) {
	puts("Device connection error");
	return 0;
}

struct PtpDeviceInfo di;

ptp_get_device_info(&r, &di);
ptp_device_info_json(&di, (char*)r.data, r.data_length);
printf("%s\n", (char*)r.data);

ptp_device_close(&r);
ptp_generic_close(&r);
```
Calling a custom opcode:
```
// Send a command, and recieve packet(s)
struct PtpCommand cmd;
cmd.code = 0x1234;
cmd.param_length = 3;
cmd.params[0] = 420;
cmd.params[1] = 420;
cmd.params[2] = 420;
return ptp_generic_send(r, &cmd);

// Send a command with data phase, recieve packets
struct PtpCommand cmd;
cmd.code = 0x1234;
cmd.param_length = 1;
cmd.params[0] = 1234;
uint32_t dat[2] = {123, 123};
return ptp_generic_send_data(r, &cmd, dat, sizeof(dat));
```
