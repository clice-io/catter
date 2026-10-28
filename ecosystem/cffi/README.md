# cffi

A Python package with a C extension, built by `pip wheel` as a developer of
the extension builds it, in its checkout: setuptools compiles and links
`_cffi_backend`, and cffi's setup.py first compiles probes of the compiler
in temporary directories it removes, which get no entries, while the
extension's sources keep theirs.
