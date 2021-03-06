
/* Copyright 2019 Bloq Inc.
 * Distributed under the MIT/X11 software license, see the accompanying
 * file COPYING or http://www.opensource.org/licenses/mit-license.php.
 */

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

vector<RpcApiInfo*> apiList;

UniValue RpcApiInfo::list_method_helper(const map<string,RpcCallInfo>& callList)
{
	// return an array of JSON-RPC calls at this server
	UniValue rv(UniValue::VARR);

	for (auto it = callList.begin(); it != callList.end(); it++) {
		const RpcCallInfo& mci = (*it).second;

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

UniValue RpcApiInfo::rpc_call_check(const UniValue& jreq,
		    const std::string& method,
		    const UniValue& params)
{
	const RpcCallInfo& mci = call_info(method);
	if (mci.method.empty())
		return jrpcErr(jreq, -32601, "method not found");

	if ((mci.params_array && !params.isArray()) ||
	    (mci.params_object && !params.isObject()))
		return jrpcErr(jreq, -32602, "Invalid params");

	return NullUniValue;
}

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
static void rpc_home(evhttp_request *req, void *)
{
	// return an array of services at this server
	UniValue rv(UniValue::VARR);

	for (auto it = apiList.begin(); it != apiList.end(); it++) {
		RpcApiInfo *ap = *it;

		string serviceVersion = ap->name + "/" + ap->version;

		UniValue api_obj(UniValue::VOBJ);
		api_obj.pushKV("name", serviceVersion);
		api_obj.pushKV("endpoint", ap->uripath);

		rv.push_back(api_obj);
	}

	send_json_response(req, rv);
};

static UniValue jsonrpc_exec_1(RpcApiInfo *ap, const UniValue& jrpc)
{
	if (!jrpc.isObject() ||
	    !jrpc.exists("method") ||
	    !jrpc["method"].isStr())
		return jrpcErr(jrpc, -32600, "Invalid request object");

	return ap->execute(jrpc);
}

static UniValue jsonrpc_exec_batch(RpcApiInfo *ap, const UniValue& jbatch)
{
	UniValue result(UniValue::VARR);

	size_t batchsz = jbatch.size();
	for (unsigned int i = 0; i < batchsz; i++) {
		const UniValue& jrpc = jbatch[i];

		result.push_back(jsonrpc_exec_1(ap, jrpc));
	}

	return result;
}

//
// HTTP endpoint: GET /rpc/1 (list methods) or POST /rpc/1 (execute RPC)
//
// JSON-RPC API endpoint.  Handles all JSON-RPC method calls.
//
static void rpc_jsonrpc(evhttp_request *req, void *opaque)
{
	RpcApiInfo *ap = (RpcApiInfo *) opaque;

	if (req->type == EVHTTP_REQ_GET) {
		UniValue rv = ap->list_methods();
		send_json_response(req, rv);
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
		jresp = jsonrpc_exec_batch(ap, jrpc);

	else
		jresp = jsonrpc_exec_1(ap, jrpc);

	send_json_response(req, jresp);
};

bool register_endpoints(struct evhttp* http)
{
	RpcApiInfo *ap = new MyApiInfo();
	assert(ap != nullptr);

	apiList.push_back(ap);

	evhttp_set_cb(http, "/", rpc_home, nullptr);

	for (auto it = apiList.begin(); it != apiList.end(); it++) {
		RpcApiInfo *ap = *it;
		evhttp_set_cb(http, ap->uripath.c_str(), rpc_jsonrpc, ap);
	}
	return true;
}

