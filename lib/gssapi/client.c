/* client.c --- SASL mechanism GSSAPI as defined in RFC 2222, client side.
 * Copyright (C) 2002, 2003, 2004  Simon Josefsson
 *
 * This file is part of GNU SASL Library.
 *
 * GNU SASL Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * GNU SASL Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with GNU SASL Library; if not, write to the Free
 * Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307 USA
 *
 */

#include "x-gssapi.h"

#include "shared.h"

struct _Gsasl_gssapi_client_state
{
  int step;
  gss_name_t service;
  gss_ctx_id_t context;
  gss_qop_t qop;
};
typedef struct _Gsasl_gssapi_client_state _Gsasl_gssapi_client_state;

int
_gsasl_gssapi_client_start (Gsasl_session_ctx * sctx, void **mech_data)
{
  _Gsasl_gssapi_client_state *state;
  Gsasl_ctx *ctx;

  ctx = gsasl_client_ctx_get (sctx);
  if (ctx == NULL)
    return GSASL_CANNOT_GET_CTX;

  if (gsasl_client_callback_authentication_id_get (ctx) == NULL)
    return GSASL_NEED_CLIENT_AUTHENTICATION_ID_CALLBACK;

  if (gsasl_client_callback_service_get (ctx) == NULL)
    return GSASL_NEED_CLIENT_SERVICE_CALLBACK;

  state = (_Gsasl_gssapi_client_state *) malloc (sizeof (*state));
  if (state == NULL)
    return GSASL_MALLOC_ERROR;

  state->context = GSS_C_NO_CONTEXT;
  state->service = NULL;
  state->step = 0;
  state->qop = GSASL_QOP_AUTH;

  *mech_data = state;

  return GSASL_OK;
}

