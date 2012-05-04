//
// connection.hpp
// ~~~~~~~~~~~~~~
//
// Copyright 2012 Red Hat, Inc.
// Copyright (c) 2003-2012 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef HTTP_SERVER3_CONNECTION_HPP
#define HTTP_SERVER3_CONNECTION_HPP

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/array.hpp>
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include "request.h"
#include "reply.h"
#include "request_handler.h"
#include "request_parser.h"

namespace http {
namespace server3 {

class base_connection
{
public:
	/// Construct a connection with the given io_service.
	explicit base_connection(boost::asio::io_service& io_service,
			request_handler& handler) :
			strand_(io_service),
			request_handler_(handler)
	{
	}

	/// Remote peer
	boost::asio::ip::tcp::endpoint peer;

protected:
	/// Log HTTP request
	void log_request();

	/// Strand to ensure the connection's handlers are not called concurrently.
	boost::asio::io_service::strand strand_;

	/// The handler used to process the incoming request.
	request_handler& request_handler_;

	/// Buffer for incoming data.
	boost::array<char, 8192> buffer_;

	/// The incoming request.
	request request_;

	/// The outgoing reply.
	reply reply_;

	/// The parser for the incoming request.
	request_parser request_parser_;

	/// HTTP/1.1 keepalive enabled?
	bool keepalive_;

	void reset()
	{
		request_.clear();
		reply_.clear();
		request_parser_.reset();
	}
};

/// Represents a single connection from a client.
class connection
	: public boost::enable_shared_from_this<connection>,
		public base_connection,
		private boost::noncopyable
{
public:
	/// Construct a connection with the given io_service.
	explicit connection(boost::asio::io_service& io_service,
			request_handler& handler) :
			base_connection(io_service, handler),
			socket_(io_service)
	{
	}

	/// Get the socket associated with the connection.
	boost::asio::ip::tcp::socket& socket();

	/// Start the first asynchronous operation for the connection.
	void start();

private:
	/// Initiate asynchronous read
	void read_more();

	/// Handle completion of a read operation.
	void handle_read(const boost::system::error_code& e,
			std::size_t bytes_transferred);

	/// Handle completion of a write operation.
	void handle_write(const boost::system::error_code& e);

	/// Socket for the connection.
	boost::asio::ip::tcp::socket socket_;
};

typedef boost::shared_ptr<connection> connection_ptr;

typedef boost::asio::ssl::stream<boost::asio::ip::tcp::socket> ssl_socket;

/// Represents a single connection from a client.
class ssl_connection
	: public boost::enable_shared_from_this<ssl_connection>,
		public base_connection,
		private boost::noncopyable
{
public:
	/// Construct a connection with the given io_service.
	explicit ssl_connection(boost::asio::io_service& io_service,
				boost::asio::ssl::context& context,
				request_handler& handler) :
			base_connection(io_service, handler),
			socket_(io_service, context)
	{
	}

	/// Start the first asynchronous operation for the connection.
	void start();

private:
	/// Initiate asynchronous read
	void read_more();

	/// Handle completion of a read operation.
	void handle_read(const boost::system::error_code& e,
			std::size_t bytes_transferred);

	/// Handle completion of a write operation.
	void handle_write(const boost::system::error_code& e);

	/// Handle completion of a handshake operation.
	void handle_handshake(const boost::system::error_code& e);

	/// Socket for the connection.
	ssl_socket socket_;

public:
	ssl_socket::lowest_layer_type& socket()
	{
		return socket_.lowest_layer();
	}

};

typedef boost::shared_ptr<ssl_connection> ssl_conn_ptr;

} // namespace server3
} // namespace http

#endif // HTTP_SERVER3_CONNECTION_HPP
