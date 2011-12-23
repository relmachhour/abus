/*
 * Copyright (C) 2011 Stephane Fillod
 *
 *   This library is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library General Public License as
 *   published by the Free Software Foundation; either version 2.1 of
 *   the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Library General Public License for more details.
 *
 * abus.hpp is a standalone C++ wrapper for A-Bus
 */
#ifndef _ABUS_HPP
#define _ABUS_HPP

#include <abus.h>

/*!
 * \addtogroup abusmm
 * @{
 */

/*! Base class of A-Bus Remote Procedure Call
 */
class cABusRPC {
public:
	/*! Constructor for base class RPC, can self-allocate the json_rpc_t */
	cABusRPC(json_rpc_t *json_rpc = NULL) {
			if (json_rpc) {
				m_json_rpc = json_rpc;
				m_bSelfAlloc = false;
			} else {
				m_json_rpc = new json_rpc_t;
				m_bSelfAlloc = true;
				json_rpc_init(m_json_rpc);
			}
	}
	/*! Destructor */
	virtual ~cABusRPC() { if (m_bSelfAlloc) { json_rpc_cleanup(m_json_rpc); delete m_json_rpc; } }

	/*! Get the JSON type of a parameterfrom a RPC. */
	int get_type(const char *name)
		{ return json_rpc_get_type(m_json_rpc, name); }

	/*! Get the value of a parameter of type integer from a RPC. */
	int get_int(const char *name, int *val)
		{ return json_rpc_get_int(m_json_rpc, name, val); }
	/*! Get the value of a parameter of type bool from a RPC. */
	int get_bool(const char *name, bool *val)
		{ return json_rpc_get_bool(m_json_rpc, name, val); }
	/*! Get the value of a parameter of type double float from a RPC. */
	int get_double(const char *name, double *val)
		{ return json_rpc_get_double(m_json_rpc, name, val); }
	/*! Get the value of a parameter of type string from a RPC. */
	int get_str(const char *name, char *val, size_t n)
		{ return json_rpc_get_str(m_json_rpc, name, val, n); }

	/*! Get the count of element in a named array from a RPC */
	int get_array_count(const char *name)
		{ return json_rpc_get_array_count(m_json_rpc, name); }
	/*! Aim the json_rpc_get_{int,bool,..} functions at an object within an array from a RPC */
	int get_point_at(const char *name, int idx)
		{ return json_rpc_get_point_at(m_json_rpc, name, idx); }

	/*! Append to a RPC a new parameter and its value of type integer */
	int append_int(const char *name, int val)
		{ return json_rpc_append_int(m_json_rpc, name, val); }
	/*! Append to a RPC a new parameter and its value of type boolean */
	int append_bool(const char *name, bool val)
		{ return json_rpc_append_bool(m_json_rpc, name, val); }
	/*! Append to a RPC a new parameter and its value of type double float */
	int append_double(const char *name, double val)
		{ return json_rpc_append_double(m_json_rpc, name, val); }
	/*! Append to a RPC a new parameter and its value of type null */
	int append_null(const char *name)
		{ return json_rpc_append_null(m_json_rpc, name); }
	/*! Append to a RPC a new parameter and its value of type string */
	int append_strn(const char *name, const char *val, size_t n)
		{ return json_rpc_append_strn(m_json_rpc, name, val, n); }
	/*! Append to a RPC a new parameter and its value of type string */
	int append_str(const char *name, const char *s)
		{ return json_rpc_append_str(m_json_rpc, name, s); }
	/*! Append to a RPC a list of JSON atoms */
	int append_args(void *dummy, ...)
		{ int ret; va_list ap; va_start(ap, dummy); ret = json_rpc_append_vargs(m_json_rpc, ap); va_end(ap); return ret; }

	/*! Set the error code and message in a RPC before response. */
	void set_error(int error_code, const char *message = NULL)
		{ json_rpc_set_error(m_json_rpc, error_code, message); }

protected:
	bool		m_bSelfAlloc;	/* m_json_rpc has been alloc'ed by constructor */
	json_rpc_t *m_json_rpc;
};

/*! A-Bus method RPC

  Usage:
\code
	cABusRequestMethod *my_rpc = my_abus->RequestMethod("examplesvc", "sum");

	my_rpc->append_int("a", 5);
	my_rpc->append_int("b", 3);
	if (my_rpc->invoke(ABUS_RPC_FLAG_NONE, 1000) == 0) {
		int res;
		my_rpc->get_int("sum_result", &res);
	}

	delete my_rpc;
\endcode
 */
class cABusRequestMethod: public cABusRPC {
public:
	/*! Constructor for a method RPC */
	cABusRequestMethod(abus_t *abus) : cABusRPC(), m_abus(abus)  {}
	/*! Destructor */
	~cABusRequestMethod() {}

	/* \internal */
	int init(const char *service_name, const char *method_name)
		{ return abus_request_method_init(m_abus, service_name, method_name, m_json_rpc); }

	/*! Invoke the RPC synchronously */
	int invoke(int flags, int timeout)
		{ return abus_request_method_invoke(m_abus, m_json_rpc, flags, timeout); }