int
_gsasl_gssapi_client_step (Gsasl_session_ctx * sctx,
			   void *mech_data,
			   const char *input,
			   size_t input_len,
			   char *output, size_t * output_len)
{
  _Gsasl_gssapi_client_state *state = mech_data;
  Gsasl_client_callback_authentication_id cb_authentication_id;
  Gsasl_client_callback_qop cb_qop;
  Gsasl_client_callback_service cb_service;
  gss_qop_t serverqop;
  Gsasl_ctx *ctx;
  gss_buffer_desc bufdesc, bufdesc2;
  gss_buffer_t buf = GSS_C_NO_BUFFER;
  OM_uint32 maj_stat, min_stat;
  int conf_state;
  int res;

  ctx = gsasl_client_ctx_get (sctx);
  if (ctx == NULL)
    return GSASL_CANNOT_GET_CTX;

  cb_authentication_id = gsasl_client_callback_authentication_id_get (ctx);
  if (cb_authentication_id == NULL)
    return GSASL_NEED_CLIENT_AUTHENTICATION_ID_CALLBACK;

  cb_service = gsasl_client_callback_service_get (ctx);
  if (cb_service == NULL)
    return GSASL_NEED_CLIENT_SERVICE_CALLBACK;

  cb_qop = gsasl_client_callback_qop_get (ctx);

  if (state->service == NULL)
    {
      size_t servicelen = 0;
      size_t hostnamelen = 0;

      res = cb_service (sctx, NULL, &servicelen,
			NULL, &hostnamelen, NULL, NULL);
      if (res != GSASL_OK)
	return res;

      bufdesc.length = servicelen + strlen ("@") + hostnamelen + 1;
      bufdesc.value = malloc (bufdesc.length);
      if (bufdesc.value == NULL)
	return GSASL_MALLOC_ERROR;

      res = cb_service (sctx, (char *) bufdesc.value, &servicelen,
			(char *) bufdesc.value + 1 + servicelen, &hostnamelen,
			NULL, NULL);
      if (res != GSASL_OK)
	{
	  free (bufdesc.value);
	  return res;
	}
      ((char *) bufdesc.value)[servicelen] = '@';
      ((char *) bufdesc.value)[bufdesc.length - 1] = '\0';

      maj_stat = gss_import_name (&min_stat, &bufdesc,
				  GSS_C_NT_HOSTBASED_SERVICE,
				  &state->service);
      free (bufdesc.value);
      if (GSS_ERROR (maj_stat))
	{
	  free (state);
	  return GSASL_GSSAPI_IMPORT_NAME_ERROR;
	}
    }

  switch (state->step)
    {
    case 1:
      bufdesc.length = input_len;
      bufdesc.value = /*XXX*/ (char *) input;
      buf = &bufdesc;
      /* fall through */

    case 0:
      bufdesc2.length = 0;
      bufdesc2.value = NULL;
      maj_stat = gss_init_sec_context (&min_stat,
				       GSS_C_NO_CREDENTIAL,
				       &state->context,
				       state->service,
				       GSS_C_NO_OID,
				       GSS_C_MUTUAL_FLAG |
				       GSS_C_REPLAY_FLAG |
				       GSS_C_SEQUENCE_FLAG |
				       GSS_C_INTEG_FLAG |
				       GSS_C_CONF_FLAG,
				       0,
				       GSS_C_NO_CHANNEL_BINDINGS,
				       buf, NULL, &bufdesc2, NULL, NULL);
      if (maj_stat != GSS_S_COMPLETE && maj_stat != GSS_S_CONTINUE_NEEDED)
	return GSASL_GSSAPI_INIT_SEC_CONTEXT_ERROR;

      if (*output_len < bufdesc2.length)
	{
	  maj_stat = gss_release_buffer (&min_stat, &bufdesc2);
	  return GSASL_TOO_SMALL_BUFFER;
	}

      *output_len = bufdesc2.length;
      memcpy (output, bufdesc2.value, bufdesc2.length);

      if (maj_stat == GSS_S_COMPLETE)
	state->step = 2;
      else
	state->step = 1;

      maj_stat = gss_release_buffer (&min_stat, &bufdesc2);
      if (maj_stat != GSS_S_COMPLETE)
	return GSASL_GSSAPI_RELEASE_BUFFER_ERROR;

      res = GSASL_NEEDS_MORE;
      break;

    case 2:
      if (*output_len <= 4)
	return GSASL_TOO_SMALL_BUFFER;

      /* [RFC 2222 section 7.2.1]:
         The client passes this token to GSS_Unwrap and interprets the
         first octet of resulting cleartext as a bit-mask specifying
         the security layers supported by the server and the second
         through fourth octets as the maximum size output_message to
         send to the server.  The client then constructs data, with
         the first octet containing the bit-mask specifying the
         selected security layer, the second through fourth octets
         containing in network byte order the maximum size
         output_message the client is able to receive, and the
         remaining octets containing the authorization identity.  The
         client passes the data to GSS_Wrap with conf_flag set to
         FALSE, and responds with the generated output_message.  The
         client can then consider the server authenticated. */

      bufdesc.length = input_len;
      bufdesc.value = /*XXX*/ (char *) input;
      maj_stat = gss_unwrap (&min_stat, state->context, &bufdesc,
			     &bufdesc2, &conf_state, &serverqop);
      if (GSS_ERROR (maj_stat))
	return GSASL_GSSAPI_UNWRAP_ERROR;

      memcpy (output, bufdesc2.value, 4);
      maj_stat = gss_release_buffer (&min_stat, &bufdesc2);
      if (GSS_ERROR (maj_stat))
	return GSASL_GSSAPI_RELEASE_BUFFER_ERROR;

      if (cb_qop)
	state->qop = cb_qop (sctx, serverqop);

      if ((state->qop & serverqop) == 0)
	/*  Server does not support what user wanted. */
	return GSASL_GSSAPI_UNSUPPORTED_PROTECTION_ERROR;

      output[0] = state->qop;

      bufdesc.length = *output_len - 4;
      cb_authentication_id (sctx, output + 4, &bufdesc.length);
      bufdesc.length += 4;
      bufdesc.value = output;
      maj_stat = gss_wrap (&min_stat, state->context, 0, GSS_C_QOP_DEFAULT,
			   &bufdesc, &conf_state, &bufdesc2);
      if (GSS_ERROR (maj_stat))
	return GSASL_GSSAPI_WRAP_ERROR;
      if (*output_len < bufdesc2.length)
	{
	  maj_stat = gss_release_buffer (&min_stat, &bufdesc2);
	  return GSASL_TOO_SMALL_BUFFER;
	}
      memcpy (output, bufdesc2.value, bufdesc2.length);
      *output_len = bufdesc2.length;

      maj_stat = gss_release_buffer (&min_stat, &bufdesc2);
      if (GSS_ERROR (maj_stat))
	return GSASL_GSSAPI_RELEASE_BUFFER_ERROR;

      state->step++;
      res = GSASL_OK;
      break;

    default:
      res = GSASL_MECHANISM_CALLED_TOO_MANY_TIMES;
      break;
    }

  return res;
}

