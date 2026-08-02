# Yar C Framework

[![Build Status](https://secure.travis-ci.org/laruence/yar-c.png)](https://travis-ci.org/laruence/yar-c)

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

### Server API

If you want to build a C server with Yar, `Yar_Server` is the API to focus on. Yar C (hereafter just "Yar") is built on libevent and msgpack. It provides daemonisation, pre-fork, socket management, logging, and pack/unpack — common building blocks for an RPC server.

#### yar_server_init

```c
int yar_server_init(char *hostname);
```

Initialise `Yar_Server`. The argument is a listen address as a string. For IPv4: `localhost:8888` or `127.0.0.1:8888` — the port is mandatory.

For Unix domain sockets: `/tmp/yar.sock`.

Returns `1` on success, `0` on failure.

Note: no server instance handle is returned — a process can only hold a single server instance.

#### yar_server_set_opt

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

##### YAR_STAND_ALONE

Run as a single process (no daemon, no pre-fork). Useful during development and debugging.

##### YAR_READ_TIMEOUT

Read timeout for requests, in seconds. Default is `5`.

##### YAR_PARENT_INIT & YAR_CUSTOM_DATA

Yar pre-forks several worker processes. This hook is called in the master process after pre-forking and master initialisation, so you can perform master-only setup.

The hook signature is:

```c
typedef void (*yar_init)(void *data);
```

The `data` pointer is whatever was set via `YAR_CUSTOM_DATA`. Use it to pass custom context through the server's lifetime.

##### YAR_CHILD_INIT

Same as above, but called in each worker process after forking. Same signature (`yar_init`). Also supports custom data via `YAR_CUSTOM_DATA`.

##### YAR_CHILD_USER & YAR_CHILD_GROUP

If set, worker processes will call `setuid` / `setgid` to switch to the specified user/group.

##### YAR_MAX_CHILDREN

Number of pre-forked worker processes. Must be between 1 and 128. Typically set to match the number of CPU cores.

##### YAR_LOG_FILE & YAR_LOG_LEVEL

By default Yar runs as a daemon, so log output is not visible on stderr/stdout. Use this option to direct logs to a file or pipe.

Supports plain files and cronolog-style pipes. For example:

```
"|/path/to/cronolog ./yar_server_%M.log"
```

This writes logs to `yar_server_*.log` in the current directory, rotated by minute.

Log levels are `YAR_DEBUG`, `YAR_NOTICE`, `YAR_WARN`, `YAR_ERROR`. A log message is emitted only if its level is greater than or equal to the configured level.

#### yar_server_get_opt

```c
void *yar_server_get_opt(yar_server_opt opt);
```

Get the current value of a server option.

Returns a pointer to the value on success, `NULL` on failure.

#### yar_server_register_handler

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

#### yar_server_run

```c
int yar_server_run();
```

Start the server. This begins the pre-fork, listen, accept, and process loop.

This call does not return unless the server is shut down.

#### yar_server_shutdown

```c
void yar_server_shutdown();
```

Gracefully shut down the server. Stops accepting new requests; each worker exits after finishing its current request.

#### yar_server_destroy

```c
void yar_server_destroy();
```

Destroy the server instance and free resources.

### Client API

If you want to call an existing Yar Server from C, `Yar_Client` is what you need.

#### yar_client_init

```c
yar_client *yar_client_init(char *hostname);
```

Create a `Yar_Client` instance. The argument is the target server address.

Returns a `yar_client` pointer on success:

```c
typedef struct _yar_client {
    int fd;
    char *hostname;
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

Returns `NULL` on failure (e.g. server unreachable).

#### yar_client_destroy

```c
void yar_client_destroy(yar_client *client);
```

Destroy a client instance and free resources.

### Parameters and Return Values

Yar uses msgpack as its serialisation protocol. A set of helper APIs is provided for packing and unpacking data.

#### Response helpers

```c
void yar_response_set_error(yar_response *response, int code, const char *message);
void yar_response_set_retval(yar_response *response, yar_payload *payload);
```

#### Unpacking (Reading Incoming Parameters)

When a request arrives, the registered handler is called with `yar_request` and `yar_response`:

```c
typedef void (*yar_handler)(yar_request *request, yar_response *response, void *data);
```

The caller's parameters are stored in `request->in` as a `yar_data` pointer. Yar protocol packs all parameters inside an array, so `request->in` is always an array.

To get the parameters:

```c
const yar_data *parameters = yar_request_get_parameters(request);
```

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

To read the raw value from a `yar_data`:

```c
void *yar_unpack_data_value(const yar_data *data);
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

    arg[index++] = *(long *)(yar_unpack_data_value(tmp));
} while (yar_unpack_iterator_next(it));

yar_unpack_iterator_free(it);
```

#### Packing (Building a Response)

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
int yar_pack_push_data(yar_packager *packager, yar_data *data);
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
yar_response_set_retval(response, &payload);
yar_pack_free(pk);
```

### Notes

Yar C supports:
- PHP client → C server
- C client → C server

PHP server (HTTP-based) is not yet callable from a C client.

## License

[PHP-3.01](https://www.php.net/license/3_01.txt)
