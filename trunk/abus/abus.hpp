/*
 * Copyright (C) 2011-2012 Stephane Fillod
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

	/*! Get the JSON type of a parameter from a RPC.
		\return a nul of positive number representing the JSON type (JSON_{INT,FLOAT,STRING,TRUE,FALSE,NULL}), a negative value in case of error
		\sa json_rpc_get_type()
	 */
	int get_type(const char *name)
		{ return json_rpc_get_type(m_json_rpc, name); }

	/*! Get the value of a parameter of type integer from a RPC.
		\return	0	if successful, non nul value otherwise
		\sa json_rpc_get_int()
	 */
	int get_int(const char *name, int *val)
		{ return json_rpc_get_int(m_json_rpc, name, val); }
	/*! Get the value of a parameter of type long long integer from a RPC.
		\return	0	if successful, non nul value otherwise
		\sa json_rpc_get_llint()
	 */
	int get_llint(const char *name, long long *val)
		{ return json_rpc_get_llint(m_json_rpc, name, val); }
	/*! Get the value of a parameter of type bool from a RPC.
		\return	0	if successful, non nul value otherwise
		\sa json_rpc_get_bool()
	 */
	int get_bool(const char *name, bool *val)
		{ return json_rpc_get_bool(m_json_rpc, name, val); }
	/*! Get the value of a parameter of type double float from a RPC.
		\return	0	if successful, non nul value otherwise
		\sa json_rpc_get_double()
	 */
	int get_double(const char *name, double *val)
		{ return json_rpc_get_double(m_json_rpc, name, val); }
	/*! Get the value of a parameter of type string (nul terminated) from a RPC.
		\return	0	if successful, non nul value otherwise
		\sa json_rpc_get_str()
	 */
	int get_str(const char *name, char *val, size_t n)
		{ return json_rpc_get_str(m_json_rpc, name, val, n); }

	/*! Get the value of a parameter of type string from a RPC.
		\return	0	if successful, non nul value otherwise
		\sa json_rpc_get_strn()
	 */
	int get_strn(const char *name, char *val, size_t *n)
		{ return json_rpc_get_strn(m_json_rpc, name, val, n); }

	/*! Get a pointer to the value of a parameter of type string from a RPC.
		\return	0	if successful, non nul value otherwise
		\sa json_rpc_get_strp()
	 */
	int get_strp(const char *name, const char **pval, size_t *n)
		{ return json_rpc_get_strp(m_json_rpc, name, pval, n); }

	/*! Get the count of element in a named array from a RPC
		\return a nul of positive number representing the count of objects in the array, a negative value in case of error
		\sa json_rpc_get_array_count()
	 */
	int get_array_count(const char *name)
		{ return json_rpc_get_array_count(m_json_rpc, name); }
	/*! Aim the json_rpc_get_{int,bool,..} functions at an object within an array from a RPC
		\return	0	if successful, non nul value otherwise
		\sa json_rpc_get_point_at()
	 */
	int get_point_at(const char *name, int idx)
		{ return json_rpc_get_point_at(m_json_rpc, name, idx); }

	/*! Append to a RPC a new parameter and its value of type integer
		\return	0	if successful, non nul value otherwise
		\sa json_rpc_append_int()
	 */
	int append_int(const char *name, int val)
		{ return json_rpc_append_int(m_json_rpc, name, val); }
	/*! Append to a RPC a new parameter and its value of type long long integer
		\return	0	if successful, non nul value otherwise
		\sa json_rpc_append_llint()
	 */
	int append_llint(const char *name, long long val)
		{ return json_rpc_append_llint(m_json_rpc, name, val); }
	/*! Append to a RPC a new parameter and its value of type boolean
		\return	0	if successful, non nul value otherwise
		\sa json_rpc_append_bool()
	 */
	int append_bool(const char *name, bool val)
		{ return json_rpc_append_bool(m_json_rpc, name, val); }
	/*! Append to a RPC a new parameter and its value of type double float
		\return	0	if successful, non nul value otherwise
		\sa json_rpc_append_double()
	 */
	int append_double(const char *name, double val)
		{ return json_rpc_append_double(m_json_rpc, name, val); }
	/*! Append to a RPC a new parameter and its value of type null
		\return	0	if successful, non nul value otherwise
		\sa json_rpc_append_null()
	 */
	int append_null(const char *name)
		{ return json_rpc_append_null(m_json_rpc, name); }
	/*! Append to a RPC a new parameter and its value of type string
		\return	0	if successful, non nul value otherwise
		\sa json_rpc_append_strn()
	 */
	int append_strn(const char *name, const char *val, size_t n)
		{ return json_rpc_append_strn(m_json_rpc, name, val, n); }
	/*! Append to a RPC a new parameter and its value of type string
		\return	0	if successful, non nul value otherwise
		\sa json_rpc_append_str()
	 */
	int append_str(const char *name, const char *s)
		{ return json_rpc_append_str(m_json_rpc, name, s); }
	/*! Append to a RPC a list of JSON atoms
		\return	0	if successful, non nul value otherwise
		\sa json_rpc_append_args()
	 */
	int append_args(void *dummy, ...)
		{ int ret; va_list ap; va_start(ap, dummy); ret = json_rpc_append_vargs(m_json_rpc, ap); va_end(ap); return ret; }

	/*! Set the error code and message in a RPC before response.
		\return	0	if successful, non nul value otherwise
		\sa json_rpc_set_error()
	 */
	int set_error(int error_code, const char *message = NULL)
		{ return json_rpc_set_error(m_json_rpc, error_code, message); }

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

	/*! Append to a RPC an attribute
		\return	0	if successful, non nul value otherwise
		\sa abus_append_attr()
	 */
	int append_attr(const char *service_name, const char *attr_name)
		{ return abus_append_attr(m_abus, m_json_rpc, service_name, attr_name); }

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

	/*! Append to a RPC an attribute
		\return	0	if successful, non nul value otherwise
		\sa abus_append_attr()
	 */
	int append_attr(const char *service_name, const char *attr_name)
		{ return abus_append_attr(m_abus, m_json_rpc, service_name, attr_name); }

