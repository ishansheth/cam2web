cmake_minimum_required(VERSION 3.0)
Project(RpiCamToWeb VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS On)
add_definitions(-DMG_ENABLE_THREADS)
find_package(OpenCV REQUIRED)

add_subdirectory(src/tools/web2h/)

function(createWebHeaders input_files input_folder)
  foreach(file IN LISTS ${input_files})

    add_custom_command(OUTPUT ${file} 
      COMMAND web2h -i ${CMAKE_CURRENT_LIST_DIR}${input_folder}${file} -o ${file}.h
      COMMENT "Creating " ${file}.h
    )
    add_custom_target(web_headers_${file} DEPENDS ${file})

  endforeach()
endfunction()


function(createDependency target input_files)
  foreach(file IN LISTS ${input_files})
    add_dependencies(${target} web_headers_${file})
  endforeach()
endfunction()

set(WEB2H_HEADERS 
  index.html 
  camera.js 
  cameraproperties.html 
  login.html
  cameraproperties.js 
  index.png 
  record_start.png 
  record_stop.png 
  styles.css up.png 
  cam2web.png 
  cam2web_white.png 
)

set(WEB2H_HEADERS_EXTERNAL   
  jquery.js
  jquery.mobile.css
  jquery.mobile.js
)

createWebHeaders(WEB2H_HEADERS /src/web/)
createWebHeaders(WEB2H_HEADERS_EXTERNAL /externals/jquery/)


add_executable(cam2web  
  src/apps/linux/cam2web.cpp
  src/core/cameras/V4L2/XV4LCamera.cpp
  src/core/XWebServer.cpp
  src/core/XManualResetEvent.cpp
  src/core/XVideoSourceToWeb.cpp
  src/core/XObjectConfigurationSerializer.cpp
  externals/mongoose/mongoose.c
  src/core/XImage.cpp
  src/core/XJpegEncoder.cpp
  src/core/cameras/V4L2/XV4LCameraConfig.cpp
  src/core/XSimpleJsonParser.cpp 
  src/core/XObjectConfigurationRequestHandler.cpp
  src/core/XStringTools.cpp
  src/core/XError.cpp
  )

target_include_directories(cam2web PRIVATE src/core src/core/cameras/V4L2 externals/mongoose)
target_link_libraries(cam2web jpeg pthread ${OpenCV_LIBS})
target_include_directories(cam2web PRIVATE ${CMAKE_BINARY_DIR})
createDependency(cam2web WEB2H_HEADERS)
createDependency(cam2web WEB2H_HEADERS_EXTERNAL)

