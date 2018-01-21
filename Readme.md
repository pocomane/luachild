
Luachild
========

Luachild is a lua module to spawn system processes and perform basic
communication with them, i.e. pipes and environment variables set. It is
compatible with lua 5.3 and luajit 2.1 and it works under windows and any os
with posix spawn+environ api (e.g. linux).

The code was extracted from the [LuaDist Ex](https://github.com/LuaDist/luaex)
module. Only few modification were made in order to adapt it to
lua-5.3/luajit-2.1, so the original copyright and license (MIT) was kept.

Build
------

A luarocks specs file is provided, so it is possible to build luachild using
the command line

```
luarocks make
```

Usage
-----

In test.lua there is an example of how to use the module. However, the
following code uses most of the API:

```
local lc = require 'luachild'

local env_dump = ''
for k,v in pairs(lc.environ()) do
  env_dump = env_dump .. ' ' .. k .. '=' .. v .. '\n'
end

lc.setenv("ENV_DUMP", env_dump)

local r,w = lc.pipe()
local p = lc.spawn { 'lua', '-e', 'print(os.getenv("ENV_DUMP"))', stdout = w }
p:wait()
w:close()

print(r:read('*l'))
```

For a quick reference:

`local lc = require 'luachild'` will load the module. After that you will be
able to access to the functions described in the following.

`local e = lc.environ()` will return a table containing all the environment
variables.

`lc.setenv(name, value)` will set the value of the environment variable `name`
to `value`. Both the arguments must be a string. Value can also be `nil`, in
which case the variable will be unset. Note: after this function call,
`lc.environ()` and any child process will get the new value for the variable,
but `os.getenv` will not. This is because lua follows the standard C definition
of the getenv function.

`local r,w = lc.pipe()` will return the two sides of a pipe. You can use `r`
and `w` as normal files: what you write in `w` will be read in `r`

`local process = lc.spawn { 'cmd', 'arg1', 'arg2'}` create a new process
running the command `cmd` with argument `arg1`, `arg2` and so on. The only
argument to `lc.spawn` is a table so you can pass some additional option as
key/value pair. The `stdin`, `stdout` and `stderr` fields can contain file
descriptors to/from which redirect the output/input. It can be a standard file
returned by `io.open` or one of the two result of `lc.pipe()`. The `env` field
can contain a table that describe the environment variables to be set for the
new process. It must be a string-to-string map, and if missing the current env will
be used (the same one returned by `lc.environ()`). The returned value can be
converted to string to get some information about the sub-process.

`lc.wait(process)` or `process:wait()` will wait for the end of the process. It
will return the integer returned by the process.

Notes
-----

Similar modules are:

- [LuaDist Ex](https://github.com/LuaDist/luaex)
- [lunix](https://github.com/wahern/lunix)
- [lua spawn](https://github.com/daurnimator/lua-spawn)
- [lua subprocess](https://github.com/xlq/lua-subprocess)

