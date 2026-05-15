#include "luachild.h"
#ifdef USE_LUAPUC
#if LUA_VERSION_NUM == 501

#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "lualib.h"
#include "lauxlib.h"

int lua_report_type_error(lua_State *L, int narg, const char * tname) {
  // TODO : proper implementation !-
  printf("bad argument %d to function %s\n", narg, tname);
  return 0;
}

size_t lua_value_length(lua_State *L, int index) {
  return lua_objlen(L, index);
}

static int file_close(lua_State *L) {
  int result = 1;
  FILE **p = (FILE **)luaL_checkudata(L, 1, LUA_FILEHANDLE);
  if (!fclose(*p)) {
    lua_pushboolean(L, 1);
  } else {
    int en = errno;  /* calls to Lua API may change this value */
    lua_pushnil(L);
    lua_pushfstring(L, "%s", strerror(en));
    lua_pushinteger(L, en);
    result = 3;
  }
  *p = NULL;
  return result;
}

int file_handler_creator(lua_State *L, const char * file_path, int get_path_from_env){
  return 1;
}

void lua_pushcfile(lua_State *L, FILE * f){
  FILE **pf = (FILE **)lua_newuserdata(L, sizeof(FILE *));
  *pf = f;
  luaL_getmetatable(L, LUA_FILEHANDLE);
  lua_setmetatable(L, -2);
  // create and set an env table with a __close function field
  lua_createtable(L, 0, 1);
  lua_pushcfunction(L, file_close);
  lua_setfield(L, -2, "__close");
  lua_setfenv(L, -2);
}

#endif // LUA_VERSION_NUM == 501
#endif // USE_LUAPUC

