# ESP32 version of usrsctp 

## Overview 

This is a version of usrsctp that compiles on the ESP32.  

This only works with version ESP-IDF versio 5.2.1.  This is because usrsctp uses several pthread routines that are only available with later versions of the ESP-IDF.  

usrsctp does a fair amount of allocating large(ish) buffers on the stack.  This can be challenging for the ESP32, which configures the default stack size for FreeRTOS tasks to be around 2K bytes. To get this to run reliably, I set the stack size to 8192 bytes.  You need to configure the default PThread stack size under `Component config->PThreads` in menuconfig of your program.  Ideally, we would want to fix this, and it should be relatively straightforward to optmize the stack size by reducing the default buffer sizes in the code so smaller default stack sizes can be used.  

## Building 
To build (from the root directory of this repository):

```
source ~/esp-idf-v5.2.1/export.sh
make 
```

After running, it will output `libusrsctp.a` that you can add to your projects.  

When linking with other code, you should add this line to your `CMakeLists.txt`:

> set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DHAVE_SIN_LEN -DHAVE_SA_LEN -DHAVE_SCONN_LEN -DIPPORT_RESERVED=1024 -DUIO_MAXIOV=1024 -D__linux__ -D__Userspace__ -DSCTP_PROCESS_LEVEL_LOCKS -DSCTP_SIMPLE_ALLOCATOR")

There are a few simple tests that you can run under the `examples` directory.  In particular `ekr_client`, which you can compile `ekr_server` and run on Linux machine (for example) to test.  Looking at the `CMakeLists.txt` for these programs in the example directory gives you an idea of how to integrate this library into your code. 


