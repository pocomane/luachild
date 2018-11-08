#include "luachild.h"
#ifdef USE_LUAJIT

#include <stdio.h>
#include <inttypes.h>

#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

static int lua_report_type_error(lua_State *L, int narg, const char * tname) {
  return luaL_typerror(L, narg, tname);
}

static size_t lua_value_length(lua_State *L, int index) {
  return lua_objlen(L, index);
}

static int (*lua_open_func)(lua_State *L) = 0;
static char * temp_file_path = 0;

static int file_handler_creator(lua_State *L, const char * file_path, int get_path_from_env){

  if (!file_path)
    return !lua_open_func || !temp_file_path;

  if (!lua_open_func) {
    lua_getglobal(L, "io");
    lua_getfield(L, -1, "open");
    lua_open_func = lua_tocfunction(L, -1);
    lua_pop(L, 2);
    if (!lua_open_func) return 0;
  }

  if (!temp_file_path) {
    if (get_path_from_env)
      temp_file_path = strdup(getenv(file_path));
    else
      temp_file_path = strdup(file_path);
    if (!temp_file_path) return 0;
  }

  return 1;
}

static int push_null_file_handler(lua_State *L){
  lua_pushcfunction(L, lua_open_func);
  lua_pushstring(L, temp_file_path);
  lua_pushstring(L, "r");
  lua_call(L, 2, LUA_MULTRET);

  FILE** iof = (FILE**)lua_touserdata(L, -1);
  if (iof) fclose(*iof);
  *iof = 0;

  return 1;
}

// // This funcrtion requires luajit internal api.
// // Please include relevant headers.
// static int push_null_file_handler(lua_State *L){
//   IOFileUD *iof = (IOFileUD *)lua_newuserdata(L, sizeof(IOFileUD));
//   iof->type = IOFILE_TYPE_FILE;
//   udataV(L->top-1) -> udtype = UDTYPE_IO_FILE;
//   iof->fp = 0;
//   return 1;
// }

static void lua_pushcfile(lua_State *L, FILE * f){
  if (!file_handler_creator(L, "COMSPEC", 1)) return;
  int x = push_null_file_handler(L);
  if (x != 1) return ;
  luaL_getmetatable(L, LUA_FILEHANDLE);
  lua_setmetatable(L, -2);
  FILE** iof = (FILE**)lua_touserdata(L, -1);
  if (iof) *iof = f;
}

#endif // USE_LUAJIT

