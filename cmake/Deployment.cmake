set_target_properties(${APP_NAME} PROPERTIES
        OUTPUT_NAME "${APP_NAME}"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        WIN32_EXECUTABLE $<NOT:$<CONFIG:Debug>>
)

if (MSVC)
    set_target_properties(${APP_NAME} PROPERTIES
            INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
endif ()

foreach (CONFIG Debug Release RelWithDebInfo MinSizeRel)
    string(TOUPPER "${CONFIG}" CONFIG_UPPER)
    set_target_properties(${APP_NAME} PROPERTIES
            "RUNTIME_OUTPUT_DIRECTORY_${CONFIG_UPPER}" "${CMAKE_BINARY_DIR}/bin")
endforeach ()

if (MSVC)
    get_target_property(QT_QMAKE_EXECUTABLE Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(QT_BIN_DIR "${QT_QMAKE_EXECUTABLE}" DIRECTORY)
    find_program(WINDEPLOYQT_EXECUTABLE
            NAMES windeployqt
            HINTS "${QT_BIN_DIR}"
            NO_DEFAULT_PATH
            REQUIRED)

    add_custom_command(TARGET ${APP_NAME} POST_BUILD
            COMMAND "${WINDEPLOYQT_EXECUTABLE}"
            "$<IF:$<CONFIG:Debug>,--debug,--release>"
            "$<TARGET_FILE:${APP_NAME}>"
            COMMENT "Deploying the Qt runtime"
            VERBATIM)
endif ()
