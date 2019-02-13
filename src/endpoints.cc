
#include "rpcsrv-config.h"
#include <string>
#include <cstdio>
#include <cassert>
#include <time.h>
#include <univalue.h>
#include <event2/http.h>
#include <event2/http_struct.h>
#include <event2/buffer.h>
#include "server.h"

using namespace std;

static const size_t MAX_HTTP_BODY = 16 * 1000 * 1000;

// generate a jsonrpc-2.0 standard error response
UniValue jrpcErr(const UniValue& rpcreq, int code, string msg)
{
	UniValue errobj(UniValue::VOBJ);
	errobj.pushKV("code", code);
	errobj.pushKV("message", msg);

	UniValue rpcresp(UniValue::VOBJ);
	rpcresp.pushKV("jsonrpc", "2.0");
	rpcresp.pushKV("error", errobj);
	rpcresp.pushKV("id", rpcreq["id"]);

	return rpcresp;
}

// generate a jsonrpc-2.0 standard successful result
UniValue jrpcOk(const UniValue& rpcreq, const UniValue& result)
{
	UniValue rpcresp(UniValue::VOBJ);
	rpcresp.pushKV("jsonrpc", "2.0");
	rpcresp.pushKV("result", result);
	rpcresp.pushKV("id", rpcreq["id"]);

	return rpcresp;
}

static bool read_http_input(evhttp_request *req, string& body)
{
	// absorb HTTP body input
	struct evbuffer *buf = evhttp_request_get_input_buffer(req);

	// concat each buffer into body string
	size_t buflen;
	while ((buflen = evbuffer_get_length(buf))) {
		if ((body.size() + buflen) > MAX_HTTP_BODY) {
			evhttp_send_error(req, 400, "input too large");
			return false;
		}

		vector<unsigned char> tmp_buf(buflen);
		int n = evbuffer_remove(buf, &tmp_buf[0], buflen);
		if (n > 0)
			body.append((const char *) &tmp_buf[0], n);
	}

	return true;
}

static void send_json_response(evhttp_request *req, const UniValue& rv)
{
	auto *OutBuf = evhttp_request_get_output_buffer(req);
	if (!OutBuf)
		return;

	std::string body = rv.write(2) + "\n";

	char clen_str[32];
	snprintf(clen_str, sizeof(clen_str), "%zu", body.size());
	evbuffer_add(OutBuf, body.c_str(), body.size());

	struct evkeyvalq * kv = evhttp_request_get_output_headers(req);
	evhttp_add_header(kv, "Content-Type", "application/json");
	evhttp_add_header(kv, "Content-Length", clen_str);
	evhttp_add_header(kv, "Server", PACKAGE_NAME "/" PACKAGE_VERSION);

	evhttp_send_reply(req, HTTP_OK, "", OutBuf);
}

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

//
// reflection: list all RPC methods supported by myapi v1
//
static void myapi_1_list_methods(evhttp_request *req)
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

	send_json_response(req, rv);
}

//
// dispatch JSON-RPC request to myapi v1 RPCs
//
static UniValue myapi_1_execute(const UniValue& jreq)
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

	assert(0 && "should never be reached");
	return NullUniValue;	// never reached
}

//
// HTTP endpoint: GET /
//
// Server self-identify meta-endpoint.  Returns reflection-style
// server information, to enable crawlers and server discovery on
// the network, describing the services exported by this server.
//
void rpc_home(evhttp_request *req, void *)
{
	// return an array of services at this server
	UniValue rv(UniValue::VARR);

	// we serve one (1) API at this server
	UniValue myapi_obj(UniValue::VOBJ);
	myapi_obj.pushKV("name", "myapi/1"); // our service, v1
	myapi_obj.pushKV("timestamp", (int64_t)time(nullptr));
	myapi_obj.pushKV("endpoint", "/rpc/1");

	rv.push_back(myapi_obj);

	send_json_response(req, rv);
};

static UniValue jsonrpc_exec_1(const UniValue& jrpc)
{
	if (!jrpc.isObject() ||
	    !jrpc.exists("method") ||
	    !jrpc["method"].isStr())
		return jrpcErr(jrpc, -32600, "Invalid request object");

	return myapi_1_execute(jrpc);
}

static UniValue jsonrpc_exec_batch(const UniValue& jbatch)
{
	UniValue result(UniValue::VARR);

	size_t batchsz = jbatch.size();
	for (unsigned int i = 0; i < batchsz; i++) {
		const UniValue& jrpc = jbatch[i];

		result.push_back(jsonrpc_exec_1(jrpc));
	}

	return result;
}

//
// HTTP endpoint: GET /rpc/1 (list methods) or POST /rpc/1 (execute RPC)
//
// JSON-RPC API endpoint.  Handles all JSON-RPC method calls.
//
void rpc_jsonrpc(evhttp_request *req, void *)
{
	if (req->type == EVHTTP_REQ_GET) {
		myapi_1_list_methods(req);
		return;
	}
	assert(req->type == EVHTTP_REQ_POST);

	// absorb HTTP body input
	string body;
	if (!read_http_input(req, body))
		return;

	// decode json-rpc msg request
	UniValue jrpc;
	UniValue jresp;
	if (!jrpc.read(body))
		jresp = jrpcErr(jrpc, -32700, "JSON parse error");

	else if (jrpc.isArray())
		jresp = jsonrpc_exec_batch(jrpc);

	else
		jresp = jsonrpc_exec_1(jrpc);

	send_json_response(req, jresp);
};

