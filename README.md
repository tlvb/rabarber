# rabarber
experimental/partial mumble voip client implementation targeted at embedded systems

## You'll need additional files to make it compile:
* `config_core.c` and `config_core.h` from [generic_config](https://github.com/tlvb/generic_config)
* `mumble_pb.c` and `mumble_pb.h` which are generated from the mumble protobuf protocol definition with protobuf-c  
* `sll_meta.h` from [ssl_meta](https://github.com/tlvb/sll_meta)  
* `varint.c` and `varint.h` from [varint-c](https://github.com/tlvb/varint-c)
These are to be either copied or symlinked into `src/`
