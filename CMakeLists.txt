cmake_minimum_required(VERSION 4.1)
project(QueueLauncher)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOUIC_SEARCH_PATHS
  ui
)

find_package(Qt6 COMPONENTS
  Core
  Gui
  Widgets
  Sql
  REQUIRED
)

add_executable(${PROJECT_NAME} WIN32
  src/Main.cpp
  src/QueueLauncher.cpp
  src/Utilities.cpp
  include/QueueLauncher.h
)
target_include_directories(${PROJECT_NAME} PRIVATE
  include
  external/ValveFileVDF/include
)
target_compile_definitions(${PROJECT_NAME} PRIVATE
  PROJECT_NAME="${PROJECT_NAME}"
)
target_link_libraries(${PROJECT_NAME} PRIVATE
  Qt::Core
  Qt::Gui
  Qt::Widgets
  Qt::Sql
)

install(TARGETS ${PROJECT_NAME}
  RUNTIME_DEPENDENCY_SET RUNTIME_DEPS
)

if (WIN32)
  get_target_property(QT_QMAKE_EXE Qt6::qmake IMPORTED_LOCATION)
  get_filename_component(QT_BIN_DIR "${QT_QMAKE_EXE}" DIRECTORY)

  set(WINDEPLOYQT_EXE "${QT_BIN_DIR}/windeployqt.exe")
  if (EXISTS "${WINDEPLOYQT_EXE}")
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
      COMMAND "${WINDEPLOYQT_EXE}" --no-translations "$<TARGET_FILE:${PROJECT_NAME}>"
    )
  endif ()

  qt_generate_deploy_app_script(TARGET ${PROJECT_NAME}
    OUTPUT_SCRIPT DEPLOY_SCRIPT
    NO_TRANSLATIONS
    NO_UNSUPPORTED_PLATFORM_ERROR
  )
  install(SCRIPT "${DEPLOY_SCRIPT}")

  install(RUNTIME_DEPENDENCY_SET RUNTIME_DEPS
    PRE_EXCLUDE_REGEXES
    "api-ms-win-.*"
    POST_EXCLUDE_REGEXES
    ".*Qt6.*"
    ".*system32.*"
  )
endif ()
