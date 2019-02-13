
#include "rpcsrv-config.h"
#include <string>
#include <cstdio>
#include <univalue.h>
#include <event2/http.h>
#include <event2/buffer.h>
#include "server.h"

using namespace std;

static const size_t MAX_HTTP_BODY = 16 * 1000 * 1000;

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

UniValue jrpcOk(const UniValue& rpcreq, const UniValue& result)
{
	UniValue rpcresp(UniValue::VOBJ);
	rpcresp.pushKV("jsonrpc", "2.0");
	rpcresp.pushKV("result", result);
	rpcresp.pushKV("id", rpcreq["id"]);

	return rpcresp;
}

void rpc_home(evhttp_request *req, void *)
{
	auto *OutBuf = evhttp_request_get_output_buffer(req);
	if (!OutBuf)
		return;

	UniValue rv(UniValue::VARR);

	UniValue api_obj(UniValue::VOBJ);
	api_obj.pushKV("name", "rpcsrv/1");		// service ora, ver 1

	rv.push_back(api_obj);

	std::string body = rv.write(2) + "\n";

	char clen_str[32];
	snprintf(clen_str, sizeof(clen_str), "%zu", body.size());
	evbuffer_add(OutBuf, body.c_str(), body.size());

	struct evkeyvalq * kv = evhttp_request_get_output_headers(req);
	evhttp_add_header(kv, "Content-Type", "application/json");
	evhttp_add_header(kv, "Content-Length", clen_str);
	evhttp_add_header(kv, "Server", "rpcsrv/" PACKAGE_VERSION);

	evhttp_send_reply(req, HTTP_OK, "", OutBuf);
};

static bool read_http_input(evhttp_request *req, string& body)
{
	// absorb HTTP body input
	struct evbuffer *buf = evhttp_request_get_input_buffer(req);

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

void rpc_exec(evhttp_request *req, void *)
{
	// absorb HTTP body input
	string body;
	if (!read_http_input(req, body))
		return;

	// decode json-rpc msg request
	UniValue jrpc;
	UniValue jresp;
	if (!jrpc.read(body)) {
		jresp = jrpcErr(jrpc, -32700, "JSON parse error");
	}

	else if (!jrpc.isObject() ||
	    !jrpc.exists("method") ||
	    !jrpc["method"].isStr()) {
		jresp = jrpcErr(jrpc, -32600, "Invalid request object");
	}

	else {
		jresp = jrpcOk(jrpc, jrpc["params"]);
	}

	body = jresp.write(2) + "\n";

	char clen_str[32];
	snprintf(clen_str, sizeof(clen_str), "%zu", body.size());

	// HTTP headers
	struct evkeyvalq * kv = evhttp_request_get_output_headers(req);
	evhttp_add_header(kv, "Content-Type", "application/json");
	evhttp_add_header(kv, "Content-Length", clen_str);
	evhttp_add_header(kv, "Server", "rpcsrv/" PACKAGE_VERSION);

	// HTTP body
	std::unique_ptr<evbuffer, decltype(&evbuffer_free)> OutBuf(evbuffer_new(), &evbuffer_free);
	evbuffer_add(OutBuf.get(), body.c_str(), body.size());

	// finalize, send everything
	evhttp_send_reply(req, HTTP_OK, "", OutBuf.get());
};

