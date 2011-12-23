#!/bin/sh

printf "Content-type: application/json\n\n"

if ! test -x ../abus-send ; then
	echo '{"jsonrpc": "2.0", "error": {"code": -32099, "message": "abus-send failed or not found"}, "id":1}'
	exit 1
fi

# (external) JSON-RPC forwarding
if ! ../abus-send - 2> /dev/null ; then
	echo '{"jsonrpc": "2.0", "error": {"code": -32603, "message": "abus-send exit failure"}, "id":1}'
	exit 1
fi
