
#include <string>
#include <map>
#include <univalue.h>
#include "myapi_1.h"
#include "server.h"

using namespace std;

class myapi_call_info {
public:
	string		method;
	bool		params_array;
	bool		params_object;
};

map<string,myapi_call_info> myapi_1_list = {
	{ "ping", { "ping", } },
	{ "echo", { "echo", } },
};

// =======================================================================
// RPC implementation section
// =======================================================================
// 

//
// RPC "ping"
//
static UniValue myapi_1_ping(const UniValue& jreq, const UniValue& params)
{
	UniValue result("pong");

	return jrpcOk(jreq, result);
}

//
// RPC "echo"
//
static UniValue myapi_1_echo(const UniValue& jreq, const UniValue& params)
{
	return jrpcOk(jreq, params);
}

// =======================================================================
// RPC service dispatch and metadata/reflection
// =======================================================================
// 

//
// reflection: describe all RPC methods supported by myapi v1
//
UniValue MyApiInfo::list_methods()
{
	// return an array of JSON-RPC calls at this server
	UniValue rv(UniValue::VARR);

	for (auto it = myapi_1_list.begin(); it != myapi_1_list.end(); it++) {
		const myapi_call_info& mci = (*it).second;

		UniValue onerpc(UniValue::VOBJ);
		onerpc.pushKV("method", mci.method);
		if (mci.params_array)
			onerpc.pushKV("paramsReq", "array");
		else if (mci.params_object)
			onerpc.pushKV("paramsReq", "object");
		rv.push_back(onerpc);
	}

	return rv;
}

//
// dispatch JSON-RPC request to myapi v1 RPCs
//
UniValue MyApiInfo::execute(const UniValue& jreq)
{
	const string& method = jreq["method"].getValStr();
	const UniValue& params = jreq["params"];

	if (!myapi_1_list.count(method))
		return jrpcErr(jreq, -32601, "method not found");

	const myapi_call_info& mci = myapi_1_list[method];
	if ((mci.params_array && !params.isArray()) ||
	    (mci.params_object && !params.isObject()))
		return jrpcErr(jreq, -32602, "Invalid params");

	if (method == "ping")
		return myapi_1_ping(jreq, params);

	else if (method == "echo")
		return myapi_1_echo(jreq, params);

	else {
		assert(0 && "invalid source code rpc list");
		return NullUniValue;	// not reached
	}
}

