
#ifndef _LUA_CHILD_H_
#define _LUA_CHILD_H_

#include "lua.h"
#include "lualib.h"

#ifdef LUA_JITLIBNAME // Match luajit
#define USE_LUAJIT
#else // fallback to PUC-Rio lua
#define USE_LUAPUC
#endif

#ifdef USE_POSIX
  #define SHFUNC
#endif

#ifdef USE_WINDOWS
#ifndef SHFUNC
  #define SHFUNC __declspec(dllexport)
#endif
#endif

#define PROCESS_HANDLE "process"

int lc_pipe(lua_State *L);
int lc_setenv(lua_State *L);
int lc_environ(lua_State *L);
int lc_spawn(lua_State *L);
int process_wait(lua_State *L);
int diriter_close(lua_State *L);
int process_tostring(lua_State *L);

int lua_report_type_error(lua_State *L, int narg, const char * tname);
size_t lua_value_length(lua_State *L, int index);

int file_handler_creator(lua_State *L, const char * file_path, int get_path_from_env);

#include <stdio.h>

void lua_pushcfile(lua_State *L, FILE * f);

#endif // _LUA_CHILD_H_

