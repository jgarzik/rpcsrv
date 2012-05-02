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

#define ARRAYLEN(array)     (sizeof(array)/sizeof((array)[0]))

namespace http {
namespace server3 {

request_handler::request_handler(const std::string& doc_root)
  : doc_root_(doc_root)
{
}

static bool jrpc_ping(json_spirit::Object &query, json_spirit::Object &result)
{
	json_spirit::Value tmpval(true);

	result.push_back(json_spirit::Pair("result", tmpval));

	return true;
}

static bool jrpc_echo(json_spirit::Object &query, json_spirit::Object &result)
{
	json_spirit::Value tmpval;

	tmpval = json_spirit::find_value(query, "params");
	result.push_back(json_spirit::Pair("result", tmpval));	// may be null

	return true;
}

struct jrpc_handler {
	std::string	method;
	bool		(*actor)(json_spirit::Object &query,
				 json_spirit::Object &result);
};

static const struct jrpc_handler rpc_handlers[] =
{
	{ "ping", jrpc_ping },
	{ "echo", jrpc_echo },
};

static void json_rpc_handler(json_spirit::Object &query,
			     json_spirit::Object &result)
{
	json_spirit::Value tmpval;

	tmpval = json_spirit::find_value(query, "method");
	if (tmpval.type() != json_spirit::str_type) {
		result.push_back(json_spirit::Pair("error", "invalid method"));
		return;
	}

	unsigned int i = 0;
	for (; i < ARRAYLEN(rpc_handlers); i++) {
		if (tmpval.get_str() == rpc_handlers[i].method) {
			bool rc = rpc_handlers[i].actor(query, result);
			if (rc)
				return;

			result.push_back(json_spirit::Pair("error", "RPC failed"));
			return;
		}
	}

	result.push_back(json_spirit::Pair("error", "unknown RPC method"));
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

	json_spirit::Value tmpval;
	json_spirit::Object result;
	json_spirit::Object query = jv.get_obj();

	tmpval = json_spirit::find_value(query, "version");
	if (tmpval.type() != json_spirit::null_type)
		result.push_back(json_spirit::Pair("version", tmpval));
	else
		result.push_back(json_spirit::Pair("version", "1.0"));	// fb to 1.0

	tmpval = json_spirit::find_value(query, "id");
	result.push_back(json_spirit::Pair("id", tmpval));	// may be null

	json_rpc_handler(query, result);

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
