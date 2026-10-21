cmake_minimum_required(VERSION 3.15)

set(PICO_BOARD pico)
set(PICO_PLATFORM rp2040)

include(pico_sdk_import.cmake)

project(231-firmware-template)

pico_sdk_init()

add_compile_options(-Wall)

# libs лежит вне папки проекта: второй аргумент задаёт каталог сборки
add_subdirectory(../libs ${CMAKE_BINARY_DIR}/libs)

# выключатель профилирования: 0 — вызовов секундомеров в образе нет
target_compile_definitions(profiling PUBLIC
    PROFILING_ENABLED=1
)


add_library(api STATIC api/api.c api/api-callbacks.c)
target_include_directories(api PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(api PRIVATE stdio_text_protocol systime profiling device logging led-task pi-task pico_stdlib)


add_library(led-task STATIC led-task/led-task.c)
target_include_directories(led-task PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(led-task PRIVATE systime pico_stdlib)

add_library(button-task STATIC button-task/button-task.c)
target_include_directories(button-task PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(button-task PRIVATE systime led-task pico_stdlib)

add_library(pi-task STATIC pi-task/pi-task.c)
target_include_directories(pi-task PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pi-task PRIVATE systime pico_stdlib)


add_executable(${PROJECT_NAME} main.c)
target_link_libraries(${PROJECT_NAME} PRIVATE stdio_text_protocol 
    systime profiling 
    logging 
    led-task 
    button-task 
    pi-task 
    api 
    pico_stdlib)


pico_enable_stdio_usb(${PROJECT_NAME} 1)
pico_enable_stdio_uart(${PROJECT_NAME} 0)
pico_add_extra_outputs(${PROJECT_NAME})

