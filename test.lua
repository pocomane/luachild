
-- utility

local function test(a,b)
  if type(a) == 'string' then
    a = tostring(a):gsub('[\n\r]*$','')
  end
  if type(b) == 'string' then
    b = tostring(b):gsub('[\n\r]*$','')
  end
  local i = debug.getinfo(2)
  print('CHECK(line:'..tostring(i.currentline)..')', tostring(a), '<->', tostring(b))
  local ret,err = pcall(function()
    assert(a == b)
  end)
  if err then
    print('TEST FAIL')
    error(err, 2)
  end
  print('OK')
end

local lua = 'lua'
local i = 0
while arg[i] do
  lua = arg[i]
  i = i - 1
end

local count, expect, got

-- Module import

local lc = require "luachild"

test('table', type(lc))
test('function', type(lc.setenv))
test('function', type(lc.environ))
test('function', type(lc.pipe))
test('function', type(lc.spawn))

-- Env iter


got = lc.environ()

count = 0
test(type(got), 'table')
for k,v in pairs(got) do
  count = count + 1
  if not v:match('[cC]:\\') then -- names could not match for windows directory ?
    test(v, os.getenv(k))
  end
end
test(true, count > 0)

-- Env set

expect = 'hello world ' .. tostring(math.random())

got = lc.setenv('TESTVAR', expect)

test(expect, lc.environ()['TESTVAR'])

lc.setenv('TESTVAR')
got = lc.environ()['TESTVAR']

test(nil, got)

-- Pipe

expect = 'hello world ' .. tostring(math.random())

local r,w = lc.pipe()
w:write(expect)
w:close()
got = r:read("*l")

test(expect, got)

expect = 'hello world ' .. tostring(math.random())

local r,w = lc.pipe()
w:write(expect)
w:close()
got = r:read("*l")

test(expect, got)

-- Spawn

expect = 'hello world ' .. tostring(math.random())

local r,w = lc.pipe()
local p=lc.spawn{lua,'-e','print("' .. expect .. '")',stdout=w}
w:close()
p:wait()
got = r:read("*l")

test(expect, got)

expect = 'hello world ' .. tostring(math.random())

local r,w = lc.pipe()
local R,W = lc.pipe()
local p=lc.spawn{lua,'-e','print("' .. expect .. '")',stdin=r, stdout=W}
w:write(expect)
w:close()
W:close()
p:wait()
got = r:read("*l")

test(expect, got)

-- Spawn env

expect = 'hello world ' .. tostring(math.random())

lc.setenv('TESTVAR', expect)
local r,w = lc.pipe()
local p=lc.spawn{lua,'-e','print(os.getenv("TESTVAR"))', stdout=w}
w:close()
p:wait()
got = r:read("*l")

test(expect, got)

expect = 'hello world ' .. tostring(math.random())

local r,w = lc.pipe()
local p=lc.spawn{lua,'-e','print(os.getenv("TESTVAR"))', stdout=w, env={PATH=os.getenv("PATH"),LD_LIBRARY_PATH=os.getenv("LD_LIBRARY_PATH"),TESTVAR=expect}}
w:close()
p:wait()
got = r:read("*l")

test(expect, got)

-- Sub-process result

local r,w = lc.pipe()
local p=lc.spawn{lua,'-e','os.exit(-123)',stdout=w}
w:close()
local result = p:wait()

test(result, -123)

-- FULL !

local lc = require 'luachild'

local env_dump = ''
for k in pairs(lc.environ()) do
  env_dump = env_dump .. ' ' .. k
end

lc.setenv("ENV_DUMP", env_dump)

local r,w = lc.pipe()
local p = lc.spawn { './lua.exe', '-e', 'print(os.getenv("ENV_DUMP"))', stdout = w }
local result = p:wait()
w:close()

local o = r:read('*l')
print(o)
print('process returend:',result)

test(o, env_dump)
test(o:match("PATH"), "PATH")
test(result, 0)

-- done

print("All is right")

