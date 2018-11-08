#include "luachild.h"
#ifdef USE_POSIX

/*
 * Extracted from "ex" API implementation
 * http://lua-users.org/wiki/ExtensionProposal
 * Copyright 2007 Mark Edgar < medgar at gmail com >
 */

#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include <fcntl.h>
#include <sys/wait.h>

#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

#if __STDC_VERSION__ < 199901L
#define restrict
#endif

#ifdef __APPLE__

#include <crt_externs.h>
#define environ (*_NSGetEnviron())

#else

extern char **environ;

#endif

#define absindex(L,i) ((i)>0?(i):lua_gettop(L)+(i)+1)

#ifndef OPEN_MAX
#define OPEN_MAX sysconf(_SC_OPEN_MAX)
#endif

/* -- nil error */
extern int push_error(lua_State *L)
{
  lua_pushnil(L);
  lua_pushstring(L, strerror(errno));
  return 2;
}

/* ----------------------------------------------------------------------------- */

/* name value -- true/nil error
 * name nil -- true/nil error */
static int lc_setenv(lua_State *L)
{
  const char *nam = luaL_checkstring(L, 1);
  const char *val = lua_tostring(L, 2);
  int err = val ? setenv(nam, val, 1) : unsetenv(nam);
  if (err == -1) return push_error(L);
  lua_pushboolean(L, 1);
  return 1;
}

/* -- environment-table */
static int lc_environ(lua_State *L)
{
  const char *nam, *val, *end;
  const char **env;
  lua_newtable(L);
  for (env = (const char **)environ; (nam = *env); env++) {
    end = strchr(val = strchr(nam, '=') + 1, '\0');
    lua_pushlstring(L, nam, val - nam - 1);
    lua_pushlstring(L, val, end - val);
    lua_settable(L, -3);
  }
  return 1;
}

static int closeonexec(int d)
{
  int fl = fcntl(d, F_GETFD);
  if (fl != -1)
    fl = fcntl(d, F_SETFD, fl | FD_CLOEXEC);
  return fl;
}

/* -- in out/nil error */
static int lc_pipe(lua_State *L)
{
  if (!file_handler_creator(L, "/dev/null", 0)) return 0;
  int fd[2];
  if (-1 == pipe(fd))
    return push_error(L);
  closeonexec(fd[0]);
  closeonexec(fd[1]);
  lua_pushcfile(L, fdopen(fd[0], "r"));
  lua_pushcfile(L, fdopen(fd[1], "w"));
  return 2;
}

/* ----------------------------------------------------------------------------- */

#ifndef INTERNAL_SPAWN_API
#include <spawn.h>
#else

typedef void *posix_spawnattr_t;

typedef struct posix_spawn_file_actions posix_spawn_file_actions_t;
struct posix_spawn_file_actions {
  int dups[3];
};

static int posix_spawn_file_actions_destroy(
  posix_spawn_file_actions_t *act)
{
  return 0;
}

static int posix_spawn_file_actions_adddup2(
  posix_spawn_file_actions_t *act,
  int d,
  int n)
{
  /* good faith effort to determine validity of descriptors */
  if (d < 0 || OPEN_MAX < d || n < 0 || OPEN_MAX < n) {
    errno = EBADF;
    return -1;
  }
  /* we only support duplication to 0,1,2 */
  if (2 < n) {
    errno = EINVAL;
    return -1;
  }
  act->dups[n] = d;
  return 0;
}

static int posix_spawn_file_actions_init(
  posix_spawn_file_actions_t *act)
{
  act->dups[0] = act->dups[1] = act->dups[2] = -1;
  return 0;
}

static int posix_spawnp(
  pid_t *restrict ppid,
  const char *restrict path,
  const posix_spawn_file_actions_t *act,
  const posix_spawnattr_t *restrict attrp,
  char *const argv[restrict],
  char *const envp[restrict])
{
  if (!ppid || !path || !argv || !envp)
    return EINVAL;
  if (attrp)
    return EINVAL;
  switch (*ppid = fork()) {
  case -1: return -1;
  default: return 0;
  case 0:
    if (act) {
      int i;
      for (i = 0; i < 3; i++)
        if (act->dups[i] != -1 && -1 == dup2(act->dups[i], i))
          _exit(111);
    }
    environ = (char **)envp;
    execvp(path, argv);
    _exit(111);
    /*NOTREACHED*/
  }
}

#endif // INTERNAL_SPAWN_API

struct process {
  int status;
  pid_t pid;
};

/* proc -- exitcode/nil error */
static int process_wait(lua_State *L)
{
  struct process *p = luaL_checkudata(L, 1, PROCESS_HANDLE);
  if (p->status == -1) {
    int status;
    if (-1 == waitpid(p->pid, &status, 0))
      return push_error(L);
    p->status = WEXITSTATUS(status);
  }
  lua_pushnumber(L, p->status);
  return 1;
}

/* proc -- string */
static int process_tostring(lua_State *L)
{
  struct process *p = luaL_checkudata(L, 1, PROCESS_HANDLE);
  char buf[40];
  lua_pushlstring(L, buf,
    sprintf(buf, "process (%lu, %s)", (unsigned long)p->pid,
      p->status==-1 ? "running" : "terminated"));
  return 1;
}

