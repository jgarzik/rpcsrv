#ifndef __RPCSRV_SERVER_H__
#define __RPCSRV_SERVER_H__

#include <string>
#include <event2/http.h>
#include <univalue.h>

extern unsigned int opt_json_indent;

extern UniValue jrpcOk(const UniValue& rpcreq, const UniValue& result);
extern UniValue jrpcErr(const UniValue& rpcreq, int code, std::string msg);
extern void send_json_response(evhttp_request *req, const UniValue& rv);

extern void rpc_home(evhttp_request *req, void *);
extern void rpc_jsonrpc(evhttp_request *req, void *);

#endif // __RPCSRV_SERVER_H__
