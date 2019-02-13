
/* Copyright 2019 Bloq Inc.
 * Distributed under the MIT/X11 software license, see the accompanying
 * file COPYING or http://www.opensource.org/licenses/mit-license.php.
 */

#include "rpcsrv-config.h"

#include <string>
#include <argp.h>
#include <memory>
#include <iostream>
#include <cstdio>
#include <unistd.h>
#include <syslog.h>
#include <signal.h>
#include <event2/event.h>
#include <event2/buffer.h>
#include <event2/http.h>
#include <univalue.h>
#include "util.h"
#include "server.h"

using namespace std;

#define DEFAULT_LISTEN_ADDR "0.0.0.0"
#define DEFAULT_LISTEN_PORT 12014

static const char *opt_pid_file = "/var/run/rpcsrv.pid";
static bool opt_daemon = false;
static struct event_base *eb = NULL;
static string listenAddr = DEFAULT_LISTEN_ADDR;
static unsigned short listenPort = DEFAULT_LISTEN_PORT;
static UniValue progCfg;

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
	{ "config", 'c', "JSON-FILE", 0,
	  "Read JSON configuration from FILE." },

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
	case 'c': {
		string body;
		FILE *f = fopen(arg, "r");
		if (f) {
			char line[1024];
			char *l;
			while ((l = fgets(line, sizeof(line), f)) != nullptr) {
				body.append(l);
			} 
			fclose(f);
		}
		break;

		if (!progCfg.read(body)) {
			fprintf(stderr, "Invalid JSON configuration file %s\n",
				arg);
			return ARGP_ERR_UNKNOWN;
		}
	}

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
	evhttp_set_allowed_methods(Server.get(),
		EVHTTP_REQ_GET | EVHTTP_REQ_POST);
	evhttp_set_cb(Server.get(), "/", rpc_home, nullptr);
	evhttp_set_cb(Server.get(), "/rpc/1", rpc_jsonrpc, nullptr);
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

