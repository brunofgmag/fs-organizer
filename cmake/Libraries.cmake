function(fsorg_add_layer LIBRARY_NAME)
    add_library(${LIBRARY_NAME} STATIC ${ARGN})
    target_include_directories(${LIBRARY_NAME} PUBLIC "${CMAKE_SOURCE_DIR}" "${CMAKE_SOURCE_DIR}/src")

    if (MSVC)
        target_compile_options(${LIBRARY_NAME} PRIVATE /permissive- /Zc:preprocessor)
    endif ()
endfunction()

fsorg_add_layer(fsorg-domain ${DOMAIN_SOURCES})
target_precompile_headers(fsorg-domain PRIVATE <filesystem> <map> <memory> <optional> <set> <string> <vector>)

fsorg_add_layer(fsorg-application ${APPLICATION_SOURCES})
target_link_libraries(fsorg-application PUBLIC fsorg-domain)
target_precompile_headers(fsorg-application REUSE_FROM fsorg-domain)

fsorg_add_layer(fsorg-infrastructure ${INFRASTRUCTURE_SOURCES})
target_link_libraries(fsorg-infrastructure PUBLIC fsorg-application Qt6::Core)
target_precompile_headers(fsorg-infrastructure PRIVATE <QtCore/QtCore>)

if (WIN32)
    target_sources(fsorg-infrastructure PRIVATE ${WINDOWS_INFRASTRUCTURE_SOURCES})
    target_link_libraries(fsorg-infrastructure PUBLIC Qt6::Widgets dwmapi)
endif ()

fsorg_add_layer(fsorg-viewmodel ${VIEWMODEL_SOURCES})
target_link_libraries(fsorg-viewmodel PUBLIC fsorg-application Qt6::Core)
target_precompile_headers(fsorg-viewmodel PRIVATE <QtCore/QtCore>)

fsorg_add_layer(fsorg-view ${VIEW_SOURCES})
target_link_libraries(fsorg-view PUBLIC fsorg-viewmodel Qt6::Widgets)
target_precompile_headers(fsorg-view PRIVATE <QtWidgets/QtWidgets>)
