# Yar C Framework

[![CI](https://github.com/laruence/yar-c/actions/workflows/ci.yml/badge.svg)](https://github.com/laruence/yar-c/actions/workflows/ci.yml)

A high-performance RPC server/client library written in C.

See also: [Yar PHP framework](https://github.com/laruence/yar), [Yar Java framework](https://github.com/weibocom/yar-java), [Lua Yar framework](https://github.com/fangfengxiang/lua-yar)

## Contents

- [Requirements](#requirements)
- [Install](#install)
- [Quick start](#quick-start)
- [How it works](#how-it-works)
- [Server API](#server-api)
- [Client API](#client-api)
- [Parameters and return values](#parameters-and-return-values)
  - [Response helpers (server side)](#response-helpers-server-side)
  - [Reading the response (client side)](#reading-the-response-client-side)
  - [Unpacking: reading incoming parameters](#unpacking-reading-incoming-parameters)
  - [Packing: building a response](#packing-building-a-response)
- [Packagers](#packagers)
- [Logging and utilities](#logging-and-utilities)
- [Interoperability](#interoperability)
- [License](#license)

## Requirements

- libevent
- [msgpack-c](https://github.com/msgpack/msgpack-c)
- [cJSON](https://github.com/DaveGamble/cJSON) (optional, enables the JSON packager)

## Install

```bash
$ ./configure --with-msgpack=/path/to/msgpack --with-event=/path/to/libevent --with-cjson=/path/to/cjson
$ make
```

This builds `libyar` (the library itself). A runnable server and client live in `example/` — see [Quick start](#quick-start).

## Quick start

A complete server and client live in the `example/` directory. They are the fastest way to understand how Yar works.

### 1. Build the examples

After building the library above, build the two binaries:

```bash
$ cd example
$ make
```

This produces `yar_server` and `yar_client`, both linked against the `libyar` you just built.

### 2. Start the server

```bash
$ ./yar_server -S 127.0.0.1:8888 -X
```

- `-S 127.0.0.1:8888` — listen address (port is mandatory).
- `-X` — standalone mode: run in the foreground as a single process instead of daemonising and pre-forking. Ideal for trying things out.

Leave it running.

### 3. Call it from C

In another shell:

```bash
$ ./yar_client -h tcp://127.0.0.1:8888 -n 2
```

`-n 2` makes two calls to the `default` method. You should see:

```
[OKEY]:{"status" => 0, "parameters" => NULL, "data" => [true, 0.2342, "dummy"]}
[OKEY]:{"status" => 0, "parameters" => NULL, "data" => [true, 0.2342, "dummy"]}
```

### 4. Call it from PHP

Yar C speaks the same protocol as the PHP framework, so a PHP client works against the same server:

```bash
$ php client.php 127.0.0.1:8888
```

`client.php` calls the `default` method with one argument — the array `[1, 2, 'a' => 4]` — and dumps the result. Here `parameters` is no longer `NULL`, because the server echoes the request parameters back:

```php
array(3) {
  ["status"]=>     int(0)
  ["parameters"]=> array(3) { [0]=> int(1) [1]=> int(2) ["a"]=> int(4) }
  ["data"]=>       array(3) { [0]=> bool(true) [1]=> float(0.2342) [2]=> string(5) "dummy" }
}
```

### What just happened

- The **server** registered one RPC method named `default`. When a call arrives, Yar unpacks the parameters into a value tree and hands them to the handler (`yar_handler_example` in `server.c`), which builds a response map with three keys: `status`, `parameters` (the request parameters echoed back), and `data`.
- The **client** opened a persistent connection, invoked `default`, and unpacked the returned map.
- The C client passed **no** arguments, so `parameters` came back `NULL`. The PHP client passed an array, so it came back echoed.

## How it works

A Yar call flows like this:

```
client                                          server
------                                          ------
build args (yar_packager[])
  -> pack method + args with msgpack/JSON
  -> send over TCP / unix socket  -------->  unpack into yar_data tree
                                               dispatch by method name
                                               handler reads params, packs response
  <---------------------------------------  reply with the SAME packager
unpack response (yar_data tree)
inspect status / retval / error
```

Two things worth noting:

- **Parameters and return values are a format-independent value tree (`yar_data`).** msgpack and JSON are just two codecs over the same tree, so handlers never touch the wire format directly.
- **The packager is chosen per request by the client, and the server replies with the same one** — so msgpack and JSON clients can be mixed against a single server.

## Server API

If you want to build a C server with Yar, this is the API to focus on. Yar is built on libevent and msgpack, and provides daemonisation, pre-fork, socket management, logging, and pack/unpack — common building blocks for an RPC server.

### Overview

| Function | Description |
|---|---|
| `yar_server_init(hostname)` | Initialise the server with a listen address |
| `yar_server_set_opt(opt, val)` | Set a server option ([options table](#yar_server_set_opt)) |
| `yar_server_get_opt(opt)` | Read back the current value of an option |
| `yar_server_register_handler(handlers)` | Register RPC methods |
| `yar_server_run()` | Enter the serve loop; blocks until shutdown |
| `yar_server_shutdown(signo)` | Graceful shutdown (takes a signal number, usable as a signal handler) |
| `yar_server_destroy()` | Free server resources |
| `yar_server_print_usage(argv0)` | Print command-line usage to stderr |

The typical setup order is exactly what `example/server.c` does:

```c
if (yar_server_init("127.0.0.1:8888")) {
    yar_server_set_opt(YAR_STAND_ALONE, &standalone);   // options first
    yar_server_set_opt(YAR_MAX_CHILDREN, &max_childs);
    yar_server_set_opt(YAR_CUSTOM_DATA, (void *)1);     // context for handlers
    yar_server_register_handler(handlers);              // then register methods
    yar_server_run();                                   // blocks until shutdown
}
```

### yar_server_init

```c
int yar_server_init(char *hostname);
```

Initialise the server. The argument is a listen address as a string. For IPv4: `localhost:8888` or `127.0.0.1:8888` — the port is mandatory. For Unix domain sockets: `/tmp/yar.sock`.

Returns `1` on success, `0` on failure (including calling it twice — a process can only hold a single server instance, and no instance handle is returned).

### yar_server_set_opt

```c
int yar_server_set_opt(yar_server_opt opt, void *val);
```

Set a server option before calling `yar_server_run()`. Returns `1` on success, `0` on failure.

| Option | `val` points to | Default | Description |
|---|---|---|---|
| `YAR_STAND_ALONE` | `int` | `0` | Non-zero runs as a single foreground process — no daemon, no pre-fork. Debug mode (`-X` in the example) |
| `YAR_READ_TIMEOUT` | `int` (seconds) | `3` | Per-connection request read timeout |
| `YAR_MAX_CHILDREN` | `int` (0–128) | `0` | Number of pre-forked workers. `0` means no pre-fork (single process). Typically the CPU core count |
| `YAR_PARENT_INIT` | `yar_init` function | – | Hook run once in the master process ([details](#process-hooks)) |
| `YAR_CHILD_INIT` | `yar_init` function | – | Hook run in each worker after fork ([details](#process-hooks)) |
| `YAR_CUSTOM_DATA` | any pointer | – | Passed as `data` to the hooks and as the third argument to handlers ([details](#process-hooks)) |
| `YAR_CHILD_USER` | `char *` | – | Workers drop privileges with `setuid()` to this user |
| `YAR_CHILD_GROUP` | `char *` | – | Workers drop privileges with `setgid()` to this group |
| `YAR_PID_FILE` | `char *` | – | Write the master PID to this file |
| `YAR_LOG_FILE` | `char *` | – | Log destination: plain file or cronolog-style pipe ([details](#log-file-and-level)) |
| `YAR_LOG_LEVEL` | `int` | `0` (all) | Minimum level that gets emitted ([details](#log-file-and-level)) |

#### Process hooks

Yar pre-forks worker processes. Two hooks let you run setup code at each stage:

```c
typedef void (*yar_init)(void *data);
```

- `YAR_PARENT_INIT` — called in the master process after initialisation and pre-forking; use it for master-only setup.
- `YAR_CHILD_INIT` — called in every worker process right after forking.

Both receive the pointer previously set with `YAR_CUSTOM_DATA` as their `data` argument — that is the supported way to pass custom context through the server's lifetime. Handlers receive the same pointer as their third argument (`cookie` in `example/server.c`, which asserts it equals `1`).

Note: pass the function pointer itself as `val` (it is cast internally), e.g. `yar_server_set_opt(YAR_PARENT_INIT, (void *)my_init);`.

#### Log file and level

By default Yar runs as a daemon, so log output is not visible on stderr/stdout. `YAR_LOG_FILE` directs logs to a file or pipe. Plain files and cronolog-style pipes are supported:

```
"|/path/to/cronolog ./yar_server_%M.log"
```

This writes logs to `yar_server_*.log` in the current directory, rotated by minute.

`YAR_LOG_LEVEL` is a threshold — a message is emitted only if its level is greater than or equal to the configured value (`YAR_OKEY` request lines are always logged):

| Level | Value |
|---|---|
| `YAR_DEBUG` | `0x1` |
| `YAR_NOTICE` | `0x2` |
| `YAR_WARNING` | `0x4` |
| `YAR_ERROR` | `0x8` |

The default level `0` logs everything.

### yar_server_get_opt

```c
const void *yar_server_get_opt(yar_server_opt opt);
```

Get the current value of a server option.

Returns a pointer to the value on success, `NULL` on failure.

### yar_server_register_handler

```c
int yar_server_register_handler(yar_server_handler *handlers);
```

Register RPC method handlers. The handler signature is:

```c
typedef void (*yar_handler)(yar_request *request, yar_response *response, void *data);
```

The handler descriptor:

```c
typedef struct _yar_server_handler {
    char *name;
    int   len;
    yar_handler handler;
} yar_server_handler;
```

`name` is the RPC method name the client will call. The table is terminated by a `{NULL, 0, NULL}` entry. Example:

```c
yar_server_handler example_handlers[] = {
    {"default", sizeof("default") - 1, yar_handler_example},
    {NULL, 0, NULL}
};
```

When a client calls the `default` method, `yar_handler_example` is invoked to process it. From a PHP client:

```php
<?php
$yar = new Yar_Client("tcp://127.0.0.1");
$yar->default($args); // yar_handler_example handles this request
```

Returns `1` on success, `0` on failure.

### yar_server_run

```c
int yar_server_run();
```

Start the server. This begins the pre-fork, listen, accept, and process loop.

This call does not return unless the server is shut down.

### yar_server_shutdown

```c
void yar_server_shutdown(int signo);
```

Gracefully shut down the server. Stops accepting new requests; each worker exits after finishing its current request. The `signo` argument lets it be installed directly as a signal handler.

### yar_server_destroy

```c
void yar_server_destroy();
```

Destroy the server instance and free resources.

## Client API

If you want to call an existing Yar Server from C, `Yar_Client` is what you need.

> **Note**: the Yar C client only supports **TCP** and **Unix domain sockets** (IPv4 only). HTTP/HTTPS targets are not supported — `yar_client_init()` returns `NULL` for URLs starting with `http://` or `https://`.

### Overview

| Function | Description |
|---|---|
| `yar_client_init(hostname)` | Create a client for a `tcp://host:port`, `host:port` or unix-socket target |
| `client->call(client, method, num_args, args)` | Call a remote method; returns a `yar_response *` |
| `yar_client_set_opt(client, opt, val)` | Set a client option ([options table](#yar_client_set_opt)) |
| `yar_client_get_opt(client, opt)` | Read back the current value of an option |
| `yar_client_destroy(client)` | Free the client |

Inspecting and freeing the result of a call is covered in [Reading the response](#reading-the-response-client-side).

### yar_client_init

```c
yar_client *yar_client_init(char *hostname);
```

Create a client instance. The argument is the target server address:

- `"tcp://127.0.0.1:8888"` or `"127.0.0.1:8888"` — the `tcp://` scheme is accepted (and stripped) so the same URI works for the PHP and C clients.
- `"/tmp/yar.sock"` — a path starting with `/` is treated as a Unix domain socket.

Returns a `yar_client` pointer on success, `NULL` on failure. The struct:

```c
typedef struct _yar_client {
    int fd;
    char *hostname;
    int persistent;
    int timeout;
    int packager;
    yar_client_call call;
} yar_client;
```

In practice you only need the `call` function pointer:

```c
typedef yar_response *(*yar_client_call)(yar_client *client, char *method, uint num_args, yar_packager *packager[]);
```

Each argument to the remote method is itself a `yar_packager` built with the [packing API](#packing-building-a-response); pass them as a C array:

```c
yar_client *client = yar_client_init("tcp://localhost:2222");

/* no arguments */
yar_response *response = client->call(client, "default", 0, NULL);

/* two arguments, each one a packager */
yar_packager *args[2] = {arg1_packager, arg2_packager};
yar_response *response = client->call(client, "default", 2, args);
```

### yar_client_set_opt

```c
int yar_client_set_opt(yar_client *client, yar_client_opt opt, void *val);
```

Set a client option. Returns `1` on success, `0` on failure.

| Option | `val` points to | Default | Description |
|---|---|---|---|
| `YAR_PERSISTENT_LINK` | `int` | `0` | Non-zero keeps the connection alive between calls |
| `YAR_CONNECT_TIMEOUT` | `int` (seconds) | `1` | Timeout applied to connect / send / receive waits |
| `YAR_OPT_PACKAGER` | `int` | `YAR_PACKAGER_MSGPACK` | Wire format: `YAR_PACKAGER_MSGPACK` (`0`) or `YAR_PACKAGER_JSON` (`1`), see [Packagers](#packagers) |

For example, to send requests as JSON instead of msgpack:

```c
int packager = YAR_PACKAGER_JSON;
yar_client_set_opt(client, YAR_OPT_PACKAGER, &packager);
```

### yar_client_get_opt

```c
const void *yar_client_get_opt(yar_client *client, yar_client_opt opt);
```

Get the current value of a client option.

Returns a pointer to the value on success, `NULL` on failure.

### yar_client_destroy

```c
void yar_client_destroy(yar_client *client);
```

Destroy a client instance and free resources.

## Parameters and Return Values

Yar represents parameters and return values as a format-independent value tree (`yar_data`). The wire format is a pluggable codec — msgpack and JSON encode/decode the same tree, so handlers never touch the wire format directly.

### Response Helpers (Server Side)

| Function | Description |
|---|---|
| `yar_response_set_retval(response, packager)` | Set the return value to a packager built with the [packing API](#packing-building-a-response) |
| `yar_response_set_error(response, code, fmt, ...)` | Fail the call with an error message; `fmt` is printf-style, so format specifiers work like `printf()` |

### Reading the Response (Client Side)

| Function | Description |
|---|---|
| `yar_response_get_status(response)` | `0` on success, non-zero on error |
| `yar_response_get_response(response)` | The return value as a `yar_data *` tree, to unpack (status 0 only) |
| `yar_response_get_error(response, &msg, &len)` | On error, returns 1 and fills `*msg` / `*len`; returns 0 if there was no error |
| `yar_response_free(response)` | Release the response contents |

`example/client.c` shows the full pattern: check status, unpack on success, read the error message otherwise, then free:

```c
yar_response *response = client->call(client, "default", 0, NULL);
/* ... inspect response ... */
yar_response_free(response);
free(response);
```

### Unpacking: Reading Incoming Parameters

When a request arrives, the registered handler is called with `yar_request` and `yar_response`. The caller's parameters are stored in `request->in` as a `yar_data` pointer; the Yar protocol packs all parameters inside an array, so `request->in` is always an array. A convenience accessor:

```c
const yar_data *yar_request_get_parameters(yar_request *request);
```

#### Inspecting data types

```c
yar_data_type yar_unpack_data_type(const yar_data *data, uint *size);
```

Returns the type of a node. For `YAR_DATA_STRING`, `YAR_DATA_MAP`, and `YAR_DATA_ARRAY`, `*size` receives the length (string) or element count (map/array) — e.g. `{'k' => 'v'}` gives `size = 1`.

| Constant | Meaning |
|---|---|
| `YAR_DATA_NULL` | null |
| `YAR_DATA_BOOL` | boolean |
| `YAR_DATA_LONG` | signed integer |
| `YAR_DATA_ULONG` | unsigned integer |
| `YAR_DATA_DOUBLE` | double |
| `YAR_DATA_STRING` | string |
| `YAR_DATA_MAP` | map (key-value pairs) |
| `YAR_DATA_ARRAY` | array |

#### Extracting values

Typed convenience wrappers — each returns the data type and writes the value:

| Function | Writes |
|---|---|
| `yar_unpack_data_null(data, &v)` | – |
| `yar_unpack_data_bool(data, &v)` | `int` |
| `yar_unpack_data_long(data, &v)` | `long` |
| `yar_unpack_data_ulong(data, &v)` | `unsigned long` |
| `yar_unpack_data_string(data, &v)` | `const char *` (get the length from `yar_unpack_data_type`) |
| `yar_unpack_data_array(data, &v)` | `const yar_data *` (elements) |
| `yar_unpack_data_map(data, &v)` | `const yar_data *` (key-value pairs) |

There is also a generic extractor, `int yar_unpack_data_value(const yar_data *data, void *arg);` — it returns the type constant and writes to `*arg`, where the pointer type depends on the data type (`int` for bool, `long`, `unsigned long`, `double`, `const char *` for string, `const yar_data *` for map/array, `NULL` for null).

**Example — parameter validation**

Suppose we expect exactly 3 parameters:

```c
uint size = 0;

if (yar_unpack_data_type(request->in, &size) != YAR_DATA_ARRAY || size != 3) {
    yar_response_set_error(response, YAR_ERROR, "invalid parameters: expected 3");
    return;
}
```

**Example — iterating parameters**

Suppose we expect 2 integer parameters:

```c
uint arg[2], dummy;
yar_data *tmp;

const yar_data *parameters = yar_request_get_parameters(request);
yar_unpack_iterator *it = yar_unpack_iterator_init(parameters);

int index = 0;
do {
    tmp = yar_unpack_iterator_current(it);

    if (yar_unpack_data_type(tmp, &dummy) != YAR_DATA_LONG) {
        yar_response_set_error(response, YAR_ERROR, "invalid parameter: integer expected");
        yar_unpack_iterator_free(it);
        return;
    }

    long val;
    yar_unpack_data_long(tmp, &val);
    arg[index++] = val;
} while (yar_unpack_iterator_next(it));

yar_unpack_iterator_free(it);
```

#### Iterator API

| Function | Description |
|---|---|
| `yar_unpack_iterator_init(data)` | Create an iterator over an array or map |
| `yar_unpack_iterator_next(it)` | Advance to the next element; non-zero while elements remain |
| `yar_unpack_iterator_current(it)` | The current element as `const yar_data *` |
| `yar_unpack_iterator_reset(it)` | Move back to the first element without re-allocating |
| `yar_unpack_iterator_free(it)` | Free the iterator |

### Packing: Building a Response

Use the packing API to construct return values (or client arguments — it is the same API). A packager is opened with a declared size, then filled strictly in order; a nested `push_array(n)` / `push_map(n)` opens a container that consumes the next `n` pushes before control returns to the parent.

| Function | Description |
|---|---|
| `yar_pack_start_map(n)` | Start a map expecting `n` key-value pairs |
| `yar_pack_start_array(n)` | Start an array expecting `n` elements |
| `yar_pack_push_null(p)` | Push NULL |
| `yar_pack_push_bool(p, val)` | Push a boolean |
| `yar_pack_push_long(p, num)` | Push a signed integer |
| `yar_pack_push_ulong(p, num)` | Push an unsigned integer |
| `yar_pack_push_double(p, num)` | Push a double |
| `yar_pack_push_string(p, str, len)` | Push a string (length-explicit, binary-safe) |
| `yar_pack_push_array(p, n)` | Open a nested array; the next `n` pushes fill it |
| `yar_pack_push_map(p, n)` | Open a nested map; the next `n` key-value pairs fill it |
| `yar_pack_push_data(p, data)` | Push an existing `yar_data` tree |
| `yar_pack_push_packager(p, pk)` | Push the tree of another packager |
| `yar_pack_to_string(p, payload)` | Serialise as msgpack into `payload` |
| `yar_pack_free(p)` | Free the packager |

`yar_pack_start_map` / `yar_pack_start_array` are macros over `yar_pack_start(type, size)`; there are also `yar_pack_start_null()` / `_bool()` / `_long()` / `_ulong()` / `_double()` / `_string()` for building scalar roots. All push functions return `1` on success, `0` on failure.

For example, to build:

```
{
    a => [b, c],
    d => e
}
```

The packing sequence is:

```c
yar_packager *pk = yar_pack_start_map(2);  // map with 2 key-value pairs

yar_pack_push_string(pk, "a", 1);   // key "a"
yar_pack_push_array(pk, 2);         // value of "a": a 2-element array
yar_pack_push_string(pk, "b", 1);   //   array element 0
yar_pack_push_string(pk, "c", 1);   //   array element 1 (array now complete)

yar_pack_push_string(pk, "d", 1);   // key "d"
yar_pack_push_string(pk, "e", 1);   // value "e" (map now complete)
```

**Real example** (from `example/server.c`)

The sample handler returns a 3-key map. Note the three techniques it demonstrates:

1. `"status"` → a long (`0`). A plain scalar value.
2. `"parameters"` → the original request parameters, echoed back. This uses `yar_pack_push_data()` to splice an **existing** `yar_data` tree straight into the output — no need to rebuild it field by field.
3. `"data"` → a nested 3-element array `[true, 0.2342, "dummy"]`. Built by opening the array with `yar_pack_push_array(pk, 3)` and pushing the three elements.

```c
const yar_data *parameters = yar_request_get_parameters(request);

yar_packager *pk = yar_pack_start_map(3);   // 3 key-value pairs

yar_pack_push_string(pk, "status", 6);      // key
yar_pack_push_long(pk, 0);                  // value

yar_pack_push_string(pk, "parameters", 10); // key
yar_pack_push_data(pk, parameters);         // value: reuse the incoming tree

yar_pack_push_string(pk, "data", 4);        // key
yar_pack_push_array(pk, 3);                 // value: a 3-element array
yar_pack_push_bool(pk, 1);                  //   element 0
yar_pack_push_double(pk, 0.2342);           //   element 1
yar_pack_push_string(pk, "dummy", 5);       //   element 2

yar_response_set_retval(response, pk);      // attach to the response
yar_pack_free(pk);                          // then free the packager
```

Attach the finished packager to the response with `yar_response_set_retval`, then free it — Yar serialises it when sending the reply.

#### Advanced: value-tree utilities

Lower-level functions for working with trees and wire bytes directly:

| Function | Description |
|---|---|
| `yar_pack_encode(p, payload, type)` | Like `yar_pack_to_string`, but pick the wire format explicitly |
| `yar_pack_take_root(p)` | Detach and return the tree built so far (the packager keeps nothing) |
| `yar_data_unpack(data, len, type)` | Decode wire bytes into an owned tree (`NULL` on failure) |
| `yar_data_pack(data, out, type)` | Encode a tree into wire bytes |
| `yar_data_dup(data)` | Deep copy of a tree |
| `yar_data_destroy(data)` / `yar_data_free(data)` | Release owned contents / contents plus the node itself |
| `yar_unpack_init(data, len, type)` + `yar_unpack_unpack(unpk)` + `yar_unpack_free(unpk)` | Streaming decode of wire bytes into a tree |
| `yar_packager_available(type)` | Whether a wire format is usable in this build (JSON requires cJSON) |

### Debug Print

```c
void yar_debug_print_data(const yar_data *data, FILE *fp);
```

Prints the contents of a `yar_data` to `fp` in a human-readable format. Pass `NULL` for `fp` to print to stdout.

## Packagers

Two wire packagers are supported:

| Constant | Value | Format |
|---|---|---|
| `YAR_PACKAGER_MSGPACK` | `0` | msgpack — binary serialisation, full fidelity, handles arbitrary binary strings. Default |
| `YAR_PACKAGER_JSON` | `1` | JSON — text serialisation, requires the library to be built with cJSON |

The server reads the packager tag from each request and replies with the same packager, so msgpack and JSON clients can be mixed against one server (see `YAR_OPT_PACKAGER` for selecting it in the C client).

JSON limitations:

- Strings with embedded NUL bytes can not be transported — if a handler returns such a string, the JSON response can not be encoded and the server drops the call (it keeps serving other clients).
- Numbers are limited to what an IEEE double can represent exactly; integers beyond 2^53 may lose precision.

## Logging and Utilities

### Logging

| Function | Description |
|---|---|
| `yar_logger_init(path, level)` | Open the log destination. `path` may be `NULL` (log to stderr), a plain file, or a `"\|command"` pipe; `level` is the threshold described under [Log file and level](#log-file-and-level). Returns `1` on success |
| `yar_logger_setopt(YAR_LOGGER_HOSTNAME, str)` | Prefix each log line with a hostname string |
| `yar_logger_destroy()` | Close the destination and free the logger |

Note: `yar_server_run()` initialises the logger itself from `YAR_LOG_FILE` / `YAR_LOG_LEVEL` — these functions are only needed when embedding `libyar` directly.

### yar_set_non_blocking

```c
static inline int yar_set_non_blocking(int fd);
```

Sets a file descriptor to non-blocking mode. Returns `1` on success, `0` on failure. Defined in `yar_common.h`.

## Interoperability

Yar C supports:
- PHP client → C server
- C client → C server

PHP server (HTTP-based) is not callable from a C client — Yar C only speaks the TCP/unix socket protocol.

## License

[Apache-2.0](https://www.apache.org/licenses/LICENSE-2.0)
