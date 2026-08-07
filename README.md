# Yar C Framework

[![CI](https://github.com/laruence/yar-c/actions/workflows/ci.yml/badge.svg)](https://github.com/laruence/yar-c/actions/workflows/ci.yml)

A high-performance RPC server/client library written in C.

See also: [Yar PHP framework](https://github.com/laruence/yar), [Yar Java framework](https://github.com/weibocom/yar-java), [Lua Yar framework](https://github.com/fangfengxiang/lua-yar)

## Requirements

- libevent
- [msgpack-c](https://github.com/msgpack/msgpack-c)

## Install

```bash
$ ./configure --with-msgpack=/path/to/msgpack --with-event=/path/to/libevent
$ make
```

## Example

An example server and client can be found in the `example/` directory.

## Manual

### Overview

Once Yar C is installed, check the `example/` directory for a simple demonstration of building an RPC service with Yar.

## Server API

If you want to build a C server with Yar, `Yar_Server` is the API to focus on. Yar C (hereafter just "Yar") is built on libevent and msgpack. It provides daemonisation, pre-fork, socket management, logging, and pack/unpack — common building blocks for an RPC server.

### yar_server_init

```c
int yar_server_init(char *hostname);
```

Initialise `Yar_Server`. The argument is a listen address as a string. For IPv4: `localhost:8888` or `127.0.0.1:8888` — the port is mandatory.

For Unix domain sockets: `/tmp/yar.sock`.

Returns `1` on success, `0` on failure.

Note: no server instance handle is returned — a process can only hold a single server instance.

### yar_server_print_usage

```c
void yar_server_print_usage(char *argv0);
```

Prints command-line usage information to stderr.

### yar_server_set_opt

```c
int yar_server_set_opt(yar_server_opt opt, void *val);
```

Set a server option. Available options:

```c
typedef enum _yar_server_opt {
    YAR_STAND_ALONE,    // Single-process mode (useful for debugging — no daemon, no pre-fork)
    YAR_READ_TIMEOUT,   // Request read timeout
    YAR_PARENT_INIT,    // Parent (master) process init hook
    YAR_CHILD_INIT,     // Worker process init hook
    YAR_CHILD_USER,     // Worker process username (setuid)
    YAR_CHILD_GROUP,    // Worker process group (setgid)
    YAR_CUSTOM_DATA,    // Custom data pointer passed to hook callbacks
    YAR_MAX_CHILDREN,   // Number of pre-forked workers (1–128, typically set to CPU core count)
    YAR_PID_FILE,       // PID file path (empty by default)
    YAR_LOG_LEVEL,      // Log level (default: ALL)
    YAR_LOG_FILE        // Log file (plain file or cronolog pipe)
} yar_server_opt;
```

For each option, `val` should be a pointer to the value. For example, setting the read timeout:

```c
#include "yar.h"

int timeout = 5;
yar_server_set_opt(YAR_READ_TIMEOUT, &timeout);
```

Returns `1` on success, `0` on failure.

#### YAR_STAND_ALONE

Run as a single process (no daemon, no pre-fork). Useful during development and debugging.

#### YAR_READ_TIMEOUT

Read timeout for requests, in seconds. Default is `3`.

#### YAR_PARENT_INIT & YAR_CUSTOM_DATA

Yar pre-forks several worker processes. This hook is called in the master process after pre-forking and master initialisation, so you can perform master-only setup.

The hook signature is:

```c
typedef void (*yar_init)(void *data);
```

The `data` pointer is whatever was set via `YAR_CUSTOM_DATA`. Use it to pass custom context through the server's lifetime.

#### YAR_CHILD_INIT

Same as above, but called in each worker process after forking. Same signature (`yar_init`). Also supports custom data via `YAR_CUSTOM_DATA`.

#### YAR_CHILD_USER & YAR_CHILD_GROUP

If set, worker processes will call `setuid` / `setgid` to switch to the specified user/group.

#### YAR_MAX_CHILDREN

Number of pre-forked worker processes. Must be between 1 and 128. Typically set to match the number of CPU cores.

#### YAR_LOG_FILE & YAR_LOG_LEVEL

By default Yar runs as a daemon, so log output is not visible on stderr/stdout. Use this option to direct logs to a file or pipe.

Supports plain files and cronolog-style pipes. For example:

```
"|/path/to/cronolog ./yar_server_%M.log"
```

This writes logs to `yar_server_*.log` in the current directory, rotated by minute.

Log levels are `YAR_DEBUG`, `YAR_NOTICE`, `YAR_WARNING`, `YAR_ERROR`. A log message is emitted only if its level is greater than or equal to the configured level.

#### yar_server_get_opt

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

`name` is the RPC method name the client will call. Example:

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
void yar_server_shutdown();
```

Gracefully shut down the server. Stops accepting new requests; each worker exits after finishing its current request.

### yar_server_destroy

```c
void yar_server_destroy();
```

Destroy the server instance and free resources.

## Client API

If you want to call an existing Yar Server from C, `Yar_Client` is what you need.

> **Note**: Yar C client only supports **TCP** and **Unix domain sockets**. HTTP/HTTPS targets are not supported — `yar_client_init()` returns `NULL` for URLs starting with `http://` or `https://`.

### yar_client_init

```c
yar_client *yar_client_init(char *hostname);
```

Create a `Yar_Client` instance. The argument is the target server address (e.g. `"tcp://localhost:2222"` or `"/tmp/yar.sock"`).

Returns a `yar_client` pointer on success, `NULL` on failure:

```c
typedef struct _yar_client {
    int fd;
    char *hostname;
    int persistent;
    int timeout;
    yar_client_call call;
} yar_client;
```

In practice you only need to know the `call` function pointer:

```c
typedef yar_response *(*yar_client_call)(yar_client *client, char *method, uint num_args, yar_packager *packager[]);
```

Once you have a client, call a remote method like this:

```c
yar_client *client = yar_client_init("tcp://localhost:2222");
yar_response *response = client->call(client, "default", 2, args);
```

### yar_client_set_opt

```c
int yar_client_set_opt(yar_client *client, yar_client_opt opt, void *val);
```

Set a client option. Available options:

```c
typedef enum _yar_client_opt {
    YAR_PERSISTENT_LINK = 1,  // Keep connection alive between calls
    YAR_CONNECT_TIMEOUT       // Connection timeout in seconds
} yar_client_opt;
```

Returns `1` on success, `0` on failure.

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

Yar uses msgpack as its serialisation protocol. A set of helper APIs is provided for packing and unpacking data.

### Response Helpers

```c
void yar_response_set_error(yar_response *response, int code, const char *fmt, ...);
void yar_response_set_retval(yar_response *response, yar_packager *packager);
```

`yar_response_set_error` uses printf-style formatting — you can include format specifiers in the message string just like `printf()`.

### Reading the Response

After a client call returns a `yar_response *`, inspect the result with:

```c
int yar_response_get_status(yar_response *response);
const yar_data *yar_response_get_response(yar_response *response);
int yar_response_get_error(yar_response *response, const char **msg, uint *len);
```

- `yar_response_get_status` — returns the status code (0 on success, non-zero on error).
- `yar_response_get_response` — returns the response data as a `yar_data *` to unpack.
- `yar_response_get_error` — if an error occurred, returns 1 and fills `*msg` and `*len` with the error string. Returns 0 if there was no error.

### Unpacking (Reading Incoming Parameters)

When a request arrives, the registered handler is called with `yar_request` and `yar_response`:

```c
typedef void (*yar_handler)(yar_request *request, yar_response *response, void *data);
```

The caller's parameters are stored in `request->in` as a `yar_data` pointer. Yar protocol packs all parameters inside an array, so `request->in` is always an array.

To get the parameters:

```c
const yar_data *parameters = yar_request_get_parameters(request);
```

#### Inspecting Data Types

To inspect the type and size of a `yar_data`:

```c
yar_data_type yar_unpack_data_type(const yar_data *data, uint *size);
```

`yar_data_type` values:

```c
typedef enum _yar_data_type {
    YAR_DATA_NULL   = 1,
    YAR_DATA_BOOL,
    YAR_DATA_LONG,
    YAR_DATA_ULONG,
    YAR_DATA_DOUBLE,
    YAR_DATA_STRING,
    YAR_DATA_MAP,
    YAR_DATA_ARRAY
} yar_data_type;
```

For `YAR_DATA_STRING`, `YAR_DATA_MAP`, and `YAR_DATA_ARRAY`, the `size` output parameter receives the length (string) or element count (map/array). For example, `{'k' => 'v'}` returns `size = 1`.

#### Extracting Values

The generic value extractor:

```c
int yar_unpack_data_value(const yar_data *data, void *arg);
```

Returns the data type constant. The value itself is written to `*arg` — the exact pointer type depends on the data type:

| Data Type | Writes to `*arg` |
|---|---|
| `YAR_DATA_NULL` | `NULL` |
| `YAR_DATA_BOOL` | `int` |
| `YAR_DATA_LONG` | `long` |
| `YAR_DATA_ULONG` | `unsigned long` |
| `YAR_DATA_DOUBLE` | `double` |
| `YAR_DATA_STRING` | `const char *` |
| `YAR_DATA_ARRAY` | `const yar_data *` (elements) |
| `YAR_DATA_MAP` | `const yar_data *` (key-value pairs) |

Typed convenience wrappers (each returns the data type and writes the value):

```c
int yar_unpack_data_null(const yar_data *data, int *val);
int yar_unpack_data_bool(const yar_data *data, int *bval);
int yar_unpack_data_long(const yar_data *data, long *num);
int yar_unpack_data_ulong(const yar_data *data, unsigned long *num);
int yar_unpack_data_string(const yar_data *data, const char **str);
int yar_unpack_data_array(const yar_data *data, const yar_data **arg);
int yar_unpack_data_map(const yar_data *data, const yar_data **arg);
```

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

```c
yar_unpack_iterator *yar_unpack_iterator_init(const yar_data *data);
int yar_unpack_iterator_next(yar_unpack_iterator *it);
const yar_data *yar_unpack_iterator_current(yar_unpack_iterator *it);
void yar_unpack_iterator_reset(yar_unpack_iterator *it);
void yar_unpack_iterator_free(yar_unpack_iterator *it);
```

`yar_unpack_iterator_reset` moves the iterator back to the first element without re-allocating — useful for iterating the same data multiple times.

### Packing (Building a Response)

After processing the request, use the packing API to construct the return value:

```c
yar_packager *yar_pack_start_map(uint size);

int yar_pack_push_array(yar_packager *packager, uint size);
int yar_pack_push_map(yar_packager *packager, uint size);
int yar_pack_push_null(yar_packager *packager);
int yar_pack_push_bool(yar_packager *packager, int val);
int yar_pack_push_long(yar_packager *packager, long num);
int yar_pack_push_ulong(yar_packager *packager, unsigned long num);
int yar_pack_push_double(yar_packager *packager, double num);
int yar_pack_push_string(yar_packager *packager, char *str, uint len);
int yar_pack_push_data(yar_packager *packager, const yar_data *data);
int yar_pack_push_packager(yar_packager *packager, yar_packager *data);
int yar_pack_to_string(yar_packager *packager, yar_payload *payload);

void yar_pack_free(yar_packager *packager);
```

Packing follows a one-dimensional sequential order. For example, to build:

```
{
    a => [b, c],
    d => e
}
```

The packing sequence is:

```c
yar_packager *pk = yar_pack_start_map(2);  // map with 2 key-value pairs

yar_pack_push_string(pk, "a", 1);   // push key "a"
yar_pack_push_array(pk, 2);         // push a 2-element array as the value of "a"
yar_pack_push_string(pk, "b", 1);   // first array element
yar_pack_push_string(pk, "c", 1);   // second array element (array is now complete)

yar_pack_push_string(pk, "d", 1);   // push key "d"
yar_pack_push_string(pk, "e", 1);   // push value "e"
```

**Real example** (see `example/server.c`):

The sample server returns a 3-element map to the client:

1. `"status"` → a long (0)
2. `"parameters"` → the original request parameters (echoed back)
3. A nested map with some sample values

After building the payload, call `yar_response_set_retval` to attach it to the response, then free the packager.

```c
yar_payload payload;
yar_pack_to_string(pk, &payload);
yar_response_set_retval(response, pk);
yar_pack_free(pk);
```

### Debug Print

```c
void yar_debug_print_data(const yar_data *data, FILE *fp);
```

Prints the contents of a `yar_data` to `fp` in a human-readable format. Pass `NULL` for `fp` to print to stdout.

## Utility Functions

### yar_set_non_blocking

```c
static inline int yar_set_non_blocking(int fd);
```

Sets a file descriptor to non-blocking mode. Returns `1` on success, `0` on failure. Defined as a static inline in `yar_common.h`.

## Logging

### yar_logger_init

```c
int yar_logger_init(const char *path, int mask);
```

Initialise the logger with a file path and a log level mask (bitwise OR of `YAR_DEBUG | YAR_NOTICE | YAR_WARNING | YAR_ERROR`).

Returns `1` on success, `0` on failure.

### yar_logger_setopt

```c
int yar_logger_setopt(yar_logger_opt opt, void *value);
```

Set a logger option. Currently supports:

```c
typedef enum _yar_logger_opt {
    YAR_LOGGER_HOSTNAME
} yar_logger_opt;
```

### yar_logger_destroy

```c
void yar_logger_destroy();
```

Destroy the logger and free resources.

## Interoperability

Yar C supports:
- PHP client → C server
- C client → C server

PHP server (HTTP-based) is not callable from a C client — Yar C only speaks the TCP/unix socket protocol.

## License

[Apache-2.0](https://www.apache.org/licenses/LICENSE-2.0)
