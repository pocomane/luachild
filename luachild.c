
#ifdef USE_POSIX
#include "luachild_posix.c"
#endif

#ifdef USE_WINDOWS
#include "luachild_windows.c"
#endif

#include "lualib.h"

#ifdef LUA_JITLIBNAME // Match luajit
#include "luachild_luajit_2_1.c"
#else // fallback to PUC-Rio lua
#include "luachild_lua_5_3.c"
#endif

#include "luachild_common.c"

