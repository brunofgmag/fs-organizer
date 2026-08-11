if (NOT CMAKE_PREFIX_PATH AND EXISTS "C:/Qt")
    file(GLOB QT_MSVC_KITS LIST_DIRECTORIES TRUE "C:/Qt/6.*/msvc2022_64")
    list(SORT QT_MSVC_KITS COMPARE NATURAL ORDER DESCENDING)
    if (QT_MSVC_KITS)
        list(GET QT_MSVC_KITS 0 AUTO_DETECTED_QT_ROOT)
        list(PREPEND CMAKE_PREFIX_PATH "${AUTO_DETECTED_QT_ROOT}")
        message(STATUS "Auto-detected Qt MSVC kit: ${AUTO_DETECTED_QT_ROOT}")
    endif ()
endif ()

find_package(Qt6 6.8 REQUIRED COMPONENTS Widgets Network LinguistTools)

qt_standard_project_setup(REQUIRES 6.8)

set(CMAKE_AUTORCC ON)

include(CTest)

if (BUILD_TESTING)
    find_package(Qt6 6.8 REQUIRED COMPONENTS Test)
endif ()
