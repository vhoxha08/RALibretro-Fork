# Cross-platform (macOS/Linux) build. The Windows build still uses RALibretro.sln / Makefile.
cmake_minimum_required(VERSION 3.20)
project(RALibretro C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 99)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(SDL2 REQUIRED)
find_package(OpenGL REQUIRED)

# ---- Dear ImGui (menu bar and dialogs outside Windows) --------------------------------------------
# defined before RAIntegration, which builds its ImGui UI backend against this target
set(IMGUI_DIR src/imgui)
add_library(imgui STATIC
  ${IMGUI_DIR}/imgui.cpp
  ${IMGUI_DIR}/imgui_draw.cpp
  ${IMGUI_DIR}/imgui_tables.cpp
  ${IMGUI_DIR}/imgui_widgets.cpp
  ${IMGUI_DIR}/misc/cpp/imgui_stdlib.cpp
  ${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp
  ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC ${IMGUI_DIR})
target_link_libraries(imgui PUBLIC SDL2::SDL2)
if(APPLE)
  target_compile_definitions(imgui PRIVATE GL_SILENCE_DEPRECATION)
endif()

# ---- RAIntegration (linked statically instead of loading RA_Integration.dll) ---------------
set(RA_INTEGRATION_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../RAIntegration" CACHE PATH "RAIntegration checkout")
if(NOT EXISTS "${RA_INTEGRATION_DIR}/CMakeLists.txt")
  message(FATAL_ERROR "RAIntegration not found at ${RA_INTEGRATION_DIR}; set RA_INTEGRATION_DIR")
endif()
set(RA_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory("${RA_INTEGRATION_DIR}" RAIntegration EXCLUDE_FROM_ALL)

# ---- generated version header ----------------------------------------------------------------
set(RALIBRETRO_GENERATED_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${RALIBRETRO_GENERATED_DIR})
add_custom_target(RALibretro_BuildVer
  # the empty GitTag argument has to survive CMake's argument handling, hence bash -c
  COMMAND bash -c "bash \"$0\" \"$1\" '' RA_LIBRETRO"
          ${CMAKE_CURRENT_SOURCE_DIR}/src/RAInterface/MakeBuildVer.sh
          ${RALIBRETRO_GENERATED_DIR}/RA_BuildVer.h
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  VERBATIM
  BYPRODUCTS ${RALIBRETRO_GENERATED_DIR}/RA_BuildVer.h
)

# ---- libchdr (CHD disc images) -----------------------------------------------------------------
set(CHDR_DIR src/libchdr)
add_library(chdr STATIC
  ${CHDR_DIR}/deps/lzma-26.02/src/LzmaDec.c
  ${CHDR_DIR}/deps/miniz-3.1.2/miniz.c
  ${CHDR_DIR}/deps/zstd-1.5.7/zstddeclib.c
  ${CHDR_DIR}/src/libchdr_bitstream.c
  ${CHDR_DIR}/src/libchdr_cdrom.c
  ${CHDR_DIR}/src/libchdr_chd.c
  ${CHDR_DIR}/src/libchdr_codec_avhuff.c
  ${CHDR_DIR}/src/libchdr_codec_cdfl.c
  ${CHDR_DIR}/src/libchdr_codec_cdlz.c
  ${CHDR_DIR}/src/libchdr_codec_cdzl.c
  ${CHDR_DIR}/src/libchdr_codec_cdzs.c
  ${CHDR_DIR}/src/libchdr_codec_flac.c
  ${CHDR_DIR}/src/libchdr_codec_huff.c
  ${CHDR_DIR}/src/libchdr_codec_lzma.c
  ${CHDR_DIR}/src/libchdr_codec_zlib.c
  ${CHDR_DIR}/src/libchdr_codec_zstd.c
  ${CHDR_DIR}/src/libchdr_flac.c
  ${CHDR_DIR}/src/libchdr_huffman.c
)
target_include_directories(chdr PUBLIC
  ${CHDR_DIR}/include
  ${CHDR_DIR}/deps/lzma-26.02/include
  ${CHDR_DIR}/deps/zstd-1.5.7
  ${CHDR_DIR}/deps/miniz-3.1.2
)
target_compile_definitions(chdr PRIVATE Z7_ST ZSTD_DISABLE_ASM)

# ---- nativefiledialog-extended (open/save dialogs outside Windows) ----------------------------------
set(NFD_INSTALL OFF CACHE BOOL "" FORCE)
set(NFD_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(src/nfd EXCLUDE_FROM_ALL)

# ---- RALibretro --------------------------------------------------------------------------------
set(RCHEEVOS_DIR "${RA_INTEGRATION_DIR}/rcheevos") # same commit as src/rcheevos
add_executable(RALibretro
  src/About.cpp
  src/Application.cpp
  src/CdRom.cpp
  src/components/Audio.cpp
  src/components/Config.cpp
  src/components/DialogImGui.cpp
  src/components/Input.cpp
  src/components/Logger.cpp
  src/components/Microphone.cpp
  src/components/Video.cpp
  src/components/VideoContext.cpp
  src/dynlib/dynlib.c
  src/Emulator.cpp
  src/Fsm.cpp
  src/Gui.cpp
  src/Gl.cpp
  src/GlUtil.cpp
  src/Hash.cpp
  src/Hash3DS.cpp
  src/HashCHD.cpp
  src/jsonsax/jsonsax.c
  src/KeyBinds.cpp
  src/libmincrypt/sha256.c
  src/libretro/BareCore.cpp
  src/libretro/Core.cpp
  src/main.cpp
  src/Memory.cpp
  src/MenuImGui.cpp
  src/Platform.cpp
  src/RA_Implementation.cpp
  src/RA_Interface_Static.cpp
  ${RCHEEVOS_DIR}/src/libretro/rc_libretro.c
  src/speex/resample.c
  src/States.cpp
  src/Util.cpp
)
add_dependencies(RALibretro RALibretro_BuildVer)

# RALibretro's own headers must win over RAIntegration's (both have an RAInterface directory)
target_include_directories(RALibretro BEFORE PRIVATE
  src
  src/RAInterface
  src/libretro
  ${RALIBRETRO_GENERATED_DIR}
)
target_compile_definitions(RALibretro PRIVATE
  OUTSIDE_SPEEX RANDOM_PREFIX=speex EXPORT= FIXED_POINT HAVE_CHD LOG_TO_FILE)
target_link_libraries(RALibretro PRIVATE RAIntegrationImGui chdr imgui nfd SDL2::SDL2 OpenGL::GL ${CMAKE_DL_LIBS})

if(APPLE)
  # OpenGL is deprecated (but available) on macOS
  target_compile_definitions(RALibretro PRIVATE GL_SILENCE_DEPRECATION)
endif()

# Make the build folder a "portable" install: RALibretro uses the executable's folder as its root
# when it contains Cores/ (with cores.json describing the known cores).
add_custom_command(TARGET RALibretro POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:RALibretro>/Cores
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/etc/cores.json
          $<TARGET_FILE_DIR:RALibretro>/Cores/cores.json
)
