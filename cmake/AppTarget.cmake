qt_add_executable(${APP_NAME} ${APP_SOURCES})

qt_add_translations(${APP_NAME}
        TS_FILES i18n/app_en.ts i18n/app_pt_BR.ts
        RESOURCE_PREFIX "/i18n"
)

set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${CMAKE_SOURCE_DIR}/VERSION.txt")
file(READ "${CMAKE_SOURCE_DIR}/VERSION.txt" FSORG_VERSION)
string(STRIP "${FSORG_VERSION}" FSORG_VERSION)

target_include_directories(${APP_NAME} PRIVATE "${CMAKE_SOURCE_DIR}/src")

target_compile_definitions(${APP_NAME} PRIVATE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        FSORG_VERSION="${FSORG_VERSION}"
        $<$<CONFIG:Release>:NDEBUG>
)

if (MSVC)
    target_compile_options(${APP_NAME} PRIVATE /permissive- /Zc:preprocessor)
endif ()

target_precompile_headers(${APP_NAME} PRIVATE
        <QtCore/QtCore>
        <QtGui/QtGui>
        <QtWidgets/QtWidgets>
        <QtNetwork/QtNetwork>
        <memory>
        <optional>
        <string>
        <vector>
)

target_link_libraries(${APP_NAME} PRIVATE
        fsorg-view
        fsorg-application
        Qt6::Widgets
        Qt6::Network
        dwmapi
)

if (MINGW)
    target_compile_definitions(${APP_NAME} PRIVATE MINGW_HAS_SECURE_API)
endif ()
