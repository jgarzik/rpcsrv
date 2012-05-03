//
// main.cpp
// ~~~~~~~~
//
// Copyright 2012 Red Hat, Inc.
// Copyright (c) 2003-2012 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <locale.h>
#include <iostream>
#include <string>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/program_options.hpp>
#include "server.h"

using namespace std;

static std::string opt_bind_addr = "0.0.0.0";
static unsigned int opt_bind_port = 8080;
static unsigned int opt_n_threads = 10;
static std::string opt_doc_root = ".";
std::string opt_ssl_pemfile;
bool use_ssl = false;

namespace po = boost::program_options;

static bool parse_cmdline(int ac, char *av[])
{
	try {
		po::options_description desc("Allowed options");
		desc.add_options()
			("help", "produce help message")

			("address,a", po::value<std::string>(&opt_bind_addr)->
				default_value("0.0.0.0"),
				"TCP bind address")

			("doc-root,d", po::value<std::string>(&opt_doc_root)->
				default_value("."),
				"Document root")

			("port,p", po::value<unsigned int>(&opt_bind_port)->
				default_value(8080),
				"TCP bind port")

			("pem", po::value<std::string>(&opt_ssl_pemfile),
				"SSL private key, etc.")

			("threads,t", po::value<unsigned int>(&opt_n_threads)->
				default_value(10),
				"Thread pool size")

		;

		po::variables_map vm;		
		po::store(po::parse_command_line(ac, av, desc), vm);
		po::notify(vm);	

		if (vm.count("help")) {
			std::cout << desc << "\n";
			exit(0);
		}

	}

	catch (exception& e) {
		std::cerr << "error: " << e.what() << "\n";
		return false;
	}
	catch (...) {
		std::cerr << "Exception of unknown type!\n";
		return false;
	}

	if (opt_ssl_pemfile.length() > 0)
		use_ssl = true;

	return true;
}

int main(int argc, char *argv[])
{
	setlocale(LC_ALL, "");

	if (!parse_cmdline(argc, argv))
		return 1;

	try
	{
		// Initialise the server.
		std::size_t num_threads =
			boost::lexical_cast<std::size_t>(opt_n_threads);

		http::server3::server s(opt_bind_addr, opt_bind_port,
					opt_doc_root, num_threads);

		// Run the server until stopped.
		s.run();
	}
	catch (std::exception& e)
	{
		std::cerr << "exception: " << e.what() << "\n";
	}

	return 0;
}