struct spawn_params {
  lua_State *L;
  const char *command, **argv, **envp;
  posix_spawn_file_actions_t redirect;
};

struct spawn_params *spawn_param_init(lua_State *L)
{
  struct spawn_params *p = lua_newuserdata(L, sizeof *p);
  p->L = L;
  p->command = 0;
  p->argv = p->envp = 0;
  posix_spawn_file_actions_init(&p->redirect);
  return p;
}

static void spawn_param_filename(struct spawn_params *p, const char *filename)
{
  p->command = filename;
}

static void spawn_param_redirect(struct spawn_params *p, const char *stdname, int fd)
{
  int d;
  switch (stdname[3]) {
  case 'i': d = STDIN_FILENO; break;
  case 'o': d = STDOUT_FILENO; break;
  case 'e': d = STDERR_FILENO; break;
  }
  posix_spawn_file_actions_adddup2(&p->redirect, fd, d);
}

static int spawn_param_execute(struct spawn_params *p)
{
  lua_State *L = p->L;
  int ret;
  struct process *proc;
  if (!p->argv) {
    p->argv = lua_newuserdata(L, 2 * sizeof *p->argv);
    p->argv[0] = p->command;
    p->argv[1] = 0;
  }
  if (!p->envp)
    p->envp = (const char **)environ;
  proc = lua_newuserdata(L, sizeof *proc);
  luaL_getmetatable(L, PROCESS_HANDLE);
  lua_setmetatable(L, -2);
  proc->status = -1;
  ret = posix_spawnp(&proc->pid, p->command, &p->redirect, 0,
                     (char *const *)p->argv, (char *const *)p->envp);
  posix_spawn_file_actions_destroy(&p->redirect);
  return ret != 0 ? push_error(L) : 1;
}

/* Converts a Lua array of strings to a null-terminated array of char pointers.
 * Pops a (0-based) Lua array and replaces it with a userdatum which is the
 * null-terminated C array of char pointers.  The elements of this array point
 * to the strings in the Lua array.  These strings should be associated with
 * this userdatum via a weak table for GC purposes, but they are not here.
 * Therefore, any function which calls this must make sure that these strings
 * remain available until the userdatum is thrown away.
 */
/* ... array -- ... vector */
static const char **make_vector(lua_State *L)
{
  size_t i, n = lua_value_length(L, -1);
  const char **vec = lua_newuserdata(L, (n + 2) * sizeof *vec);
                                        /* ... arr vec */
  for (i = 0; i <= n; i++) {
    lua_rawgeti(L, -2, i);              /* ... arr vec elem */
    vec[i] = lua_tostring(L, -1);
    if (!vec[i] && i > 0) {
      luaL_error(L, "expected string for argument %d, got %s",
                 i, lua_typename(L, lua_type(L, -1)));
      return 0;
    }
    lua_pop(L, 1);                      /* ... arr vec */
  }
  vec[n + 1] = 0;
  lua_replace(L, -2);                   /* ... vector */
  return vec;
}

/* ... envtab -- ... envtab vector */
static void spawn_param_env(struct spawn_params *p)
{
  lua_State *L = p->L;
  size_t i = 0;
  lua_newtable(L);                      /* ... envtab arr */
  lua_pushliteral(L, "=");              /* ... envtab arr "=" */
  lua_pushnil(L);                       /* ... envtab arr "=" nil */
  for (i = 0; lua_next(L, -4); i++) {   /* ... envtab arr "=" k v */
    if (!lua_tostring(L, -2)) {
      luaL_error(L, "expected string for environment variable name, got %s",
                 lua_typename(L, lua_type(L, -2)));
      return;
    }
    if (!lua_tostring(L, -1)) {
      luaL_error(L, "expected string for environment variable value, got %s",
                 lua_typename(L, lua_type(L, -1)));
      return;
    }
    lua_pushvalue(L, -2);               /* ... envtab arr "=" k v k */
    lua_pushvalue(L, -4);               /* ... envtab arr "=" k v k "=" */
    lua_pushvalue(L, -3);               /* ... envtab arr "=" k v k "=" v */
    lua_concat(L, 3);                   /* ... envtab arr "=" k v "k=v" */
    lua_rawseti(L, -5, i);              /* ... envtab arr "=" k v */
    lua_pop(L, 1);                      /* ... envtab arr "=" k */
  }                                     /* ... envtab arr "=" */
  lua_pop(L, 1);                        /* ... envtab arr */
  p->envp = make_vector(L);             /* ... envtab arr vector */
}

/* ... argtab -- ... argtab vector */
static void spawn_param_args(struct spawn_params *p)
{
  const char **argv = make_vector(p->L);
  if (!argv[0]) argv[0] = p->command;
  p->argv = argv;
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

#define new_dirent(L) lua_newtable(L)
#define DIR_HANDLE "DIR*"

static void get_redirect(lua_State *L,
                         int idx, const char *stdname, struct spawn_params *p)
{
  lua_getfield(L, idx, stdname);
  if (!lua_isnil(L, -1))
    spawn_param_redirect(p, stdname, fileno(check_file(L, -1, stdname)));
  lua_pop(L, 1);
}

/* filename [args-opts] -- proc/nil error */
/* args-opts -- proc/nil error */
static int lc_spawn(lua_State *L)
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
  spawn_param_filename(params, lua_tostring(L, 1));
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

#endif // USE_POSIX

