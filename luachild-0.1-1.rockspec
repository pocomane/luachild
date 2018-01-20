package = "luachild"
version = "0.1-1"
source = {
  url = "git://github.com/pocomane/luachild",
}
description = {
  summary = "A lua library to spawn programs",
  homepage = "https://github.com/pocomane/luachil",
  license = "MIT",
}
supported_platforms = {
  "unix",
  "windows",
}
dependencies = {
  "lua >= 5.1, < 5.4",
}
build = {
  platforms = {
    unix = {
      type = "builtin",
      modules = {
        ["luachild"] = {
          defines = { "USE_POSIX" },
          incdirs = { "./" },
          sources = { "luachild.c" },
        },
      },
    },
    windows = {
      type = "builtin",
      modules = {
        ["luachild"] = {
          defines = { "USE_WINDOWS" },
          incdirs = { "./" },
          sources = { "luachild.c" },
        },
      },
    },
  },
}
