
# rpcsrv

## Introduction

A C++ multi-threaded JSON-RPC server skeleton, in straight C++11.

Currently implements two simple JSON-RPC methods, "ping" (always returns
true) and "echo" (echoes params back as the result), but sufficient code
exists to support a wide range of RPC methods, inputs and outputs.

## Building

Building follows the familiar pattern:

	git submodule update --init
	./autogen.sh	# only if building from git repo
	./configure
	make
	make install

## Running the server

Run "./rpcsrvd --help" for a summary of server configuration options,
and their default values.  See `example-config.json` for the server
JSON configuration file.

## Examples

### GET / -- Server reflection endpoint

This returns an array of all the JSON-RPC services at this HTTP endpoint.

We only have one service at this endpoint, "myapi":

```
$ curl http://localhost:12014/
[
  {
    "name": "myapi/1",
    "timestamp": 1550068729,
    "endpoint": "/rpc/1"
  }
]
```

### GET /rpc/1 -- JSON-RPC reflection endpoint

This returns an array of all the JSON-RPC methods for this service.

We have two methods at this service, "ping" and "echo":

```
$ curl http://localhost:12014/rpc/1
[
  {
    "method": "ping"
  },
  {
    "method": "echo"
  }
]
```

### POST /rpc/1 -- execute RPC calls with JSON-RPC protocol

We call the "echo" RPC, which echoes any parameters sent to it:

```
$ curl -X POST -d '{"jsonrpc":"2.0","method":"echo","params":[1,2,3,4], "id":1234}' http://localhost:12014/rpc/1
{
  "jsonrpc": "2.0",
  "result": [
    1,
    2,
    3,
    4
  ],
  "id": 1234
}
```

