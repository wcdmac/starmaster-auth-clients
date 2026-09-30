# 单文件 XLL 构建（Windows / MSVC）。产物为 DeviceLogin.xll。
cmake_minimum_required(VERSION 3.20)
project(DeviceLoginXLL CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
enable_language(RC)

add_library(DeviceLogin SHARED
    xllmain.cpp
    login_dialog.cpp
    device_flow.cpp
    xlcall.cpp
    qrcodegen.cpp
    resource.rc
    xll.def
)

# 导出名由 xll.def 控制；后缀改为 .xll
set_target_properties(DeviceLogin PROPERTIES SUFFIX ".xll")

target_link_libraries(DeviceLogin user32 gdi32 winhttp)

# 关闭 MSVC 警告噪声
if(MSVC)
    target_compile_options(DeviceLogin PRIVATE /W3 /EHsc)
endif()
