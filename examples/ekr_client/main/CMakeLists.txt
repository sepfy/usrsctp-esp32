set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DHAVE_SIN_LEN -DHAVE_SA_LEN -DHAVE_SCONN_LEN -DIPPORT_RESERVED=1024 -DUIO_MAXIOV=1024 -D__linux__ -D__Userspace__ -DSCTP_PROCESS_LEVEL_LOCKS -DSCTP_SIMPLE_ALLOCATOR")

idf_component_register(SRCS "main.c" "../../../programs/programs_helper.c"
                    INCLUDE_DIRS "." "${CMAKE_CURRENT_SOURCE_DIR}/../../../usrsctplib" "${CMAKE_CURRENT_SOURCE_DIR}/../../../programs")

target_link_libraries(${COMPONENT_TARGET} "${CMAKE_CURRENT_SOURCE_DIR}/../../../libusrsctp.a")
