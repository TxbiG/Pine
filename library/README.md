# Pine standard library

Import a module with `#import <name>`; the compiler searches `$PINE_LIBRARY`, then `library/`
next to the importing file, then `./library`.

| Module | Purpose |
| --- | --- |
| `<str>` | C-string helpers: `str_len`, `str_at`, `str_copy`, `str_eq_span` |
| `<io>`  | Console and file I/O: `io_println`, `io_open`, `io_read`, `io_read_line`, `io_write`, `io_read_file`, `io_write_file`, ... |
| `<net>` | TCP and UDP sockets (IPv4/IPv6): connect/listen/accept, `net_send_to` / `net_recv_from`, multicast, `net_poll`, socket options, DNS and address helpers, byte-order packing |
| `<json>` | Allocation-free JSON parser (`json_doc` / `json_parse` / `json_get` / `json_at` / `json_string`) and writer (`json_w_*`) |
| `<math>`, `<thread>`, ... | Pre-existing modules |

`str`, `io`, `net` and `json` use external functions implemented in `runtime/pine_rt.c`, so link it:

```sh
pine main.pine > main.c
cc main.c library/runtime/pine_rt.c -o main            # Linux / macOS
cc main.c library/runtime/pine_rt.c -o main -lws2_32   # Windows (MinGW)
```

Buffers are byte slices (`[]u8`); pass a fixed array such as `u8 buf[256];`. Functions report
failure with `-1` (or a negative `JSON_ERR_*`); `net_last_error()` returns the OS error code.

Run the library tests with `tests/library/run.sh /path/to/pine`.

## Known limits

- No floating-point literals in the language yet, so JSON numbers are read as integers
  (`json_to_i64`); `json_is_integer` tells you when a fraction/exponent was dropped.
- The JSON parser validates brackets, escapes, literals and number syntax but not comma/colon
  grammar between values (`{"a" 1}` is accepted).
- `<net>` has no TLS, raw sockets or Unix-domain sockets, and no HTTP client.
- Not exercised by the tests: IPv6 (the build machine had no IPv6 loopback), a successful multicast join, and the Windows (Winsock) code paths, which have not been compiled or run.
