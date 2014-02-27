# OHM


#### Dependencies

  * libelf
  * libdwarf   (for debug information)
  * libunwind  (for stack unwinding)
  * lua
  * CMA, XPMEM (optional; for optimized IPC)

---

#### Notes

If you are using a relatively newer compiler (say, GCC 4.8.1), it
is highly likely that libdwarf shipped with Ubuntu/Debian distros
would not work correctly. This is due to incompatibility between
the DWARF4 extensions emitted by the compiler, and that supported
by libdwarf. In such a case, use the internal libdwarf:

```bash
$ git submodule init
$ git submodule update
$ ./autogen.sh && ./configure --enable-internal-libdwarf
```

