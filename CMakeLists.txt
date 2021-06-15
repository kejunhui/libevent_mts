CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
PROJECT(caster)
SET (CMAKE_INCLUDE_CURRENT_DIR ON)
SET (CMAKE_CXX_FLAGS "-std=c++14 -O3 -fPIC -pie -z noexecstack -fstack-protector")
#add_definitions(-D_GLIBCXX_USE_CXX11_ABI=0)
#add_definitions("-Wall -g") 	 
include_directories (
	ntripcaster/thirdparty/
	ntripcaster/thirdparty/linux
	ntripcaster/thirdparty/linux/libevent/
	ntripcaster/src/app
	ntripcaster/src/base
	ntripcaster/tools
	/usr/include/boost 	
	)

FILE (GLOB CASTER_SRCS
	ntripcaster/*.cpp
	ntripcaster/src/*.cpp
	ntripcaster/src/app/*.cpp
	ntripcaster/src/base/*.cpp
	ntripcaster/tools/*.cpp 
	)
		
LINK_DIRECTORIES (/usr/local/lib /usr/lib)  		
		 
ADD_EXECUTABLE (caster ${CASTER_SRCS})
FIND_PACKAGE (Boost COMPONENTS system thread REQUIRED)

TARGET_LINK_LIBRARIES (caster ${Boost_LIBRARIES}
			 libevent.so libevent_pthreads.so libevent_openssl.so  -ldl)
