/*
This software is licensed under the terms of the MIT license reproduced below.

===============================================================================

Copyright 2006-2007 Mark Edgar <medgar123@gmail.com>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.

===============================================================================

*/

#include "luachild.h"
#ifdef USE_WINDOWS

#define WIN32_LEAN_AND_MEAN 1
#define NOGDI 1

#include <stdlib.h>
#include <windows.h>
#include <fcntl.h>

#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

#define absindex(L,i) ((i)>0?(i):lua_gettop(L)+(i)+1)

#define debug(...) /* fprintf(stderr, __VA_ARGS__) */
#define debug_stack(L) /* #include "../lds.c" */

#define file_handle(fp) (HANDLE)_get_osfhandle(fileno(fp))

/* Push nil, followed by the Windows error message corresponding to
 * the error number, or a string giving the error value in decimal if
 * no error message is found.  If nresults is -2, always push nil and
 * the error message and return 2 even if error is NO_ERROR.  If
 * nresults is -1 and error is NO_ERROR, then push true and return 1.
 * Otherwise, if error is NO_ERROR, return nresults.
 */
static int windows_pusherror(lua_State *L, DWORD error, int nresults)
{
  if (error != NO_ERROR || nresults == -2) {
    char buffer[1024];
    size_t len = sprintf(buffer, "%lu (0x%lX): ", error, error);
    size_t res = FormatMessage(
      FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_FROM_SYSTEM,
      0, error, 0, buffer + len, sizeof buffer - len, 0);
    if (res) {
      len += res;
      while (len > 0 && isspace(buffer[len - 1]))
        len--;
    }
    else {
      len += sprintf(buffer + len, "<error string not available>");
    }
    lua_pushnil(L);
    lua_pushlstring(L, buffer, len);
    nresults = 2;
  }
  else if (nresults < 0) {
    lua_pushboolean(L, 1);
    nresults = 1;
  }
  return nresults;
}

static int push_error(lua_State *L) {
 return windows_pusherror(L, GetLastError(), -2);
}

/* ----------------------------------------------------------------------------- */

/* name value -- true/nil error
 * name nil -- true/nil error */
int lc_setenv(lua_State *L)
{
  const char *nam = luaL_checkstring(L, 1);
  const char *val = lua_tostring(L, 2);
  if (!SetEnvironmentVariable(nam, val))
    return push_error(L);
  lua_pushboolean(L, 1);
  return 1;
}

/* -- environment-table */
int lc_environ(lua_State *L)
{
  const char *nam, *val, *end;
  const char *envs = GetEnvironmentStrings();
  if (!envs) return push_error(L);
  lua_newtable(L);
  for (nam = envs; *nam; nam = end + 1) {
    end = strchr(val = strchr(nam, '=') + 1, '\0');
    lua_pushlstring(L, nam, val - nam - 1);
    lua_pushlstring(L, val, end - val);
    lua_settable(L, -3);
  }
  return 1;
}

int lc_pipe(lua_State *L)
{
  if (!file_handler_creator(L, "COMSPEC", 1)) return 0;
  HANDLE ph[2];
  if (!CreatePipe(ph + 0, ph + 1, 0, 0))
    return push_error(L);
  SetHandleInformation(ph[0], HANDLE_FLAG_INHERIT, 0);
  SetHandleInformation(ph[1], HANDLE_FLAG_INHERIT, 0);
  lua_pushcfile(L, _fdopen(_open_osfhandle((long)ph[0], _O_RDONLY), "r"));
  lua_pushcfile(L, _fdopen(_open_osfhandle((long)ph[1], _O_WRONLY), "w"));
  return 2;
}

/* ----------------------------------------------------------------------------- */

/* -- in out/nil error */
struct spawn_params {
  lua_State *L;
  const char *cmdline;
  const char *environment;
  STARTUPINFO si;
};

