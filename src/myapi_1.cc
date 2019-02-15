
/* Copyright 2019 Bloq Inc.
 * Distributed under the MIT/X11 software license, see the accompanying
 * file COPYING or http://www.opensource.org/licenses/mit-license.php.
 */

#include <string>
#include <map>
#include <univalue.h>
#include "myapi_1.h"
#include "server.h"

using namespace std;

map<string,RpcCallInfo> myapi_1_list = {
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
	return list_method_helper(myapi_1_list);
}

const RpcCallInfo& MyApiInfo::call_info(const std::string& method)
{
	return myapi_1_list[method];
}

//
// dispatch JSON-RPC request to myapi v1 RPCs
//
UniValue MyApiInfo::execute(const UniValue& jreq)
{
	const string& method = jreq["method"].getValStr();
	const UniValue& params = jreq["params"];

	UniValue uret = rpc_call_check(jreq, method, params);
	if (!uret.isNull())
		return uret;

	if (method == "ping")
		return myapi_1_ping(jreq, params);

	else if (method == "echo")
		return myapi_1_echo(jreq, params);

	else {
		assert(0 && "invalid source code rpc list");
		return NullUniValue;	// not reached
	}
}

