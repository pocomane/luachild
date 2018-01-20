
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

#include "luachild.h"

int set_table_field(lua_State *L, const char * field_name){
  lua_pushstring(L, field_name);
  lua_insert(L, -2);
  lua_settable(L, -3);
  return 0;
}

int luaopen_luachild(lua_State *L)
{
  
  /* Process methods */

  luaL_newmetatable(L, PROCESS_HANDLE);

  lua_pushcfunction(L, process_tostring);
  set_table_field(L, "__tostring");

  lua_pushcfunction(L, process_wait);
  set_table_field(L, "wait");

  lua_pushvalue(L, -1);
  lua_setfield(L, -2, "__index");

  /* Top module functions */

  lua_newtable(L);
 
  lua_pushcfunction(L, lc_pipe);
  set_table_field(L, "pipe");

  lua_pushcfunction(L, lc_setenv);
  set_table_field(L, "setenv");

  lua_pushcfunction(L, lc_environ);
  set_table_field(L, "environ");

  lua_pushcfunction(L, lc_spawn);
  set_table_field(L, "spawn");

  lua_pushcfunction(L, process_wait);
  set_table_field(L, "wait");

  return 1;
}

