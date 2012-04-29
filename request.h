//
// request.hpp
// ~~~~~~~~~~~
//
// Copyright 2012 Red Hat, Inc.
// Copyright (c) 2003-2012 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef HTTP_SERVER3_REQUEST_HPP
#define HTTP_SERVER3_REQUEST_HPP

#include <string>
#include <vector>
#include "header.h"

namespace http {
namespace server3 {

/// A request received from a client.
struct request
{
  std::string method;
  std::string uri;
  int http_version_major;
  int http_version_minor;
  std::vector<header> headers;
  std::string content;

  void clear() {
  	method.clear();
	uri.clear();
	http_version_major = 0;
	http_version_minor = 0;
	headers.clear();
	content.clear();
  }

  std::string get_header(std::string name) {
  	std::vector<header>::iterator hi;
	for (hi = headers.begin(); hi != headers.end();hi++) {
		if ((*hi).name == name)
			return (*hi).value;
	}

	return "";
  }
};

} // namespace server3
} // namespace http

#endif // HTTP_SERVER3_REQUEST_HPP
