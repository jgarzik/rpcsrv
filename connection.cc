//
// connection.cpp
// ~~~~~~~~~~~~~~
//
// Copyright 2012 Red Hat, Inc.
// Copyright (c) 2003-2012 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <stdio.h>
#include <sstream>
#include <vector>
#include <boost/bind.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include "connection.h"
#include "reply.h"
#include "request_handler.h"

namespace http {
namespace server3 {

connection::connection(boost::asio::io_service& io_service,
    request_handler& handler)
  : strand_(io_service),
    socket_(io_service),
    request_handler_(handler)
{
}

boost::asio::ip::tcp::socket& connection::socket()
{
  return socket_;
}

void connection::start()
{
  socket_.async_read_some(boost::asio::buffer(buffer_),
      strand_.wrap(
        boost::bind(&connection::handle_read, shared_from_this(),
          boost::asio::placeholders::error,
          boost::asio::placeholders::bytes_transferred)));
}


std::string FormatTime(boost::posix_time::ptime now)
{
	static std::locale loc(std::cout.getloc(),
		new boost::posix_time::time_facet("%d/%b/%Y:%H:%M:%S"));

	std::stringstream ss;
	ss.imbue(loc);
	ss << now;
	return ss.str();
}

void connection::handle_read(const boost::system::error_code& e,
    std::size_t bytes_transferred)
{
  using namespace boost::posix_time;
  using namespace boost::gregorian;

  if (!e)
  {
    boost::tribool result;
    boost::tie(result, boost::tuples::ignore) = request_parser_.parse(
        request_, buffer_.data(), buffer_.data() + bytes_transferred);

    if (result)
    {
      reply reply_;

      keepalive_ = false;
      if ((request_.http_version_major > 1) ||
          ((request_.http_version_major == 1) &&
	   (request_.http_version_minor > 0))) {
      	keepalive_ = true;

	std::string cxn_hdr = request_.get_header("connection");
	if (cxn_hdr == "close")
		keepalive_ = false;
      }

      request_handler_.handle_request(request_, reply_, keepalive_);
      boost::asio::async_write(socket_, reply_.to_buffers(),
          strand_.wrap(
            boost::bind(&connection::handle_write, shared_from_this(),
              boost::asio::placeholders::error)));

      std::string addrstr = peer.address().to_string();
      std::string timestr = FormatTime(second_clock::universal_time());
      printf("%s - - [%s -0000] \"%s %s HTTP/%d.%d\" %d %lu\n",
      	     addrstr.c_str(),
	     timestr.c_str(),
	     request_.method.c_str(),
	     request_.uri.c_str(),
	     request_.http_version_major,
	     request_.http_version_minor,
	     reply_.status,
	     reply_.content.size());

      if (keepalive_) {
	    request_.clear();
	    request_parser_.reset();
	    socket_.async_read_some(boost::asio::buffer(buffer_),
	        strand_.wrap(
	          boost::bind(&connection::handle_read, shared_from_this(),
	            boost::asio::placeholders::error,
	            boost::asio::placeholders::bytes_transferred)));
      }
    }
    else if (!result)
    {
      reply reply_ = reply::stock_reply(reply::bad_request);
      boost::asio::async_write(socket_, reply_.to_buffers(),
          strand_.wrap(
            boost::bind(&connection::handle_write, shared_from_this(),
              boost::asio::placeholders::error)));
    }
    else
    {
      socket_.async_read_some(boost::asio::buffer(buffer_),
          strand_.wrap(
            boost::bind(&connection::handle_read, shared_from_this(),
              boost::asio::placeholders::error,
              boost::asio::placeholders::bytes_transferred)));
    }
  }

  // If an error occurs then no new asynchronous operations are started. This
  // means that all shared_ptr references to the connection object will
  // disappear and the object will be destroyed automatically after this
  // handler returns. The connection class's destructor closes the socket.
}

void connection::handle_write(const boost::system::error_code& e)
{
  if (!e && !keepalive_)
  {
    // Initiate graceful connection closure.
    boost::system::error_code ignored_ec;
    socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignored_ec);
  }

  // No new asynchronous operations are started. This means that all shared_ptr
  // references to the connection object will disappear and the object will be
  // destroyed automatically after this handler returns. The connection class's
  // destructor closes the socket.
}

} // namespace server3
} // namespace http
