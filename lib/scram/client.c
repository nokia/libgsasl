/* client.c --- SASL SCRAM client side functions.
 * Copyright (C) 2009  Simon Josefsson
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
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

/* Get specification. */
#include "scram.h"

/* Get malloc, free. */
#include <stdlib.h>

/* Get memcpy, strlen. */
#include <string.h>

#include "tokens.h"
#include "parser.h"
#include "printer.h"

#define CNONCE_ENTROPY_BYTES 18

struct scram_client_state
{
  int step;
  struct scram_client_first cf;
  struct scram_server_first sf;
  struct scram_client_final cl;
  struct scram_server_final sl;
};

int
_gsasl_scram_sha1_client_start (Gsasl_session * sctx, void **mech_data)
{
  struct scram_client_state *state;
  char buf[CNONCE_ENTROPY_BYTES];
  int rc;

  state = (struct scram_client_state *) calloc (sizeof (*state), 1);
  if (state == NULL)
    return GSASL_MALLOC_ERROR;

  rc = gsasl_nonce (buf, CNONCE_ENTROPY_BYTES);
  if (rc != GSASL_OK)
    {
      free (state);
      return rc;
    }

  rc = gsasl_base64_to (buf, CNONCE_ENTROPY_BYTES,
			&state->cf.client_nonce, NULL);
  if (rc != GSASL_OK)
    {
      free (state);
      return rc;
    }

  *mech_data = state;

  return GSASL_OK;
}

int
_gsasl_scram_sha1_client_step (Gsasl_session * sctx,
			       void *mech_data,
			       const char *input, size_t input_len,
			       char **output, size_t * output_len)
{
  struct scram_client_state *state = mech_data;
  int res = GSASL_MECHANISM_CALLED_TOO_MANY_TIMES;
  int rc;

  *output = NULL;
  *output_len = 0;

  switch (state->step)
    {
    case 0:
      {
	const char *p;

	/* FIXME */
	state->cf.cbflag = 'n';

	p = gsasl_property_get (sctx, GSASL_AUTHID);
	if (!p)
	  return GSASL_NO_AUTHID;

	/* FIXME check that final document uses query strings. */
	rc = gsasl_saslprep (p, GSASL_ALLOW_UNASSIGNED,
			     &state->cf.username, NULL);
	if (rc != GSASL_OK)
	  return rc;

	p = gsasl_property_get (sctx, GSASL_AUTHZID);
	if (p)
	  state->cf.authzid = strdup (p);

	rc = scram_print_client_first (&state->cf, output);
	if (rc == -2)
	  return GSASL_MALLOC_ERROR;
	else if (rc != 0)
	  return GSASL_AUTHENTICATION_ERROR;

	*output_len = strlen (*output);

	/* Save "cbind" for next step. */
	p = strchr (*output, ',');
	if (!p)
	    return GSASL_AUTHENTICATION_ERROR;
	p++;
	p = strchr (p, ',');
	if (!p)
	    return GSASL_AUTHENTICATION_ERROR;
	p++;
	rc = gsasl_base64_to (*output, p - *output, &state->cl.cbind, NULL);
	if (rc != 0)
	  return rc;

	/* We are done. */
	state->step++;
	return GSASL_NEEDS_MORE;
	break;
      }

    case 1:
      {
	if (strlen (input) != input_len)
	  return GSASL_MECHANISM_PARSE_ERROR;

	if (scram_parse_server_first (input, &state->sf) < 0)
	  return GSASL_MECHANISM_PARSE_ERROR;

	if (strlen (state->sf.nonce) < strlen (state->cf.client_nonce) ||
	    memcmp (state->cf.client_nonce, state->sf.nonce,
		    strlen (state->cf.client_nonce)) != 0)
	  return GSASL_AUTHENTICATION_ERROR;

	state->cl.nonce = strdup (state->sf.nonce);
	state->cl.proof = strdup ("proof");

	rc = scram_print_client_final (&state->cl, output);
	if (rc != 0)
	  return GSASL_MALLOC_ERROR;

	*output_len = strlen (*output);

	state->step++;
	return GSASL_NEEDS_MORE;
	break;
      }

    case 2:
      {
	if (strlen (input) != input_len)
	  return GSASL_MECHANISM_PARSE_ERROR;

	if (scram_parse_server_final (input, &state->sl) < 0)
	  return GSASL_MECHANISM_PARSE_ERROR;

	/* FIXME verify verifier. */

	state->step++;
	return GSASL_OK;
	break;
      }

    default:
      break;
    }

  return res;
}

void
_gsasl_scram_sha1_client_finish (Gsasl_session * sctx, void *mech_data)
{
  struct scram_client_state *state = mech_data;

  if (!state)
    return;

  scram_free_client_first (&state->cf);
  scram_free_server_first (&state->sf);
  scram_free_client_final (&state->cl);
  scram_free_server_final (&state->sl);

  free (state);
}