int
_gsasl_gssapi_client_finish (Gsasl_session_ctx * sctx, void *mech_data)
{
  _Gsasl_gssapi_client_state *state = mech_data;
  OM_uint32 maj_stat, min_stat;

  maj_stat = gss_release_name (&min_stat, &state->service);
  if (state->context != GSS_C_NO_CONTEXT)
    maj_stat = gss_delete_sec_context (&min_stat, &state->context,
				       GSS_C_NO_BUFFER);

  free (state);

  return GSASL_OK;
}

int
_gsasl_gssapi_client_encode (Gsasl_session_ctx * sctx,
			     void *mech_data,
			     const char *input, size_t input_len,
			     char *output, size_t * output_len)
{
  _Gsasl_gssapi_client_state *state = mech_data;
  OM_uint32 min_stat, maj_stat;
  gss_buffer_desc foo;
  gss_buffer_t input_message_buffer = &foo;
  gss_buffer_desc output_message_buffer;

  foo.length = input_len;
  foo.value = /*XXX*/ input;

  if (state && state->step == 3 &&
      state->qop & (GSASL_QOP_AUTH_INT|GSASL_QOP_AUTH_CONF))
    {
      maj_stat = gss_wrap (&min_stat,
			   state->context,
			   state->qop & GSASL_QOP_AUTH_CONF ? 1 : 0,
			   GSS_C_QOP_DEFAULT,
			   input_message_buffer,
			   NULL,
			   &output_message_buffer);
      if (GSS_ERROR (maj_stat))
	return GSASL_GSSAPI_WRAP_ERROR;
      if (*output_len < output_message_buffer.length)
	return GSASL_TOO_SMALL_BUFFER;
      if (output)
	memcpy (output, output_message_buffer.value,
		output_message_buffer.length);
      *output_len = output_message_buffer.length;
    }
  else
    {
      *output_len = input_len;
      if (output)
	memcpy (output, input, input_len);
      return GSASL_OK;
    }

  return GSASL_OK;
}

int
_gsasl_gssapi_client_decode (Gsasl_session_ctx * sctx,
				 void *mech_data,
				 const char *input, size_t input_len,
				 char *output, size_t * output_len)
{
  _Gsasl_gssapi_client_state *state = mech_data;
  OM_uint32 min_stat, maj_stat;
  gss_buffer_desc foo;
  gss_buffer_t input_message_buffer = &foo;
  gss_buffer_desc output_message_buffer;

  foo.length = input_len;
  foo.value = /*XXX*/ input;

  if (state && state->step == 3 &&
      state->qop & (GSASL_QOP_AUTH_INT|GSASL_QOP_AUTH_CONF))
    {
      maj_stat = gss_unwrap (&min_stat,
			     state->context,
			     input_message_buffer,
			     &output_message_buffer, NULL, NULL);
      if (GSS_ERROR (maj_stat))
	return GSASL_GSSAPI_UNWRAP_ERROR;
      if (*output_len < output_message_buffer.length)
	return GSASL_TOO_SMALL_BUFFER;
      if (output)
	memcpy (output, output_message_buffer.value,
		output_message_buffer.length);
      *output_len = output_message_buffer.length;
    }
  else
    {
      *output_len = input_len;
      if (output)
	memcpy (output, input, input_len);
      return GSASL_OK;
    }

  return GSASL_OK;
}