# rabarber
Rabarber is an experimental/partial mumble voip client implementation targeted at embedded systems.
Specifically, the C.H.I.P. Pro is targeted, and it's starting to show in certain parts of the codebase,
so far in `lradc.c/h`. It's also the first project where I try programming with alsa, openssl, or protobuf
for that matter.

## You'll need additional files to make it compile:
* `config_core.c` and `config_core.h` from [generic_config](https://github.com/tlvb/generic_config)
* `mumble_pb.c` and `mumble_pb.h` which are generated from the mumble protobuf protocol definition with protobuf-c  
* `sll_meta.h` from [sll_meta](https://github.com/tlvb/sll_meta)  
* `varint.c` and `varint.h` from [varint-c](https://github.com/tlvb/varint-c)  
These are to be either copied or symlinked into `src/`

The reason for this is because they are either fetched or generrated from other codebases, as is the case
with the protobuf protocol files, or because I saw them as isolated enough in scope and functionality,
as well as versatile enough that I might want to use them in other projects, that I made a generic
non-rabarber-specific implementation.

## Current issues
* It's not finished yet  
* Frequent ALSA underrun error messages, while not ideal, the program works for the intended purpose.  
* Only a minimally viable part of the mumble protocol is so far supported, for connection+tx+rx, e.g. no udp.
