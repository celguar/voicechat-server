#
# This file is part of the CMaNGOS Project. See AUTHORS file for Copyright information
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
#
cmake_minimum_required(VERSION 3.9)

project(voicechat-server C)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(EXECUTABLE_NAME voicechat-server)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/build)

file(GLOB VOICECHATSERVER_HEADERS_FILES
        *.h
)

file(GLOB VOICECHATSERVER_SOURCES_FILES
        ascent_socket.c
        configfile.c
        launcher.c
        log.c
        network_handlers.c
        network_unixselect.c
        network_win32.c
        voice_channel.c
        voice_socket.c
)

set(EXECUTABLE_SRCS
      ${VOICECHATSERVER_HEADERS_FILES}
      ${VOICECHATSERVER_SOURCES_FILES}
   )

if(WIN32)
  # add resource file to windows build
  set(EXECUTABLE_SRCS ${EXECUTABLE_SRCS} voicechat-server.rc)
endif()

add_executable(${EXECUTABLE_NAME}
  ${EXECUTABLE_SRCS}
)

if(WIN32)
  target_link_libraries(${EXECUTABLE_NAME}
          wsock32
          ws2_32
  )
endif()

# Standalone build - define build dirs
if (${CMAKE_PROJECT_NAME} MATCHES "voicechat-server")
  if(WIN32)
    if(PLATFORM MATCHES X86) # 32-bit
      set(DEP_ARCH Win32)
    else() # 64-bit
      set(DEP_ARCH x64)
    endif()
  endif()

  if(CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_CONFIGURATION_TYPES "Release;Debug;RelWithDebInfo;MinSizeRel" CACHE STRING "Reset the configurations to what we need" FORCE)
  else()
    if(NOT CMAKE_BUILD_TYPE)
      if (NOT DEBUG)
        message("Defaulting to release build.")
        set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
      else()
        set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)
      endif()
    endif()
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY HELPSTRING "Choose the type of build")
    # set the valid options for cmake-gui drop-down list
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug;Release;RelWithDebInfo;MinSizeRel")
  endif()

  set(BIN_FOLDER_NAME bin)
  set(CONF_FOLDER_NAME etc)

  # Set Install folders
  set(BIN_DIR ${CMAKE_INSTALL_PREFIX}/${BIN_FOLDER_NAME})
  set(CONF_DIR ${CMAKE_INSTALL_PREFIX}/${CONF_FOLDER_NAME})

  if(WIN32)
    set(BIN_DIR ${CMAKE_INSTALL_PREFIX})
    set(CONF_DIR ${BIN_DIR})
    get_property(IS_MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG) # ninja generator does not support $(Configuration)
    if(MSVC AND "$ENV{CLION_IDE}" STREQUAL "" AND IS_MULTI_CONFIG)
      set(DEV_BIN_DIR ${CMAKE_BINARY_DIR}/${BIN_FOLDER_NAME}/${DEP_ARCH}_$(Configuration))
      set(DEV_PROVIDED_LIBS_FOLDER ${CMAKE_SOURCE_DIR}/dep/lib/${DEP_ARCH}_$(Configuration))
    else()
      set(DEV_BIN_DIR ${CMAKE_BINARY_DIR}/${BIN_FOLDER_NAME}/${DEP_ARCH}_${CMAKE_BUILD_TYPE})
      set(DEV_PROVIDED_LIBS_FOLDER ${CMAKE_SOURCE_DIR}/dep/lib/${DEP_ARCH}_${CMAKE_BUILD_TYPE})
    endif()
  endif()
endif ()

# Other projects - define BIN DIR and DEV_BIN_DIR here

if(WIN32)
  # Define OutDir to source/bin/(platform)_(configuaration) folder.
  set_target_properties(${EXECUTABLE_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY_DEBUG "${DEV_BIN_DIR}")
  set_target_properties(${EXECUTABLE_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY_RELEASE "${DEV_BIN_DIR}")
  set_target_properties(${EXECUTABLE_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${DEV_BIN_DIR}")
  set_target_properties(${EXECUTABLE_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${DEV_BIN_DIR}")
  set_target_properties(${EXECUTABLE_NAME} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${DEV_BIN_DIR}")
  set_target_properties(${EXECUTABLE_NAME} PROPERTIES PROJECT_LABEL "VoiceChatServer")


  # Add conf file to dev folder
  add_custom_command(TARGET ${EXECUTABLE_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different \"${CMAKE_CURRENT_SOURCE_DIR}/voicechat-server.conf.dist.in\" \"$<TARGET_FILE_DIR:${EXECUTABLE_NAME}>/voicechat-server.conf.dist\")
endif()

set(EXECUTABLE_LINK_FLAGS "")

if(UNIX)
  set(EXECUTABLE_LINK_FLAGS "-pthread ${EXECUTABLE_LINK_FLAGS}")
endif()

if(APPLE)
  set(EXECUTABLE_LINK_FLAGS "-framework Carbon ${EXECUTABLE_LINK_FLAGS}")
endif()

set_target_properties(${EXECUTABLE_NAME} PROPERTIES LINK_FLAGS
  "${EXECUTABLE_LINK_FLAGS}"
)

install(TARGETS ${EXECUTABLE_NAME} DESTINATION ${BIN_DIR})

# CMaNGOS
if (${CMAKE_PROJECT_NAME} MATCHES "CMaNGOS")
  set(CMAKE_BUILD_TYPE ${BUILD_TYPE})
  install(FILES voicechat-server.conf.dist.in DESTINATION ${CONF_DIR} RENAME voicechat-server.conf.dist)

  if(MSVC)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/\${BUILD_TYPE}/${EXECUTABLE_NAME}.pdb DESTINATION ${BIN_DIR} CONFIGURATIONS Debug)
  endif()
endif()