static int need_quote(const char *s, size_t l){
  int i = 0;
  for (i=0;i<l;i++) {
    const char c = s[i];
    if (c < ' ' || c > '~') return 1; // ASCII Control OR non 7 bit
    if (c == ' ' || c == '\\' || c == '"' ) return 1;
  }
  return 0;
}

static void add_back_slash(luaL_Buffer *b, int bscount) {
  int i;
  for (i=0; i<bscount; i++)
    luaL_addchar(b, '\\');
}

/* quotes and adds argument string to b */
static void quote_argument(luaL_Buffer *b, const char *s){
  int bscount = 0;
  luaL_addchar(b, '"');
  for (; *s; s++) {
    switch (*s) {
    case '\\':
      bscount += 1;
      break;
    case '"':
      add_back_slash(b, 2*bscount+1);
      luaL_addchar(b, '"');
      bscount = 0;
      break;
    default:
      add_back_slash(b, bscount);
      luaL_addchar(b, *s);
      bscount = 0;
      break;
    }
  }
  add_back_slash(b, 2*bscount);
  luaL_addchar(b, '"');
}

static void add_argument(luaL_Buffer *b, const char *s) {
  size_t l = strlen(s);
  // Note: some windows applications parse directly the whole command line
  // through the window API. So, for compatibility it is better to quote the
  // arguments only when it is really needed. E.g. 'dir /?' will print help
  // informations, while 'dir "/?"' will print an error.
  if (need_quote(s,l)) {
    quote_argument(b,s);
  } else {
    luaL_addlstring(b,s,l);
  }
}

static struct spawn_params *spawn_param_init(lua_State *L)
{
  static const STARTUPINFO si = {sizeof si};
  struct spawn_params *p = lua_newuserdata(L, sizeof *p);
  p->L = L;
  p->cmdline = p->environment = 0;
  p->si = si;
  return p;
}

/* cmd ... -- cmd ... */
static void spawn_param_filename(struct spawn_params *p)
{
  lua_State *L = p->L;
  luaL_Buffer b;
  luaL_buffinit(L, &b);
  add_argument(&b, lua_tostring(L, 1));
  luaL_pushresult(&b);
  lua_replace(L, 1);
  p->cmdline = lua_tostring(L, 1);
}

/* cmd ... argtab -- cmdline ... */
static void spawn_param_args(struct spawn_params *p)
{
  lua_State *L = p->L;
  int argtab = lua_gettop(L);
  size_t i, n = lua_value_length(L, argtab);
  luaL_Buffer b;
  debug("spawn_param_args:"); debug_stack(L);
  lua_pushnil(L);                 /* cmd opts ... argtab nil */
  luaL_buffinit(L, &b);           /* cmd opts ... argtab nil b... */
  lua_pushvalue(L, 1);            /* cmd opts ... argtab nil b... cmd */
  luaL_addvalue(&b);              /* cmd opts ... argtab nil b... */
  /* concatenate the arg array to a string */
  for (i = 1; i <= n; i++) {
    const char *s;
    lua_rawgeti(L, argtab, i);    /* cmd opts ... argtab nil b... arg */
    lua_replace(L, argtab + 1);   /* cmd opts ... argtab arg b... */
    luaL_addchar(&b, ' ');
    /* XXX checkstring is confusing here */
    s = lua_tostring(L, argtab + 1);
    if (!s) {
      luaL_error(L, "expected string for argument %d, got %s",
                 i, lua_typename(L, lua_type(L, argtab + 1)));
      return;
    }
    add_argument(&b, luaL_checkstring(L, argtab + 1));
  }
  luaL_pushresult(&b);            /* cmd opts ... argtab arg cmdline */
  lua_replace(L, 1);              /* cmdline opts ... argtab arg */
  lua_pop(L, 2);                  /* cmdline opts ... */
  p->cmdline = lua_tostring(L, 1);
}

