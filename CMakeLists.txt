cmake_minimum_required(VERSION 3.14)

project(vtk_viewer VERSION 0.1 LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(VTK REQUIRED)
include(${VTK_USE_FILE})

find_package(Qt5 COMPONENTS Core Quick REQUIRED)
find_package(DCMTK REQUIRED)
find_package(OpenCV REQUIRED)
find_package(CGAL)

include_directories(${OpenCV_INCLUDE_DIRS})

set(PROJECT_SOURCES
        main.cpp
        qml.qrc
)

set(VIEWERS_SOURCES
        Viewers/QVTKModelViewer.cpp
        Viewers/QVTKPlaneViewer.cpp
        Viewers/MriDataProvider.cpp
)

set(MODEL_SOURCES
        Model/head_cloud.cpp
        Model/model_builder.cpp
        Model/post_processing.cpp
        Model/utility_dcm.cpp
)

add_executable(vtk_viewer
        ${PROJECT_SOURCES}
        ${VIEWERS_SOURCES}
        ${MODEL_SOURCES}
)

target_link_libraries(vtk_viewer PRIVATE
        Qt5::Core
        Qt5::Quick
        CGAL::CGAL
        ${OpenCV_LIBS}
        ${VTK_LIBRARIES}
        ${DCMTK_LIBRARIES}
)