private:
	abus_t *m_abus;
};


/*! C++ friendly callback declaration helper.
  To be used in public class declaration, along with declpp_method()/event_subscribepp()
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

	/*! Declare a new method in a service
		\return	0	if successful, non nul value otherwise
		\sa declpp_method(), undecl_method()
	 */
	int decl_method(const char *service_name, const char *method_name,
					abus_callback_t method_callback, int flags = 0, void *arg = NULL,
					const char *descr = NULL, const char *fmt = NULL, const char *result_fmt = NULL)
		{ return abus_decl_method(&m_abus, service_name, method_name, method_callback, flags, arg, descr, fmt, result_fmt); }

	/*! Helper macro to be used with abus_declpp_method_member() */
#define declpp_method(_service_name, _method_name, _obj, _method, _flags, descr, fmt, res_fmt) \
        decl_method((_service_name), (_method_name), &(_obj)->_method##Wrapper, (_flags), (void *)(_obj), descr, fmt, res_fmt)

	/*! Undeclare a method from a service
		\return	0	if successful, non nul value otherwise
		\sa decl_method()
	 */
	int undecl_method(const char *service_name, const char *method_name)
		{ return abus_undecl_method(&m_abus, service_name, method_name); }

	/*! Instantiate a new RPC for invocation */
	cABusRequestMethod *RequestMethod(const char *service_name, const char *method_name) {
		cABusRequestMethod *p = new cABusRequestMethod(&m_abus);
		if (p) p->init(service_name, method_name);
		return p;
	}

	/*! Declare a new event in a service */
	int decl_event(const char *service_name, const char *event_name, const char *descr = NULL, const char *fmt = NULL)
		{ return abus_decl_event(&m_abus, service_name, event_name, descr, fmt); }

	/*! Undeclare an event from a service
		\return	0	if successful, non nul value otherwise
		\sa decl_event()
	 */
	int undecl_event(const char *service_name, const char *event_name)
		{ return abus_undecl_event(&m_abus, service_name, event_name); }

	/*! Instantiate a new event for publishing */
	cABusRequestEvent *RequestEvent(const char *service_name, const char *event_name) {
		cABusRequestEvent *p = new cABusRequestEvent(&m_abus);
		if (p) p->init(service_name, event_name);
		return p;
	}

	/*! Subscribe to an event from a service
		\return	0	if successful, non nul value otherwise
		\sa event_subscribepp(), event_unsubscribe()
	 */
	int event_subscribe(const char *service_name, const char *event_name, abus_callback_t callback, int flags = ABUS_RPC_FLAG_NONE, void *arg = NULL, int timeout = -1)
		{ return abus_event_subscribe(&m_abus, service_name, event_name, callback, flags, arg, timeout); }

	/*! Unsubscribe from an event
		\return	0	if successful, non nul value otherwise
		\sa event_subscribe()
	 */
	int event_unsubscribe(const char *service_name, const char *event_name, abus_callback_t callback, void *arg = NULL, int timeout = -1)
		{ return abus_event_unsubscribe(&m_abus, service_name, event_name, callback, arg, timeout); }


	/*! Helper macro to be used with abus_declpp_method_member() */
#define event_subscribepp(_service_name, _event_name, _obj, _method, _flags, _timeout) \
        event_subscribe((_service_name), (_event_name), &(_obj)->_method##Wrapper, (_flags), (void *)(_obj), (_timeout))
	/*! Helper macro to be used with abus_declpp_method_member() */
#define event_unsubscribepp(_service_name, _event_name, _obj, _method, _timeout) \
        event_unsubscribe((_service_name), (_event_name), &(_obj)->_method##Wrapper, (void *)(_obj), (_timeout))

	/*! Declare a new attribute of type integer in a service */
	int decl_attr_int(const char *service_name, const char *attr_name, int *val = NULL, int flags = ABUS_RPC_FLAG_NONE, const char *descr = NULL)
		{ return abus_decl_attr_int(&m_abus, service_name, attr_name, val, flags, descr); }
	/*! Declare a new attribute of type long long integer in a service */
	int decl_attr_llint(const char *service_name, const char *attr_name, long long *val = NULL, int flags = ABUS_RPC_FLAG_NONE, const char *descr = NULL)
		{ return abus_decl_attr_llint(&m_abus, service_name, attr_name, val, flags, descr); }
	/*! Declare a new attribute of type boolean in a service */
	int decl_attr_bool(const char *service_name, const char *attr_name, bool *val = NULL, int flags = ABUS_RPC_FLAG_NONE, const char *descr = NULL)
		{ return abus_decl_attr_bool(&m_abus, service_name, attr_name, val, flags, descr); }
	/*! Declare a new attribute of type double in a service */
	int decl_attr_double(const char *service_name, const char *attr_name, double *val = NULL, int flags = ABUS_RPC_FLAG_NONE, const char *descr = NULL)
		{ return abus_decl_attr_double(&m_abus, service_name, attr_name, val, flags, descr); }
	/*! Declare a new attribute of type string in a service */
	int decl_attr_str(const char *service_name, const char *attr_name, char *val, size_t n, int flags = ABUS_RPC_FLAG_NONE, const char *descr = NULL)
		{ return abus_decl_attr_str(&m_abus, service_name, attr_name, val, n, flags, descr); }

	/*! Undeclare a method from a service
		\return	0	if successful, non nul value otherwise
		\sa decl_method()
	 */
	int undecl_attr(const char *service_name, const char *attr_name)
		{ return abus_undecl_attr(&m_abus, service_name, attr_name); }

	/*! Notify that the value of an attribute has changed
	    Any listeners of this attribute will receive an event
	 */
	int abus_attr_changed(abus_t *abus, const char *service_name, const char *attr_name)
		{ return abus_attr_changed(&m_abus, service_name, attr_name); }


	/*! Get the value of an attribute of type integer exposed by a service
		\return	0	if successful, non nul value otherwise
	 */
	int attr_get_int(const char *service_name, const char *attr_name, int *val, int timeout = -1)
		{ return abus_attr_get_int(&m_abus, service_name, attr_name, val, timeout); }
	/*! Get the value of an attribute of type long long integer exposed by a service
		\return	0	if successful, non nul value otherwise
	 */
	int attr_get_llint(const char *service_name, const char *attr_name, long long *val, int timeout = -1)
		{ return abus_attr_get_llint(&m_abus, service_name, attr_name, val, timeout); }
	/*! Get the value of an attribute of type boolean exposed by a service
		\return	0	if successful, non nul value otherwise
	 */
	int attr_get_bool(const char *service_name, const char *attr_name, bool *val, int timeout = -1)
		{ return abus_attr_get_bool(&m_abus, service_name, attr_name, val, timeout); }
	/*! Get the value of an attribute of type double float exposed by a service
		\return	0	if successful, non nul value otherwise
	 */
	int attr_get_double(const char *service_name, const char *attr_name, double *val, int timeout = -1)
		{ return abus_attr_get_double(&m_abus, service_name, attr_name, val, timeout); }
	/*! Get the value of an attribute of type string exposed by a service
		\return	0	if successful, non nul value otherwise
	 */
	int attr_get_str(const char *service_name, const char *attr_name, char *val, size_t n, int timeout = -1)
		{ return abus_attr_get_str(&m_abus, service_name, attr_name, val, n, timeout); }
 

	/*! Set the value of an attribute of type integer exposed by a service
		\return	0	if successful, non nul value otherwise
	 */
	int attr_set_int(const char *service_name, const char *attr_name, int val, int timeout = -1)
		{ return abus_attr_set_int(&m_abus, service_name, attr_name, val, timeout); }
	/*! Set the value of an attribute of type long long integer exposed by a service
		\return	0	if successful, non nul value otherwise
	 */
	int attr_set_llint(const char *service_name, const char *attr_name, long long val, int timeout = -1)
		{ return abus_attr_set_llint(&m_abus, service_name, attr_name, val, timeout); }
	/*! Set the value of an attribute of type boolean exposed by a service
		\return	0	if successful, non nul value otherwise
	 */
	int attr_set_bool(const char *service_name, const char *attr_name, bool val, int timeout = -1)
		{ return abus_attr_set_bool(&m_abus, service_name, attr_name, val, timeout); }
	/*! Set the value of an attribute of type double float exposed by a service
		\return	0	if successful, non nul value otherwise
	 */
	int attr_set_double(const char *service_name, const char *attr_name, double val, int timeout = -1)
		{ return abus_attr_set_double(&m_abus, service_name, attr_name, val, timeout); }
	/*! Set the value of an attribute of type string exposed by a service
		\return	0	if successful, non nul value otherwise
	 */
	int attr_set_str(const char *service_name, const char *attr_name, const char *val, int timeout = -1)
		{ return abus_attr_set_str(&m_abus, service_name, attr_name, val, timeout); }


	/*! Subscribe to change event of an attribute from a service
		\return	0	if successful, non nul value otherwise
		\sa attr_onchange_subscribepp(), attr_unsubscribe_onchange()
	 */
	int attr_subscribe_onchange(const char *service_name, const char *attr_name, abus_callback_t callback, int flags = ABUS_RPC_FLAG_NONE, void *arg = NULL, int timeout = -1)
		{ return abus_attr_subscribe_onchange(&m_abus, service_name, attr_name, callback, flags, arg, timeout); }
	/*! Unubscribe from change event of an attribute from a service
		\return	0	if successful, non nul value otherwise
		\sa attr_subscribe_onchange()
	 */
	int attr_unsubscribe_onchange(const char *service_name, const char *attr_name, abus_callback_t callback, void *arg, int timeout)
		{ return abus_attr_unsubscribe_onchange(&m_abus, service_name, attr_name, callback, arg, timeout); }

	/*! Helper macro to be used with abus_declpp_method_member() */
#define attr_onchange_subscribepp(_service_name, _attr_name, _obj, _method, _flags, _timeout) \
        attr_onchange_subscribe((_service_name), (_attr_name), &(_obj)->_method##Wrapper, (_flags), (void *)(_obj), (_timeout))
	/*! Helper macro to be used with abus_declpp_method_member() */
#define attr_onchange_unsubscribepp(_service_name, _attr_name, _obj, _method, _timeout) \
        attr_onchange_unsubscribe((_service_name), (_attr_name), &(_obj)->_method##Wrapper, (void *)(_obj), (_timeout))

private:
	abus_t m_abus;

};

/*! @} */

#endif	/* _ABUS_HPP */
