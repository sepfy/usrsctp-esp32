idf_component_register(SRCS "../../../programs/test_timer.c"
                    INCLUDE_DIRS "." "${CMAKE_CURRENT_SOURCE_DIR}/../../../usrsctplib")

target_link_libraries(${COMPONENT_TARGET} "${CMAKE_CURRENT_SOURCE_DIR}/../../../libusrsctp.a")
