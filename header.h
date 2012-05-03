//
// header.hpp
// ~~~~~~~~~~
//
// Copyright 2012 Red Hat, Inc.
// Copyright (c) 2003-2012 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef HTTP_SERVER3_HEADER_HPP
#define HTTP_SERVER3_HEADER_HPP

#include <string>

namespace http {
namespace server3 {

struct header
{
	std::string name;
	std::string value;

	header() { }

	header(std::string name_, std::string value_) {
		name = name_;
		value = value_;
	}
};

} // namespace server3
} // namespace http

#endif // HTTP_SERVER3_HEADER_HPP
