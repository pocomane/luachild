
#ifndef _LUA_CHILD_H_
#define _LUA_CHILD_H_

#include "lua.h"

#define PROCESS_HANDLE "process"

static int lc_pipe(lua_State *L);
static int lc_setenv(lua_State *L);
static int lc_environ(lua_State *L);
static int lc_spawn(lua_State *L);
static int process_wait(lua_State *L);
static int diriter_close(lua_State *L);
static int process_tostring(lua_State *L);

static int lua_report_type_error(lua_State *L, int narg, const char * tname);
static size_t lua_value_length(lua_State *L, int index);

static int file_handler_creator(lua_State *L, const char * file_path, int get_path_from_env);
static void lua_pushcfile(lua_State *L, FILE * f);

#endif // _LUA_CHILD_H_

