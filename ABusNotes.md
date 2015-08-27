# Introduction #

This page contains brainstorming notes for the design of the A-Bus.

The A-Bus is expected to fulfill common IPC needs in embedded systems.

It has to be light, efficient, and comfortable. Although A-Bus borrows some concepts from D-Bus, in no ways it is a Desktop bus.

The A-Bus also seeks to make re-use/access documentation of existing services easier for the developer, following inspiration from plugin declaration in gstreamer.

NB: page to be refactored as scope of work, specs, implementation guidelines,..

# Details #

  * Service/object paradigm
    * _RPC_ and _events_
    * publish/subscribe model for events
    * expose _properties_
    * addressable by (string) name
    * Like D-Bus, the A-Bus object's names are namespaced
    * Objects shall register with their PID (ThID?), protocol name and address and host location(url?) at the name resolution system. Clients don't need to.
  * Flexible RPC, marshaling based on [JSON-RPC](http://json-rpc.org/)
  * Calls can be synchronous or asynchronous (thanks to JSON-RPC's "id")
    * if async, response interpreted as an event
    * callback based, much like closures and signals
    * built-in timeout handling
  * Shall work transparently for the client:
    * local to a multi-thread process
    * local to a host (OS)
    * across network
  * Direct communication between programs, no requirement for a broker but a name resolution system and a client-side cache.
  * Allow more than one protocol bearer, transparently
    * inter-thread comm with a process (which IPC?)
    * UNIX socket
    * TCP socket
    * HTTP?
    * [TIPC](http://tipc.sourceforge.net/), ...
  * Endian-agnostic thanks to [JSON](http://json.org/)
  * _Properties_
    * simplify exposing(server-side) and getting/setting(client-side) of properties (aka attributes)
      * ala sysfs or g\_object, but requiring neither of them
      * offer subscribe "upon\_change" of a property
      * properties spec requested through json-rpc
      * recommended convention: subdir in namespace for state & config. Or simply use Read only & Read Write flags.
      * metadata for type self-declaration ala "parameter specification objects" (G\_TYPE\_PARAM) et al., but without requiring glib
        * would allow property inspector tool
        * inject automatic documentation from doxy?
      * visibility: default to network, may be also host or process
  * Explicit method declaration server-side, semi-automatically connected to language functions
    * -> allow automated type-validation of `"params"`
    * -> allow to browse services
    * -> automatic documentation (extract doxy)?
    * Use swig for automatic attributes/method export?
  * glib-friendly (g\_object & mainloop), but without depending on it -> `libgabus`
  * written in C for POSIX systems & possibly win32 (yike!)
    * wrappers made available thanks to [swig](http://swig.org)
    * careful memory usage tracking in order to prevent memleaks, ..
    * wish: almost no external lib dependency

## libabus ##

This is an end-user library.

  * libabus provides client as well as server helpers
  * embeds a tailored JSON parser
  * handling call timeouts
  * offer support for built-in timer as events
  * recommend to be hosted in its own thread? but not strong requirement
  * `send/append message(obj_dest, args:"%d%d%s%10f", &a, &b, s, tab)` + extra format for blobs, ..
  * [blob](http://en.wikipedia.org/wiki/Opaque_binary_blob) friendly with transparent escaping in JSON format
  * implement call in remote _thread_ context:
    * as explicit wait (mainloop)
    * as signal handler?
      * allow nesting?

+ abus-monitor, abus-inspect, abus-client

## Name resolution system ##

The name resolution system let a client resolve the bearer address of a service by its name. This name resolution then let the client connect directly with the service program.

### Intra-host name resolution ###

Intra-host name resolution relies on a file based, self-announcing system of each service. For each service name, a file with that name in the /tmp/abus directory (or defined by a ABUS\_RESOLV\_PATH environment variable) holds the address of the service.

This daemon-less system let the A-Bus run intra-host in any condition, with no dependency of any already running process, removing a single point of failure.

Services are expected to cleanup their self-announcement in the $ABUS\_RESOLV\_PATH (`atexit(unlink)`), but should they fail to do so upon exit, clients must be robust an gracefully handle the case.

NB: security has to be explicitly addressed by the end-user.

### Inter-host name resolution ###

Name resolution inter hosts, i.e. across a network, goes through an `abus-proxy`. The `abus-proxy` could work in two different ways:
  * mainly a name server, accessed locally as a service itself referenced in the A-Bus service file directory under name `"abus_resolver"`
  * simply mirroring (rsync?) the remote contents of $ABUS\_RESOLV\_PATH locally

## Wishlist ##

  * easy to build (distributed) state machines
  * service/objects compatible with json-rpc, callable from outside world (e.g. JavaScript, ..) through bearer proxy (HTTP or TCP)
  * optional for local RPC (same process/host): [BSON](http://bsonspec.org/) _binary_ encoding
  * security: to be addressed when going multi host
  * availability: redundancy to be addressed later
  * intra-host: helper to create/tear down a shared memory between client and server, with optional ring buffer accessors for single producer/consumer
  * easy to connect to [klish](http://code.google.com/p/klish/)
  * easy to connect to snmp
  * optional libtool versioning export? in order to detect API brokerage

## Worth reading ##

  * JSON-WSP  http://en.wikipedia.org/wiki/JSON-WSP
  * http://en.wikipedia.org/wiki/D-bus

## Inspiration ##
http://gpsd.berlios.de/protocol-evolution.html

##  ##