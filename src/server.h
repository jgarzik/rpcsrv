#ifndef __RPCSRV_SERVER_H__
#define __RPCSRV_SERVER_H__

#include <string>
#include <event2/http.h>
#include <univalue.h>

class RpcApiInfo {
public:
	std::string	name;
	std::string	version;
	std::string	uripath;

	RpcApiInfo() {}
	RpcApiInfo(const std::string& name_, const std::string& version_,
		   const std::string& uripath_) : name(name_),
						  version(version_),
						  uripath(uripath_) {}

	virtual UniValue execute(const UniValue& jreq) = 0;
	virtual UniValue list_methods() = 0;
};

extern unsigned int opt_json_indent;

extern UniValue jrpcOk(const UniValue& rpcreq, const UniValue& result);
extern UniValue jrpcErr(const UniValue& rpcreq, int code, std::string msg);
extern void send_json_response(evhttp_request *req, const UniValue& rv);

extern bool register_endpoints(struct evhttp* http);

#endif // __RPCSRV_SERVER_H__
