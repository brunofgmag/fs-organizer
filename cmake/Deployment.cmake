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

add_custom_command(TARGET ${APP_NAME} POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${CMAKE_SOURCE_DIR}/assets/fonts/OFL.txt"
        "$<TARGET_FILE_DIR:${APP_NAME}>/Archivo-OFL.txt"
        COMMENT "Placing the font license beside the executable"
        VERBATIM)

if (MSVC)
    get_target_property(QT_QMAKE_EXECUTABLE Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(QT_BIN_DIR "${QT_QMAKE_EXECUTABLE}" DIRECTORY)
    find_program(WINDEPLOYQT_EXECUTABLE
            NAMES windeployqt
            HINTS "${QT_BIN_DIR}"
            NO_DEFAULT_PATH
            REQUIRED)
    find_program(POWERSHELL_EXE NAMES pwsh powershell REQUIRED)

    add_custom_command(TARGET ${APP_NAME} POST_BUILD
            COMMAND "${WINDEPLOYQT_EXECUTABLE}"
            "$<IF:$<CONFIG:Debug>,--debug,--release>"
            --translations en,pt_BR
            --skip-plugin-types qmltooling,generic,imageformats,iconengines,networkinformation,platforminputcontexts
            --exclude-plugins qcertonlybackend,qopensslbackend
            --no-system-d3d-compiler
            --no-system-dxc-compiler
            --no-opengl-sw
            --no-compiler-runtime
            "$<TARGET_FILE:${APP_NAME}>"
            COMMAND "${POWERSHELL_EXE}" -NoProfile -ExecutionPolicy Bypass
            -File "${CMAKE_SOURCE_DIR}/tools/deploy-msvc-runtime.ps1"
            -DeploymentDir "$<TARGET_FILE_DIR:${APP_NAME}>"
            -Configuration "$<CONFIG>"
            COMMAND "${POWERSHELL_EXE}" -NoProfile -ExecutionPolicy Bypass
            -File "${CMAKE_SOURCE_DIR}/tools/prune-deployment.ps1"
            -DeploymentDir "$<TARGET_FILE_DIR:${APP_NAME}>"
            COMMENT "Deploying the Qt runtime, pruned"
            VERBATIM)
endif ()

if (NOT WIN32)
    include(GNUInstallDirs)

    install(TARGETS ${APP_NAME} RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

    install(FILES "${CMAKE_SOURCE_DIR}/assets/linux/fs-organizer.desktop"
            DESTINATION "${CMAKE_INSTALL_DATADIR}/applications")

    foreach (ICON_SIZE 16 24 32 48 64 128 256)
        install(FILES "${CMAKE_SOURCE_DIR}/assets/branding/icon_${ICON_SIZE}.png"
                DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/${ICON_SIZE}x${ICON_SIZE}/apps"
                RENAME "${APP_NAME}.png")
    endforeach ()

    install(FILES "${CMAKE_SOURCE_DIR}/LICENSE"
            DESTINATION "${CMAKE_INSTALL_DOCDIR}")
    install(FILES "${CMAKE_SOURCE_DIR}/assets/fonts/OFL.txt"
            DESTINATION "${CMAKE_INSTALL_DOCDIR}"
            RENAME "Archivo-OFL.txt")
endif ()
