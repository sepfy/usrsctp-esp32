file(GLOB USRSCTP_SRCS "usrsctplib/*.c" "usrsctplib/netinet/*.c")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DHAVE_SIN_LEN -DHAVE_SA_LEN -DHAVE_SCONN_LEN -DIPPORT_RESERVED=1024 -DUIO_MAXIOV=1024 -D__linux__ -D__Userspace__ -DSCTP_PROCESS_LEVEL_LOCKS -DSCTP_SIMPLE_ALLOCATOR -Wno-error=format= -Wno-format")

idf_component_register(SRCS ${USRSCTP_SRCS} INCLUDE_DIRS "./" "./usrsctplib")
