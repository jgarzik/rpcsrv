//
// request_handler.cpp
// ~~~~~~~~~~~~~~~~~~~
//
// Copyright 2012 Red Hat, Inc.
// Copyright (c) 2003-2012 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "request_handler.h"
#include <fstream>
#include <sstream>
#include <string>
#include <boost/lexical_cast.hpp>
#include "mime_types.h"
#include "reply.h"
#include "request.h"
#include "json/json_spirit_reader.h"
#include "json/json_spirit_writer.h"
#include "json/json_spirit_utils.h"
#include "rpcs.h"

namespace http {
namespace server3 {

request_handler::request_handler(const std::string& doc_root)
  : doc_root_(doc_root)
{
}

void request_handler::handle_request(const request& req, reply& rep,
				     bool keepalive)
{
	// Decode url to path.
	std::string request_path;
	if (!url_decode(req.uri, request_path)) {
		rep = reply::stock_reply(reply::bad_request);
		return;
	}

	if (req.method != "GET" && req.method != "POST") {
		rep = reply::stock_reply(reply::bad_request);
		return;
	}

	// Request path must be absolute and not contain "..".
	if (request_path.empty() || request_path[0] != '/'
			|| request_path.find("..") != std::string::npos) {
		rep = reply::stock_reply(reply::bad_request);
		return;
	}

	if (request_path != "/v1/api") {
		rep = reply::stock_reply(reply::not_found);
		return;
	}

	json_spirit::Value jv;
	if (!json_spirit::read(req.content, jv) ||
	    (jv.type() != json_spirit::obj_type)) {
		rep = reply::stock_reply(reply::bad_request);
		return;
	}

	json_spirit::Object result;
	json_spirit::Object query = jv.get_obj();

	json_rpc::exec(query, result);

	// Fill out the reply to be sent to the client.
	rep.status = reply::ok;
	rep.content = json_spirit::write(result);
	rep.content += "\r\n";

	rep.headers.push_back(header("Content-Length",
			boost::lexical_cast<std::string>(rep.content.size())));
	rep.headers.push_back(header("Content-Type", "application/json"));
	if (!keepalive)
		rep.headers.push_back(header("Connection", "close"));
}

bool request_handler::url_decode(const std::string& in, std::string& out)
{
  out.clear();
  out.reserve(in.size());
  for (std::size_t i = 0; i < in.size(); ++i)
  {
    if (in[i] == '%')
    {
      if (i + 3 <= in.size())
      {
        int value = 0;
        std::istringstream is(in.substr(i + 1, 2));
        if (is >> std::hex >> value)
        {
          out += static_cast<char>(value);
          i += 2;
        }
        else
        {
          return false;
        }
      }
      else
      {
        return false;
      }
    }
    else if (in[i] == '+')
    {
      out += ' ';
    }
    else
    {
      out += in[i];
    }
  }
  return true;
}

} // namespace server3
} // namespace http
