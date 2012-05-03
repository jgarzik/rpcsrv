
//
// Copyright 2012 Red Hat, Inc.
//

#ifndef __RPCS_H__
#define __RPCS_H__

#include "json/json_spirit_value.h"

namespace json_rpc {

extern void exec(json_spirit::Object &query, json_spirit::Object &result);

}	// namespace json_rpc

#endif // __RPCS_H__
