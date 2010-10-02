option(SCRIPTS          "Build core with scripts included"                            1)
option(COREDEBUG        "Build with additional debug-code included"                   0)
option(SCRIPTPCH        "Use precompiled headers when compiling scripts"              1)
option(GAMEPCH          "Use precompiled headers when compiling game project"         1)
if(WIN32)
  option(USE_MYSQL_SOURCES "Use your own installed MySQL instead of the internal one" 0)
else(WIN32)
  set(USE_MYSQL_SOURCES 0)
endif(WIN32)
option(WARNINGS         "Enable all compile-warnings during compile"                  0)