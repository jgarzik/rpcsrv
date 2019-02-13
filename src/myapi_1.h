#ifndef __RPCSRV_MYAPI_1_H__
#define __RPCSRV_MYAPI_1_H__

#include <univalue.h>
#include <event2/http.h>

extern UniValue myapi_1_execute(const UniValue& jreq);
extern void myapi_1_list_methods(evhttp_request *req);

#endif // __RPCSRV_MYAPI_1_H__
