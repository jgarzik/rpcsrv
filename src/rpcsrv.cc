
/* Copyright 2019 Bloq Inc.
 * Distributed under the MIT/X11 software license, see the accompanying
 * file COPYING or http://www.opensource.org/licenses/mit-license.php.
 */

#include "rpcsrv-config.h"

#include <string>
#include <argp.h>
#include <iostream>
#include <unistd.h>
#include <syslog.h>
#include <event2/event.h>
#include <event2/buffer.h>
#include <event2/http.h>
#include <univalue.h>
#include "util.h"

using namespace std;

#define DEFAULT_LISTEN_ADDR "0.0.0.0"
#define DEFAULT_LISTEN_PORT 12014

static const size_t MAX_HTTP_BODY = 16 * 1000 * 1000;
static const char *opt_pid_file = "/var/run/rpcsrv.pid";
static bool opt_daemon = false;
static struct event_base *eb = NULL;
static string listenAddr = DEFAULT_LISTEN_ADDR;
static unsigned short listenPort = DEFAULT_LISTEN_PORT;

/* Command line arguments and processing */
const char *argp_program_version =
	"rpcsrv " VERSION "\n"
	"Copyright 2019 Bloq, Inc.\n"
	"This is free software; see the source for copying conditions.  There is NO "
	"warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.";

const char *argp_program_bug_address = PACKAGE_BUGREPORT;

static char doc[] =
	"RPC daemon\n";

static struct argp_option options[] = {
	{ "daemon", 1002, NULL, 0,
	  "Daemonize; run server in background." },

	{ "listen-addr", 1003, "ADDRESS", 0,
	  "Listen address (default: " DEFAULT_LISTEN_ADDR ")" },

	{ "listen-port", 1004, "PORT", 0,
	  "Listen port (default: 12014)" },

	{ "pid-file", 'p', "file", 0,
	  "File used for recording daemon PID, and multiple exclusion (default: /var/run/rpcsrv.pid)" },

	{ 0 },
};

/*
 * command line processing
 */
static error_t parse_opt (int key, char *arg, struct argp_state *state)
{
	switch(key) {
	case 'p':
		opt_pid_file = arg;
		break;

	case 1002:
		opt_daemon = true;
		break;

	case 1003:
		listenAddr.assign(arg);
		break;

	case 1004:
		listenPort = atoi(arg);
		if (listenPort == 0)
			argp_usage(state);
		break;

	default:
		return ARGP_ERR_UNKNOWN;
	}

	return 0;
}

static struct argp argp = { options, parse_opt, NULL, doc };

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

	evbuffer_add(OutBuf, body.c_str(), body.size());

	struct evkeyvalq * kv = evhttp_request_get_output_headers(req);
	evhttp_add_header(kv, "Content-Type", "application/json");
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

void rpc_unknown(evhttp_request *req, void *)
{
	auto *OutBuf = evhttp_request_get_output_buffer(req);
	if (!OutBuf)
		return;
	evbuffer_add_printf(OutBuf, "<html><body><center><h1>404 not found</h1></center></body></html>");
	evhttp_send_error(req, 404, "not found");
};

static void pid_file_cleanup(void)
{
	if (opt_pid_file && *opt_pid_file)
		unlink(opt_pid_file);
}

static void shutdown_signal(int signo)
{
	event_base_loopbreak(eb);
}

int main(int argc, char *argv[])
{
	// Parse command line
	if (argp_parse(&argp, argc, argv, 0, NULL, NULL))
		return EXIT_FAILURE;

	// Init libevent
	eb = event_base_new();
	if (!eb) {
		std::cerr << "Failed to init libevent." << std::endl;
		return EXIT_FAILURE;
	}

	// Init HTTP server
	std::unique_ptr<evhttp, decltype(&evhttp_free)> Server(evhttp_new(eb), &evhttp_free);
	if ((!Server) ||
	    (evhttp_bind_socket(Server.get(), listenAddr.c_str(), listenPort) < 0)) {
		std::cerr << "Failed to create & bind http server." << std::endl;
		return EXIT_FAILURE;
	}

	// HTTP server URI callbacks
	evhttp_set_cb(Server.get(), "/", rpc_home, nullptr);
	evhttp_set_cb(Server.get(), "/exec", rpc_exec, nullptr);
	evhttp_set_gencb(Server.get(), rpc_unknown, nullptr);

	openlog("rpcsrv", LOG_PID, LOG_DAEMON);

	// Process auto-cleanup
	signal(SIGTERM, shutdown_signal);
	signal(SIGINT, shutdown_signal);
	atexit(pid_file_cleanup);

	// Daemonize
	if (opt_daemon && daemon(0, 0) < 0) {
		syslog(LOG_ERR, "Failed to daemonize: %s", strerror(errno));
		return EXIT_FAILURE;
	}

	// Hold open PID file until process exits
	int pid_fd = write_pid_file(opt_pid_file);
	if (pid_fd < 0)
		return EXIT_FAILURE;

	// The Main Event -- execute event loop
	syslog(LOG_INFO, "starting");
	if (event_base_dispatch(eb) == -1) {
		syslog(LOG_ERR, "Failed to run message loop.");
		return EXIT_FAILURE;
	}

	syslog(LOG_INFO, "shutting down");
	return EXIT_SUCCESS;
}

