cmake_minimum_required(VERSION 3.16)
project(tron2_highlevel_examples LANGUAGES CXX)

find_package(Threads REQUIRED)
find_package(Boost 1.73 REQUIRED COMPONENTS system)
find_package(OpenSSL REQUIRED)
find_package(nlohmann_json 3.6.0 REQUIRED)
find_package(SDL2 REQUIRED CONFIG)
find_package(OpenCV REQUIRED COMPONENTS core imgcodecs highgui)
find_path(WEBSOCKETPP_INCLUDE_DIR websocketpp/client.hpp)
if(NOT WEBSOCKETPP_INCLUDE_DIR)
    message(FATAL_ERROR "WebSocket++ headers not found; install libwebsocketpp-dev")
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# ---------------------------------------------------------------------------
# 共享模块。边界与 python3/examples/highlevel 的模块一致。
# ---------------------------------------------------------------------------

# 连接、JSON 协议、请求匹配、键盘状态机、BRDG 解析。
add_library(tron2_protocol STATIC protocol.cpp)
target_compile_features(tron2_protocol PUBLIC cxx_std_17)
target_include_directories(tron2_protocol PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(tron2_protocol SYSTEM PRIVATE ${WEBSOCKETPP_INCLUDE_DIR})
target_link_libraries(tron2_protocol PUBLIC Threads::Threads Boost::system
    OpenSSL::SSL OpenSSL::Crypto nlohmann_json::nlohmann_json)

# 参数解析、确认提示、连接构造（对应 example_common.py）。
add_library(tron2_example_common STATIC example_common.cpp)
target_link_libraries(tron2_example_common PUBLIC tron2_protocol)

# 双臂、头部、夹爪（对应 robot_utils.py）。
add_library(tron2_robot_utils STATIC robot_utils.cpp)
target_link_libraries(tron2_robot_utils PUBLIC tron2_example_common)

# 底盘、升降台、键盘控制（对应 mobile_platform_common.py）。SDL2 只在这里需要。
add_library(tron2_mobile_common STATIC mobile_platform_common.cpp)
target_link_libraries(tron2_mobile_common PUBLIC tron2_example_common SDL2::SDL2)

# 相机接收与统计，不含 OpenCV。
add_library(tron2_camera_common STATIC camera_common.cpp)
target_link_libraries(tron2_camera_common PUBLIC tron2_example_common)

# OpenCV 解码与窗口显示，只有需要显示的两个相机示例链接。
add_library(tron2_camera_display STATIC camera_display.cpp)
target_include_directories(tron2_camera_display SYSTEM PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(tron2_camera_display PUBLIC tron2_camera_common PRIVATE ${OpenCV_LIBS})

# ---------------------------------------------------------------------------
# 示例可执行文件。名称与 python3/examples/highlevel 的脚本名（去掉 .py）一致。
# ---------------------------------------------------------------------------

# 双臂、头部、夹爪：参数为源码顶部常量，只接受连接类选项。
set(ARM_EXAMPLES
    get_joint_state get_ee_pose get_gripper_state
    move_joint move_head move_pose
    move_servo_joint move_servo_pose move_gripper
    move_head_websocket move_head_websocket_client)

# 底盘与升降台：保留完整命令行参数。
set(MOBILE_EXAMPLES
    get_chassis_state get_lifter_state get_lifter_position
    move_lifter_position move_lifter_velocity set_mode_chassis move_chassis)

# 相机：需要 OpenCV 解码显示。
set(CAMERA_DISPLAY_EXAMPLES camera_stream camera_stream_3)
# 相机吞吐测试：纯接收，不需要 OpenCV。
set(CAMERA_HEADLESS_EXAMPLES camera_websocket_benchmark)

foreach(example IN LISTS ARM_EXAMPLES)
    add_executable(${example} ${example}.cpp)
    target_link_libraries(${example} PRIVATE tron2_robot_utils)
endforeach()

foreach(example IN LISTS MOBILE_EXAMPLES)
    add_executable(${example} ${example}.cpp)
    target_link_libraries(${example} PRIVATE tron2_mobile_common)
endforeach()

foreach(example IN LISTS CAMERA_DISPLAY_EXAMPLES)
    add_executable(${example} ${example}.cpp)
    target_link_libraries(${example} PRIVATE tron2_camera_display)
endforeach()

foreach(example IN LISTS CAMERA_HEADLESS_EXAMPLES)
    add_executable(${example} ${example}.cpp)
    target_link_libraries(${example} PRIVATE tron2_camera_common)
endforeach()

set(ALL_EXAMPLES ${ARM_EXAMPLES} ${MOBILE_EXAMPLES}
    ${CAMERA_DISPLAY_EXAMPLES} ${CAMERA_HEADLESS_EXAMPLES})

install(TARGETS ${ALL_EXAMPLES} RUNTIME DESTINATION bin)

# ---------------------------------------------------------------------------
# 测试。默认只跑不连接机器人的离线协议测试和每个示例的 --help 检查。
# ---------------------------------------------------------------------------
include(CTest)
if(BUILD_TESTING)
    add_executable(tron2_highlevel_tests tests.cpp)
    target_include_directories(tron2_highlevel_tests SYSTEM PRIVATE ${WEBSOCKETPP_INCLUDE_DIR})
    target_link_libraries(tron2_highlevel_tests PRIVATE tron2_protocol)
    add_test(NAME tron2_highlevel_offline COMMAND tron2_highlevel_tests)
    set_tests_properties(tron2_highlevel_offline PROPERTIES TIMEOUT 30)
    foreach(example IN LISTS ALL_EXAMPLES)
        add_test(NAME help_${example} COMMAND ${example} --help)
    endforeach()
endif()
