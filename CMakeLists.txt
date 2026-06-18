cmake_minimum_required(VERSION 3.10)
project(native_library C)

set(CMAKE_C_STANDARD 99)

include_directories(native/include)

add_library(native_library SHARED
    native/device_manager.c
    native/video_capture.c
    native/audio_capture.c
)

if(MSVC)
    # Vincula as bibliotecas do sistema Windows necessárias
    target_link_libraries(native_library PRIVATE
        mf
        mfplat
        mfuuid
        mfreadwrite
        ole32
        oleaut32
        uuid
        user32
        shlwapi
    )
    # Define flags de compilação adicionais para MSVC se necessário
    target_compile_definitions(native_library PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()