	/*! Invoke the RPC asynchronously */
	int invokeAsync(int flags, int timeout, abus_callback_t callback, void *arg)
		{ return abus_request_method_invoke_async(m_abus, m_json_rpc, timeout, callback, flags, arg); }

	/*! Wait of the asynchronously invoked RPC to complete */
	int waitAsync(int timeout)
		{ return abus_request_method_wait_async(m_abus, m_json_rpc, timeout); }

private:
	abus_t *m_abus;
};

/*! A-Bus event RPC

  Usage:
\code
	cABusRequestEvent *my_rpc = my_abus->RequestEvent("examplesvc", "enter_pressed");

	my_rpc->append_str("typed_char", some_chars);
	my_rpc->publish();

	delete my_rpc;
\endcode
 */
class cABusRequestEvent: cABusRPC {
public:
	/*! Constructor for an event RPC */
	cABusRequestEvent(abus_t *abus) : m_abus(abus)  {}
	/*! Destructor */
	~cABusRequestEvent() { }

	/* \internal */
	int init(const char *service_name, const char *event_name)
		{ return abus_request_event_init(m_abus, service_name, event_name, m_json_rpc); }

	/*! Publish the event */
	int publish(int flags = ABUS_RPC_FLAG_NONE)
		{ return abus_request_event_publish(m_abus, m_json_rpc, flags); }

private:
	abus_t *m_abus;
};


/*! C++ friendly callback declaration helper.
  To be used in public class declaration, along with declpp_method/event_subscribepp
 */
#define abus_declpp_method_member(_cl, _method) \
	static void _method##Wrapper(json_rpc_t *json_rpc, void *arg) \
		{ \
			_cl *_pObj = (_cl *) arg; \
			cABusRPC cbRPC(json_rpc);\
			_pObj->_method(&cbRPC); \
		} \
	void _method(cABusRPC *)

/*! A-Bus class
 */
class cABus {
public:
	/*! Constructor for access to A-Bus */
	cABus() { abus_init(&m_abus); }
	/*! Destructor */
	virtual ~cABus() { abus_cleanup(&m_abus); }

	/*! Declare a new method in a service */
	int decl_method(const char *service_name, const char *method_name,
					abus_callback_t method_callback, int flags = 0, void *arg = NULL,
					const char *fmt = NULL, const char *result_fmt = NULL)
		{ return abus_decl_method(&m_abus, service_name, method_name, method_callback, flags, arg, fmt, result_fmt); }

	/*! Helper macro to be used with abus_declpp_method_member() */
#define declpp_method(_service_name, _method_name, _obj, _method, _flags, fmt, res_fmt) \
        decl_method((_service_name), (_method_name), &(_obj)->_method##Wrapper, (_flags), (void *)(_obj), fmt, res_fmt)

	/*! Undeclare a method from a service */
	int undecl_method(const char *service_name, const char *method_name)
		{ return abus_undecl_method(&m_abus, service_name, method_name); }

	/*! Instantiate a new RPC for invocation */
	cABusRequestMethod *RequestMethod(const char *service_name, const char *method_name) {
		cABusRequestMethod *p = new cABusRequestMethod(&m_abus);
		if (p) p->init(service_name, method_name);
		return p;
	}

	/*! Declare a new event in a service */
	int decl_event(const char *service_name, const char *event_name, const char *fmt = NULL)
		{ return abus_decl_event(&m_abus, service_name, event_name, fmt); }

	/*! Instantiate a new event for publishing */
	cABusRequestEvent *RequestEvent(const char *service_name, const char *event_name) {
		cABusRequestEvent *p = new cABusRequestEvent(&m_abus);
		if (p) p->init(service_name, event_name);
		return p;
	}

	/*! Subscribe to an event from a service */
	int event_subscribe(const char *service_name, const char *event_name, abus_callback_t callback, int flags = ABUS_RPC_FLAG_NONE, void *arg = NULL, int timeout = -1)
		{ return abus_event_subscribe(&m_abus, service_name, event_name, callback, flags, arg, timeout); }

	/*! Unsubscribe from an event */
	int event_unsubscribe(const char *service_name, const char *event_name, abus_callback_t callback, void *arg = NULL, int timeout = -1)
		{ return abus_event_unsubscribe(&m_abus, service_name, event_name, callback, arg, timeout); }


	/*! Helper macro to be used with abus_declpp_method_member() */
#define event_subscribepp(_service_name, _event_name, _obj, _method, _flags, _timeout) \
        event_subscribe((_service_name), (_event_name), &(_obj)->_method##Wrapper, (_flags), (void *)(_obj), (_timeout))
	/*! Helper macro to be used with abus_declpp_method_member() */
#define event_unsubscribepp(_service_name, _event_name, _obj, _method, _timeout) \
        event_unsubscribe((_service_name), (_event_name), &(_obj)->_method##Wrapper, (void *)(_obj), (_timeout))

private:
	abus_t m_abus;

};

/*! @} */

#endif	/* _ABUS_HPP */