/* ... tab nil nil [...] -- ... tab envstr */
static char *add_env(lua_State *L, int tab, size_t where) {
  char *t;
  lua_checkstack(L, 2);
  lua_pushvalue(L, -2);
  if (lua_next(L, tab)) {
    size_t klen, vlen;
    const char *key = lua_tolstring(L, -2, &klen);
    const char *val = lua_tolstring(L, -1, &vlen);
    t = add_env(L, tab, where + klen + vlen + 2);
    memcpy(&t[where], key, klen);
    t[where += klen] = '=';
    memcpy(&t[where + 1], val, vlen + 1);
  }
  else {
    t = lua_newuserdata(L, where + 1);
    t[where] = '\0';
    lua_replace(L, tab + 1);
  }
  return t;
}

/* ... envtab -- ... envtab envstr */
static void spawn_param_env(struct spawn_params *p)
{
  lua_State *L = p->L;
  int envtab = lua_gettop(L);
  lua_pushnil(L);
  lua_pushnil(L);
  p->environment = add_env(L, envtab, 0);
  lua_settop(L, envtab + 1);
}

static void spawn_param_redirect(struct spawn_params *p, const char *stdname, HANDLE h)
{
  SetHandleInformation(h, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
  if (!(p->si.dwFlags & STARTF_USESTDHANDLES)) {
    p->si.hStdInput  = GetStdHandle(STD_INPUT_HANDLE);
    p->si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    p->si.hStdError  = GetStdHandle(STD_ERROR_HANDLE);
    p->si.dwFlags |= STARTF_USESTDHANDLES;
  }
  switch (stdname[3]) {
  case 'i': p->si.hStdInput = h; break;
  case 'o': p->si.hStdOutput = h; break;
  case 'e': p->si.hStdError = h; break;
  }
}

struct process {
  int status;
  HANDLE hProcess;
  DWORD dwProcessId;
};

static int spawn_param_execute(struct spawn_params *p)
{
  lua_State *L = p->L;
  char *c, *e;
  PROCESS_INFORMATION pi;
  BOOL ret;
  struct process *proc = lua_newuserdata(L, sizeof *proc);
  luaL_getmetatable(L, PROCESS_HANDLE);
  lua_setmetatable(L, -2);
  proc->status = -1;
  c = strdup(p->cmdline);
  e = (char *)p->environment; /* strdup(p->environment); */
  /* XXX does CreateProcess modify its environment argument? */
  ret = CreateProcess(0, c, 0, 0, TRUE, 0, e, 0, &p->si, &pi);
  /* if (e) free(e); */
  free(c);
  if (!ret)
    return push_error(L);
  proc->hProcess = pi.hProcess;
  proc->dwProcessId = pi.dwProcessId;
  return 1;
}

/* proc -- exitcode/nil error */
int process_wait(lua_State *L)
{
  struct process *p = luaL_checkudata(L, 1, PROCESS_HANDLE);
  if (p->status == -1) {
    DWORD exitcode;
    if (WAIT_FAILED == WaitForSingleObject(p->hProcess, INFINITE)
        || !GetExitCodeProcess(p->hProcess, &exitcode))
      return push_error(L);
    p->status = exitcode;
  }
  lua_pushnumber(L, p->status);
  return 1;
}

/* proc -- string */
int process_tostring(lua_State *L)
{
  struct process *p = luaL_checkudata(L, 1, PROCESS_HANDLE);
  char buf[40];
  lua_pushlstring(L, buf,
    sprintf(buf, "process (%lu, %s)", (unsigned long)p->dwProcessId,
      p->status==-1 ? "running" : "terminated"));
  return 1;
}

static FILE *check_file(lua_State *L, int idx, const char *argname)
{
  FILE **pf;
  if (idx > 0) pf = luaL_checkudata(L, idx, LUA_FILEHANDLE);
  else {
    idx = absindex(L, idx);
    pf = lua_touserdata(L, idx);
    luaL_getmetatable(L, LUA_FILEHANDLE);
    if (!pf || !lua_getmetatable(L, idx) || !lua_rawequal(L, -1, -2))
      luaL_error(L, "bad %s option (%s expected, got %s)",
                 argname, LUA_FILEHANDLE, luaL_typename(L, idx));
    lua_pop(L, 2);
  }
  if (!*pf) return luaL_error(L, "attempt to use a closed file"), NULL;
  return *pf;
}

static void get_redirect(lua_State *L,
                         int idx, const char *stdname, struct spawn_params *p)
{
  lua_getfield(L, idx, stdname);
  if (!lua_isnil(L, -1))
    spawn_param_redirect(p, stdname, file_handle(check_file(L, -1, stdname)));
  lua_pop(L, 1);
}

/* filename [args-opts] -- proc/nil error */
/* args-opts -- proc/nil error */
int lc_spawn(lua_State *L)
{
  struct spawn_params *params;
  int have_options;
  switch (lua_type(L, 1)) {
  default: return lua_report_type_error(L, 1, "string or table");
  case LUA_TSTRING:
    switch (lua_type(L, 2)) {
    default: return lua_report_type_error(L, 2, "table");
    case LUA_TNONE: have_options = 0; break;
    case LUA_TTABLE: have_options = 1; break;
    }
    break;
  case LUA_TTABLE:
    have_options = 1;
    lua_getfield(L, 1, "command");      /* opts ... cmd */
    if (!lua_isnil(L, -1)) {
      /* convert {command=command,arg1,...} to command {arg1,...} */
      lua_insert(L, 1);                 /* cmd opts ... */
    }
    else {
      /* convert {arg0,arg1,...} to arg0 {arg1,...} */
      size_t i, n = lua_value_length(L, 1);
      lua_rawgeti(L, 1, 1);             /* opts ... nil cmd */
      lua_insert(L, 1);                 /* cmd opts ... nil */
      for (i = 2; i <= n; i++) {
        lua_rawgeti(L, 2, i);           /* cmd opts ... nil argi */
        lua_rawseti(L, 2, i - 1);       /* cmd opts ... nil */
      }
      lua_rawseti(L, 2, n);             /* cmd opts ... */
    }
    if (lua_type(L, 1) != LUA_TSTRING)
      return luaL_error(L, "bad command option (string expected, got %s)",
                        luaL_typename(L, 1));
    break;
  }
  params = spawn_param_init(L);
  /* get filename to execute */
  spawn_param_filename(params);
  /* get arguments, environment, and redirections */
  if (have_options) {
    lua_getfield(L, 2, "args");         /* cmd opts ... argtab */
    switch (lua_type(L, -1)) {
    default:
      return luaL_error(L, "bad args option (table expected, got %s)",
                        luaL_typename(L, -1));
    case LUA_TNIL:
      lua_pop(L, 1);                    /* cmd opts ... */
      lua_pushvalue(L, 2);              /* cmd opts ... opts */
      if (0) /*FALLTHRU*/
    case LUA_TTABLE:
      if (lua_value_length(L, 2) > 0)
        return
          luaL_error(L, "cannot specify both the args option and array values");
      spawn_param_args(params);         /* cmd opts ... */
      break;
    }
    lua_getfield(L, 2, "env");          /* cmd opts ... envtab */
    switch (lua_type(L, -1)) {
    default:
      return luaL_error(L, "bad env option (table expected, got %s)",
                        luaL_typename(L, -1));
    case LUA_TNIL:
      break;
    case LUA_TTABLE:
      spawn_param_env(params);          /* cmd opts ... */
      break;
    }
    get_redirect(L, 2, "stdin", params);    /* cmd opts ... */
    get_redirect(L, 2, "stdout", params);   /* cmd opts ... */
    get_redirect(L, 2, "stderr", params);   /* cmd opts ... */
  }
  return spawn_param_execute(params);   /* proc/nil error */
}

#endif // USE_WINDOWS

