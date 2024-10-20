list(APPEND inc_list
    ${COMPONENT_DIR}/config
    ${COMPONENT_DIR}/bsp/include
    ${COMPONENT_DIR}/comm/include
    ${COMPONENT_DIR}/misc/include
    ${COMPONENT_DIR}/gui
)

list (APPEND src_list
    ${COMPONENT_DIR}/main.cpp
    ${COMPONENT_DIR}/bsp/src/key.cpp
    ${COMPONENT_DIR}/bsp/src/lcd_st7735.cpp
    ${COMPONENT_DIR}/comm/tcp_server.cpp
    ${COMPONENT_DIR}/comm/tcp_data_handle.cpp
    ${COMPONENT_DIR}/gui/gui.cpp
    ${COMPONENT_DIR}/misc/cmds.cpp
)

idf_component_register(
    SRCS
    ${src_list}
    INCLUDE_DIRS
    ${inc_list}
    )
