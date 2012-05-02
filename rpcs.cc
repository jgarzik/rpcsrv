
#include "json/json_spirit_reader.h"
#include "json/json_spirit_writer.h"
#include "json/json_spirit_utils.h"
#include "rpcs.h"

#define ARRAYLEN(array)     (sizeof(array)/sizeof((array)[0]))

namespace json_rpc {

static bool ping(json_spirit::Object &query, json_spirit::Object &result)
{
	json_spirit::Value tmpval(true);

	result.push_back(json_spirit::Pair("result", tmpval));

	return true;
}

static bool echo(json_spirit::Object &query, json_spirit::Object &result)
{
	json_spirit::Value tmpval;

	tmpval = json_spirit::find_value(query, "params");
	result.push_back(json_spirit::Pair("result", tmpval));	// may be null

	return true;
}

struct handler {
	std::string	method;
	bool		(*actor)(json_spirit::Object &query,
				 json_spirit::Object &result);
};

static const struct handler rpc_handlers[] =
{
	{ "ping", ping },
	{ "echo", echo },
};

void exec(json_spirit::Object &query, json_spirit::Object &result)
{
	json_spirit::Value tmpval;

	tmpval = json_spirit::find_value(query, "version");
	if (tmpval.type() != json_spirit::null_type)
		result.push_back(json_spirit::Pair("version", tmpval));
	else
		result.push_back(json_spirit::Pair("version", "1.0"));	// fb to 1.0

	tmpval = json_spirit::find_value(query, "id");
	result.push_back(json_spirit::Pair("id", tmpval));	// may be null

	tmpval = json_spirit::find_value(query, "method");
	if (tmpval.type() != json_spirit::str_type) {
		result.push_back(json_spirit::Pair("error", "invalid method"));
		return;
	}

	unsigned int i = 0;
	for (; i < ARRAYLEN(rpc_handlers); i++) {
		if (tmpval.get_str() == rpc_handlers[i].method) {
			bool rc = rpc_handlers[i].actor(query, result);
			if (rc)
				return;

			result.push_back(json_spirit::Pair("error", "RPC failed"));
			return;
		}
	}

	result.push_back(json_spirit::Pair("error", "unknown RPC method"));
}

}	// namespace json_rpc

