
-- utility

local function test(a,b)
  local function conv(c)
    local i=string.byte(c)
    if i<32 or i>127 then
      return '\\x'..string.format('%X',i)
    end
    return c
  end
  local i = debug.getinfo(2)
  print('CHECK(line:'..tostring(i.currentline)..')', tostring(a):gsub('.',conv), '<->', tostring(b):gsub('.',conv))
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

test(expect:gsub('[\n\r]*$',''), got:gsub('[\n\r]*$',''))

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

test(expect:gsub('[\n\r]*$',''), got:gsub('[\n\r]*$',''))

expect = 'hello world ' .. tostring(math.random())

local r,w = lc.pipe()
local p=lc.spawn{lua,'-e','print(os.getenv("TESTVAR"))', stdout=w, env={PATH=os.getenv("PATH"),LD_LIBRARY_PATH=os.getenv("LD_LIBRARY_PATH"),TESTVAR=expect}}
w:close()
p:wait()
got = r:read("*l")

test(expect:gsub('[\n\r]*$',''), got:gsub('[\n\r]*$',''))

-- Sub-process result

local function readall()
  local f = io.open("tmp.out.txt","rb")
  local size = f:seek('end',0)
  f:seek('set',0)
  local r = f:read(size)
  f:close()
  return r
end

local argdump = [[f=io.open('tmp.out.txt','wb') for i=0,#arg do f:write(arg[i],string.char(10)) end f:close()]]

local r,w = lc.pipe()
local p=lc.spawn{lua,'-e','os.exit(123)',stdout=w}
w:close()
local result = p:wait()

test(result, 123)

-- Passing any character to the child process

for c = 0, 255 do
  local s = string.char(c)
  if  c ~= 0  -- \0 - C string terminator issue
  and c ~= 42 and c ~= 63 -- windows wildcard * and ? have issues
  then
    lc.spawn({lua,'-e',argdump, '--', 'o', s, 'o', stderr=io.stdout, stdout=io.stdout}):wait()
    local r = readall()
    test(r, 'o\n'..s..'\no\n')
  end
end

-- Passing some back slash patterns to the child process (sensible under windows shenarios)

lc.spawn({lua,'-e',argdump, '--', 'o', '\\', 'o', stderr=io.stdout, stdout=io.stdout}):wait()
local r = readall()
test(r, 'o\n\\\no\n')

lc.spawn({lua,'-e',argdump, '--', 'o', '\\\\', 'o', stderr=io.stdout, stdout=io.stdout}):wait()
local r = readall()
test(r, 'o\n\\\\\no\n')

lc.spawn({lua,'-e',argdump, '--', 'o', '\\n', 'o', stderr=io.stdout, stdout=io.stdout}):wait()
local r = readall()
test(r, 'o\n\\n\no\n')

lc.spawn({lua,'-e',argdump, '--', 'o', '\\\\n', 'o', stderr=io.stdout, stdout=io.stdout}):wait()
local r = readall()
test(r, 'o\n\\\\n\no\n')

lc.spawn({lua,'-e',argdump, '--', 'o', '\\"o', 'o', stderr=io.stdout, stdout=io.stdout}):wait()
local r = readall()
test(r, 'o\n\\"o\no\n')

lc.spawn({lua,'-e',argdump, '--', 'o', '\\\\"o', 'o', stderr=io.stdout, stdout=io.stdout}):wait()
local r = readall()
test(r, 'o\n\\\\"o\no\n')

-- FULL !

local lc = require 'luachild'

local env_dump = ''
for k in pairs(lc.environ()) do
  env_dump = env_dump .. ' ' .. k
end

lc.setenv("ENV_DUMP", env_dump)

local r,w = lc.pipe()
local p = lc.spawn { lua, '-e', 'print(os.getenv("ENV_DUMP"))', stdout = w }
local result = p:wait()
w:close()

local o = r:read('*l')
print(o)
print('process returend:',result)

test(o:gsub('[\n\r]*$',''), env_dump:gsub('[\n\r]*$',''))
test(o:match("PATH"), "PATH")
test(result, 0)

-- done

print("All is right")

