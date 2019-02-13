
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
#include "myapi_1.h"

using namespace std;

static const size_t MAX_HTTP_BODY = 16 * 1000 * 1000;
unsigned int opt_json_indent = 0;

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

void send_json_response(evhttp_request *req, const UniValue& rv)
{
	auto *OutBuf = evhttp_request_get_output_buffer(req);
	if (!OutBuf)
		return;

	std::string body = rv.write(opt_json_indent) + "\n";

	char clen_str[32];
	snprintf(clen_str, sizeof(clen_str), "%zu", body.size());
	evbuffer_add(OutBuf, body.c_str(), body.size());

	struct evkeyvalq * kv = evhttp_request_get_output_headers(req);
	evhttp_add_header(kv, "Content-Type", "application/json");
	evhttp_add_header(kv, "Content-Length", clen_str);
	evhttp_add_header(kv, "Server", PACKAGE_NAME "/" PACKAGE_VERSION);

	evhttp_send_reply(req, HTTP_OK, "", OutBuf);
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

