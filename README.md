
A C++ multi-threaded JSON-RPC server skeleton, in straight C++11.

Currently implements two simple JSON-RPC methods, "ping" (always returns
true) and "echo" (echoes params back as the result), but sufficient code
exists to support a wide range of RPC methods, inputs and outputs.

Building follows the familiar pattern:

	./autogen.sh	# only if building from git repo
	./configure
	make
	make install

Run "./rpcsrvd --help" for a summary of server configuration options,
and their default values.

