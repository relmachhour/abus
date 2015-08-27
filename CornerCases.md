# Introduction #

The A-Bus must be robust, rock solid, regardless of the fooling by the end-user program.

Better anticipate any weird corner cases and test them than endure them while fixing a troublesome application.

# Corner Cases #

These are corner cases that have to be appropriately handled by A-Bus, and _tested_.
Of course, automated testing is a must.

Any failure case must not let the system non working, i.e. later valid rpc must still run ok (when applicable).

  * RPC to a non existent service name
  * RPC to a non existent service, that used to exist
  * RPC to an existent service not responding
  * RPC to a service, client not listening the response
  * RPC to a service having its input message queue full
  * RPC to a service, client having its response message queue full
  * RPC to a service for non existent method/property
  * malformed json-rpc command/response
    * zero-length message
    * arbitrary-length binary message
    * subtly broken json message (e.g. missing brace, ..)
  * RPC command/response bigger than max bearer message size
  * huge number of items in json message
  * huge level of nesting in json message