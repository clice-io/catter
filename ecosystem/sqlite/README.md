# SQLite

A build that makes its own sources: configure (autosetup, which compiles
jimsh when no tclsh is there), then programs the build compiles and runs at
once (lemon, mkkeywordhash, mksourceid) and Tcl scripts that write the
amalgamation, `sqlite3.c`, which the library and the shell are compiled
from.
