
# INCLUDE(FindLua)
# IF(NOT LUA_FOUND)
	SET(LUA_SOURCES_CORE lapi.c lcode.c lctype.c ldebug.c ldo.c ldump.c lfunc.c lgc.c llex.c
		lmem.c lobject.c lopcodes.c lparser.c lstate.c lstring.c ltable.c
		ltm.c lundump.c lvm.c lzio.c)
	SET(LUA_SOURCES_LIB lauxlib.c lbaselib.c lbitlib.c lcorolib.c ldblib.c liolib.c
		lmathlib.c loslib.c lstrlib.c ltablib.c lutf8lib.c loadlib.c linit.c)
	SET(LUA_SOURCES ${LUA_SOURCES_CORE} ${LUA_SOURCES_LIB})

	SET(LUA_LIBRARIES lua;m;${CMAKE_DL_LIBS})
	SET(LUA_VERSION_PATCH 1)
	SET(LUA_VERSION_MINOR 3)
	SET(LUA_VERSION_MAJOR 5)
	SET(LUA_VERSION_STRING "${LUA_VERSION_MAJOR}.${LUA_VERSION_MINOR}.${LUA_VERSION_PATCH}")
	SET(LUA_INCLUDE_DIR "lua")

	ADD_DEFINITIONS(-DLUA_COMPAT_5_2)

	IF(WIN32 AND NOT CYGWIN)

		#	"AR=$(CC) -shared -o" "RANLIB=strip --strip-unneeded" \
		#"SYSCFLAGS=-DLUA_BUILD_AS_DLL" "SYSLIBS=" "SYSLDFLAGS=-s" lua.exe
		#$(MAKE) "LUAC_T=luac.exe" luac.exe

	ELSEIF(APPLE)
		ADD_DEFINITIONS(-DLUA_USE_MACOSX)

		# TODO: readline?
	ELSEIF(UNIX)
		IF(${CMAKE_SYSTEM_NAME} MATCHES "Linux" OR ${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")
			ADD_DEFINITIONS(-DLUA_USE_LINUX)
		ELSE()
			ADD_DEFINITIONS(-DLUA_USE_POSIX -DLUA_USE_DLOPEN)

			IF(${CMAKE_SYSTEM_NAME} MATCHES "SunOS")
				ADD_DEFINITIONS(-D_REENTRANT)
			ENDIF()
		ENDIF()
	ELSE()
		ADD_DEFINITIONS(-DLUA_USE_C89)
		MESSAGE("Using Lua build for C89")
		MESSAGE("*** C89 does not guarantee 64-bit integers for Lua.")
	ENDIF()

	ADD_LIBRARY(lua ${LUA_SOURCES})

# ENDIF() # LUA_FOUND

