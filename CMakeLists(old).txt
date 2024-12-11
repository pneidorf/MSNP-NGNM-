cmake_minimum_required(VERSION 3.10)
project(ZeroMQProject)

# Установка libzmq
execute_process(
    COMMAND sudo sh -c "echo 'deb http://download.opensuse.org/repositories/network:/messaging:/zeromq:/release-stable/xUbuntu_20.04/ /' > /etc/apt/sources.list.d/network:messaging:zeromq:release-stable.list"
    COMMAND curl -fsSL https://download.opensuse.org/repositories/network:messaging:zeromq:release-stable/xUbuntu_20.04/Release.key | gpg --dearmor | sudo tee /etc/apt/trusted.gpg.d/network_messaging_zeromq_release-stable.gpg > /dev/null
    COMMAND sudo apt update
    COMMAND sudo apt install -y libzmq3-dev
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

# Установка czmq
execute_process(
    COMMAND git clone https://github.com/zeromq/czmq.git
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
execute_process(
    COMMAND ./autogen.sh
    COMMAND ./configure
    COMMAND make check
    COMMAND sudo make install
    COMMAND sudo ldconfig
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/czmq
)

# Установка libtool
execute_process(
    COMMAND sudo apt install -y libtool
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

# Установка libcap
execute_process(
    COMMAND sudo apt install -y libpcap-dev
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

set(SOURCE_DIR ${CMAKE_SOURCE_DIR}/tests)
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/src)

# Компиляция server.c и client.c
add_executable(server ${SOURCE_DIR}/server.c)
target_link_libraries(server zmq pcap)  

add_executable(client ${SOURCE_DIR}/client.c)
target_link_libraries(client zmq)

# Запуск server и client
add_custom_target(run_server
    COMMAND ./server
    DEPENDS server
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/src
)

add_custom_target(run_client
    COMMAND ./client
    DEPENDS client
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/src
)

add_custom_target(run_all
    COMMAND ./server &
    COMMAND ./client
    DEPENDS server client
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/src
)

