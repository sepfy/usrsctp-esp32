file(GLOB USRSCTP_SRCS "usrsctplib/*.c" "usrsctplib/netinet/*.c")

idf_component_register(SRCS ${USRSCTP_SRCS} INCLUDE_DIRS "./" "./usrsctplib")

target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-format")

target_compile_definitions(${COMPONENT_LIB} PUBLIC
    -DESP32
    -DHTTP_DO_NOT_USE_CUSTOM_CONFIG
    -DMQTT_DO_NOT_USE_CUSTOM_CONFIG
    -DHAVE_SIN_LEN
    -DHAVE_SA_LEN
    -DHAVE_SCONN_LEN
    -DIPPORT_RESERVED=1024
    -DUIO_MAXIOV=1024
    -D__linux__
    -D__Userspace__
    -DSCTP_PROCESS_LEVEL_LOCKS
    -DSCTP_SIMPLE_ALLOCATOR
)
