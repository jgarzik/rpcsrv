#ifndef __RPCSRV_MYAPI_1_H__
#define __RPCSRV_MYAPI_1_H__

/* Copyright 2019 Bloq Inc.
 * Distributed under the MIT/X11 software license, see the accompanying
 * file COPYING or http://www.opensource.org/licenses/mit-license.php.
 */

#include <univalue.h>
#include <event2/http.h>
#include "server.h"

//
// Defines an example API service
//
class MyApiInfo : public RpcApiInfo {
public:
	MyApiInfo() : RpcApiInfo("myapi",	// service name
				 "1",		// service version
				 "/rpc/1") {}	// service local URI path

	// Each API service must implement these
	UniValue execute(const UniValue& jreq);
	UniValue list_methods();
	const RpcCallInfo& call_info(const std::string& method);
};

#endif // __RPCSRV_MYAPI_1_H__
