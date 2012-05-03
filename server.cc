//
// server.cpp
// ~~~~~~~~~~
//
// Copyright 2012 Red Hat, Inc.
// Copyright (c) 2003-2012 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <stdio.h>
#include <boost/thread/thread.hpp>
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <vector>
#include "server.h"

extern std::string opt_ssl_pemfile;
extern bool use_ssl;

namespace http {
namespace server3 {

server::server(const std::string& address, unsigned int port,
    const std::string& doc_root, std::size_t thread_pool_size)
  : thread_pool_size_(thread_pool_size),
    signals_(io_service_),
    acceptor_(io_service_),
    context_(boost::asio::ssl::context::tlsv1),
    new_connection_(),
    request_handler_(doc_root)
{
  // Register to handle the signals that indicate when the server should exit.
  // It is safe to register for the same signal multiple times in a program,
  // provided all registration for the specified signal is made through Asio.
  signals_.add(SIGINT);
  signals_.add(SIGTERM);
#if defined(SIGQUIT)
  signals_.add(SIGQUIT);
#endif // defined(SIGQUIT)
  signals_.async_wait(boost::bind(&server::handle_stop, this));

  if (use_ssl) {
    context_.use_certificate_chain_file(opt_ssl_pemfile);
    context_.use_private_key_file(opt_ssl_pemfile, boost::asio::ssl::context::pem);
  }

  // Open the acceptor with the option to reuse the address (i.e. SO_REUSEADDR).
  char portstr[32];
  snprintf(portstr, sizeof(portstr), "%u", port);
  boost::asio::ip::tcp::resolver resolver(io_service_);
  boost::asio::ip::tcp::resolver::query query(address, portstr);
  boost::asio::ip::tcp::endpoint endpoint = *resolver.resolve(query);
  acceptor_.open(endpoint.protocol());
  acceptor_.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
  acceptor_.bind(endpoint);
  acceptor_.listen();

  start_accept();
}

void server::run()
{
  // Create a pool of threads to run all of the io_services.
  std::vector<boost::shared_ptr<boost::thread> > threads;
  for (std::size_t i = 0; i < thread_pool_size_; ++i)
  {
    boost::shared_ptr<boost::thread> thread(new boost::thread(
          boost::bind(&boost::asio::io_service::run, &io_service_)));
    threads.push_back(thread);
  }

  // Wait for all threads in the pool to exit.
  for (std::size_t i = 0; i < threads.size(); ++i)
    threads[i]->join();
}

void server::start_accept()
{
	if (use_ssl) {
		new_ssl_conn_.reset(new ssl_connection(io_service_, context_, request_handler_));
		acceptor_.async_accept(new_ssl_conn_->socket(), new_ssl_conn_->peer,
		    boost::bind(&server::handle_accept, this,
		      boost::asio::placeholders::error));
	} else {
		new_connection_.reset(new connection(io_service_, request_handler_));
		acceptor_.async_accept(new_connection_->socket(), new_connection_->peer,
		    boost::bind(&server::handle_accept, this,
		      boost::asio::placeholders::error));
	}
}

void server::handle_accept(const boost::system::error_code& e)
{
	if (!e)
	{
		if (use_ssl)
			new_ssl_conn_->start();
		else
			new_connection_->start();
	}

	start_accept();
}

void server::handle_stop()
{
	io_service_.stop();
}

} // namespace server3
} // namespace http
