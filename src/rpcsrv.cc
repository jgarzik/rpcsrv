
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

