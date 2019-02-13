#ifndef __RPCSRV_MYAPI_1_H__
#define __RPCSRV_MYAPI_1_H__

#include <univalue.h>
#include <event2/http.h>
#include "server.h"

class MyApiInfo : public RpcApiInfo {
public:
	MyApiInfo() : RpcApiInfo("myapi", "1", "/rpc/1") {}

	UniValue execute(const UniValue& jreq);
	UniValue list_methods();
	const RpcCallInfo& call_info(const std::string& method);
};

#endif // __RPCSRV_MYAPI_1_H__
