cmake_minimum_required(VERSION 3.0)

project(soloud VERSION 1.1.1 ) #LANGUAGES C++)

set(SOLOUD_SOURCE_PATH ./src/src)
file(GLOB CORE_SOURCE ${SOLOUD_SOURCE_PATH}/core/*.cpp)
file(GLOB_RECURSE BACKEND_SOURCE ${SOLOUD_SOURCE_PATH}/backend/*.cpp)
file(GLOB FILTER_SOURCE ${SOLOUD_SOURCE_PATH}/filter/*.cpp)
file(GLOB_RECURSE AUDIOSOURCE_SOURCE ${SOLOUD_SOURCE_PATH}/audiosource/*.cpp ${SOLOUD_SOURCE_PATH}/audiosource/*.c)
set(SOLOUD_SOURCE
        ${CORE_SOURCE}
        ${BACKEND_SOURCE}
		${FILTER_SOURCE}
		${AUDIOSOURCE_SOURCE}
		)

add_library(soloud STATIC ${SOLOUD_SOURCE})	
target_include_directories(soloud PUBLIC ./src/include)
#backends per runtime dll/so linkage:
#still need header files though: WITH_SDL2 WITH_PORTAUDIO WITH_OPENAL
set(SOLOUD_DEFINITIONS WITH_NULL)
IF(WIN32)
	# all backends seems to get a handle exception on deinit (with SDL2, Windows 10 x64, compiled to 32bit) WITH_XAUDIO2 WITH_WINMM
	# Wasapi is brocken as of 02.2019 (returs error at once in init) WITH_WASAPI
	#all seem to need CoInitializeEx(0, COINIT_MULTITHREADED); which does not play nice with SDL2
	 set(SOLOUD_DEFINITIONS ${SOLOUD_DEFINITIONS}  WITH_XAUDIO2 WITH_WINMM ) 
	#only on windows (loads from DLL if it can find it-> shouldn't be on libretro):
	#target_compile_definitions(soloud PRIVATE  WITH_SDL2)
	#target_sources(soloud PRIVATE ${SOLOUD_SOURCE_PATH}/backend/sdl/soloud_sdl2_dll.c)
ENDIF(WIN32)
include(CheckIncludeFile) 
CHECK_INCLUDE_FILE(portaudio.h HAVE_PORTAUDIO_H LANGUAGE CXX)
IF(HAVE_PORTAUDIO_H)
	set(SOLOUD_DEFINITIONS ${SOLOUD_DEFINITIONS} WITH_PORTAUDIO)#linked at runtime
ENDIF(HAVE_PORTAUDIO_H)
FIND_PACKAGE(ALSA)
IF(ALSA_FOUND)
	set(SOLOUD_DEFINITIONS ${SOLOUD_DEFINITIONS} WITH_ALSA)
	target_link_libraries(soloud INTERFACE ${ALSA_LIBRARIES})
	target_include_directories(soloud PRIVATE ${ALSA_INCLUDE_DIRS})
ENDIF(ALSA_FOUND)
#http://sol.gfxile.net/soloud/quickstart.html
#TODO find other backends
message(STATUS "soloud will be compiled with: ${SOLOUD_DEFINITIONS}")
target_compile_definitions(soloud PRIVATE ${SOLOUD_DEFINITIONS})