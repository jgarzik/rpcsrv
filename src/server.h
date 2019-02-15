#ifndef __RPCSRV_SERVER_H__
#define __RPCSRV_SERVER_H__

#include <string>
#include <map>
#include <event2/http.h>
#include <univalue.h>

//
// Class describes a single JSON-RPC method call
//
class RpcCallInfo {
public:
	std::string	method;		// JSON-RPC method name
	bool		params_array;	// require params to be array?
	bool		params_object;	// require params to be object?
};

//
// API service virtual base class
//
class RpcApiInfo {
public:
	std::string	name;		// service name
	std::string	version;	// service version
	std::string	uripath;	// service local URI path

	RpcApiInfo() {}
	RpcApiInfo(const std::string& name_, const std::string& version_,
		   const std::string& uripath_) : name(name_),
						  version(version_),
						  uripath(uripath_) {}

	// API services implement these
	virtual UniValue execute(const UniValue& jreq) = 0;
	virtual UniValue list_methods() = 0;
	virtual const RpcCallInfo& call_info(const std::string& method) = 0;

	// helpers for API services
	UniValue rpc_call_check(const UniValue& jreq,
                    const std::string& method,
                    const UniValue& params);
	UniValue list_method_helper(const std::map<std::string,RpcCallInfo>& callList);
};

extern unsigned int opt_json_indent;

extern UniValue jrpcOk(const UniValue& rpcreq, const UniValue& result);
extern UniValue jrpcErr(const UniValue& rpcreq, int code, std::string msg);
extern void send_json_response(evhttp_request *req, const UniValue& rv);

extern bool register_endpoints(struct evhttp* http);

#endif // __RPCSRV_SERVER_H__
