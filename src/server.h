#ifndef __RPCSRV_SERVER_H__
#define __RPCSRV_SERVER_H__

#include <event2/http.h>

extern void rpc_home(evhttp_request *req, void *);
extern void rpc_jsonrpc(evhttp_request *req, void *);

#endif // __RPCSRV_SERVER_H__
