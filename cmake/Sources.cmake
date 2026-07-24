set(APP_SOURCES
        src/main.cpp
)

if (EXISTS "${CMAKE_SOURCE_DIR}/assets/branding/app.ico")
    list(APPEND APP_SOURCES assets/app.rc)
endif ()
