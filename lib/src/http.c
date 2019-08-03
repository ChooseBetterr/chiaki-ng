/*
 * This file is part of Chiaki.
 *
 * Chiaki is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Chiaki is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Chiaki.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <chiaki/http.h>

#include <stdbool.h>
#include <string.h>
#include <sys/socket.h>


CHIAKI_EXPORT void chiaki_http_header_free(ChiakiHttpHeader *header)
{
	while(header)
	{
		ChiakiHttpHeader *cur = header;
		header = header->next;
		free(cur);
	}
}

CHIAKI_EXPORT ChiakiErrorCode chiaki_http_header_parse(ChiakiHttpHeader **header, char *buf, size_t buf_size)
{
	*header = NULL;
#define FAIL(reason) do { chiaki_http_header_free(*header); return (reason); } while(0);
	char *key_ptr = buf;
	char *value_ptr = NULL;

	for(char *end = buf + buf_size; buf<end; buf++)
	{
		char c = *buf;
		if(!c)
			break;

		if(!value_ptr)
		{
			if(c == ':')
			{
				if(key_ptr == buf)
					FAIL(CHIAKI_ERR_INVALID_DATA);
				*buf = '\0';
				buf++;
				if(buf == end || *buf != ' ')
					FAIL(CHIAKI_ERR_INVALID_DATA);
				buf++;
				if(buf == end)
					FAIL(CHIAKI_ERR_INVALID_DATA);
				value_ptr = buf;
			}
			else if(c == '\r' || c == '\n')
			{
				if(key_ptr + 1 < buf) // no : encountered yet
					FAIL(CHIAKI_ERR_INVALID_DATA);
				key_ptr = buf + 1;
			}
		}
		else
		{
			if(c == '\r' || c == '\n')
			{
				if(value_ptr == buf) // empty value
					FAIL(CHIAKI_ERR_INVALID_DATA);

				*buf = '\0';
				ChiakiHttpHeader *entry = malloc(sizeof(ChiakiHttpHeader));
				if(!entry)
					FAIL(CHIAKI_ERR_MEMORY);
				entry->key = key_ptr;
				entry->value = value_ptr;
				entry->next = *header;
				*header = entry;

				key_ptr = buf + 1;
				value_ptr = NULL;
			}
		}
	}
	return CHIAKI_ERR_SUCCESS;
#undef FAIL
}

CHIAKI_EXPORT void chiaki_http_response_fini(ChiakiHttpResponse *response)
{
	if(!response)
		return;
	chiaki_http_header_free(response->headers);
}

CHIAKI_EXPORT ChiakiErrorCode chiaki_http_response_parse(ChiakiHttpResponse *response, char *buf, size_t buf_size)
{
	static const char *http_version = "HTTP/1.1 ";
	static const size_t http_version_size = 9;

	if(buf_size < http_version_size)
		return CHIAKI_ERR_INVALID_DATA;

	if(strncmp(buf, http_version, http_version_size) != 0)
		return CHIAKI_ERR_INVALID_DATA;

	buf += http_version_size;
	buf_size -= http_version_size;

	char *line_end = memchr(buf, '\r', buf_size);
	if(!line_end)
		return CHIAKI_ERR_INVALID_DATA;
	size_t line_length = (line_end - buf) + 2;
	if(buf_size <= line_length || line_end[1] != '\n')
		return CHIAKI_ERR_INVALID_DATA;
	*line_end = '\0';

	char *endptr;
	response->code = (int)strtol(buf, &endptr, 10);
	if(response->code == 0)
		return CHIAKI_ERR_INVALID_DATA;

	buf += line_length;
	buf_size -= line_length;

	return chiaki_http_header_parse(&response->headers, buf, buf_size);
}

CHIAKI_EXPORT ChiakiErrorCode chiaki_recv_http_header(int sock, char *buf, size_t buf_size, size_t *header_size, size_t *received_size, ChiakiStopPipe *stop_pipe, uint64_t timeout_ms)
{
	// 0 = ""
	// 1 = "\r"
	// 2 = "\r\n"
	// 3 = "\r\n\r"
	// 4 = "\r\n\r\n" (final)
	int nl_state = 0;
	static const int transitions_r[] = { 1, 1, 3, 1 };
	static const int transitions_n[] = { 0, 2, 0, 4 };

	*received_size = 0;
	while(true)
	{
		if(stop_pipe)
		{
			ChiakiErrorCode err = chiaki_stop_pipe_select_single(stop_pipe, sock, timeout_ms);
			if(err != CHIAKI_ERR_SUCCESS)
				return err;
		}

		ssize_t received = recv(sock, buf, buf_size, 0);
		if(received <= 0)
			return CHIAKI_ERR_NETWORK;

		*received_size += received;
		for(; received > 0; buf++, received--)
		{
			switch(*buf)
			{
				case '\r':
					nl_state = transitions_r[nl_state];
					break;
				case '\n':
					nl_state = transitions_n[nl_state];
					break;
				default:
					nl_state = 0;
					break;
			}
			if(nl_state == 4)
			{
				received--;
				break;
			}
		}

		if(nl_state == 4)
		{
			*header_size = *received_size - received;
			break;
		}
	}

	return CHIAKI_ERR_SUCCESS;
}