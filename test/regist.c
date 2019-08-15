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

#include <munit.h>

#include <chiaki/rpcrypt.h>
#include <chiaki/regist.h>

static const uint8_t ambassador[CHIAKI_RPCRYPT_KEY_SIZE] = { 0x13, 0x37, 0xde, 0xad, 0xbe, 0xef, 0xc0, 0xff, 0xee, 0x42, 0x63, 0x68, 0x69, 0x61, 0x6b, 0x69 };
static const uint32_t pin = 13374201;
static const char * const psn_id = "ChiakiNanami1337";

static MunitResult test_aeropause(const MunitParameter params[], void *user)
{
	uint8_t expected[CHIAKI_RPCRYPT_KEY_SIZE] = { 0x0b, 0xe1, 0x2f, 0xbb, 0x4c, 0x7c, 0x99, 0x4a, 0x41, 0x1e, 0x2d, 0x4c, 0xa4, 0x19, 0xf4, 0x35 };
	uint8_t aeropause[CHIAKI_RPCRYPT_KEY_SIZE];
	chiaki_rpcrypt_aeropause(aeropause, ambassador);
	munit_assert_memory_equal(sizeof(expected), aeropause, expected);
	return MUNIT_OK;
}

static MunitResult test_pin_bright(const MunitParameter params[], void *user)
{
	uint8_t expected[CHIAKI_RPCRYPT_KEY_SIZE] = { 0x3f, 0xd0, 0xd6, 0x4f, 0xdc, 0xbb, 0x3e, 0xcc, 0x50, 0xba, 0xed, 0xef, 0x97, 0x34, 0xc7, 0xc9 };
	ChiakiRPCrypt rpcrypt;
	chiaki_rpcrypt_init_regist(&rpcrypt, ambassador, pin);
	munit_assert_memory_equal(sizeof(expected), rpcrypt.bright, expected);
	return MUNIT_OK;
}

static MunitResult test_request_payload(const MunitParameter params[], void *user)
{
	uint8_t expected[] = {
		0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
		0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
		0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
		0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
		0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
		0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
		0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
		0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
		0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x0b, 0xe1, 0x2f, 0xbb,
		0x4c, 0x7c, 0x99, 0x4a, 0x41, 0x1e, 0x2d, 0x4c, 0xa4, 0x19, 0xf4, 0x35, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
		0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
		0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
		0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
		0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
		0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
		0xf2, 0xfd, 0x4d, 0xdc, 0x86, 0x21, 0xda, 0x3d, 0x9f, 0x35, 0xf3, 0xdf, 0x82, 0x58, 0x65, 0x08, 0x6f, 0xb5, 0xee, 0xd1, 0x1e, 0xdd, 0xdd, 0xaa, 0x5a, 0xce, 0x28, 0xdf, 0x2f, 0x13, 0x9c, 0x0c,
		0x66, 0x6f, 0xec, 0x1c, 0x5d, 0xd1, 0x9f, 0x08, 0x15, 0xdd, 0x5c, 0x61, 0x57, 0xe6, 0xad, 0x69, 0x60, 0x12, 0x67, 0x2d, 0x4b, 0x64,
	};

	ChiakiRPCrypt rpcrypt;
	chiaki_rpcrypt_init_regist(&rpcrypt, ambassador, pin);

	uint8_t payload[0x400];
	size_t payload_size = sizeof(payload);
	ChiakiErrorCode err = chiaki_regist_request_payload_format(payload, &payload_size, &rpcrypt, psn_id);
	munit_assert_int(err, ==, CHIAKI_ERR_SUCCESS);
	munit_assert_size(payload_size, ==, sizeof(expected));
	munit_assert_memory_equal(sizeof(expected), payload, expected);
	return MUNIT_OK;
}

MunitTest tests_regist[] = {
	{
		"/aeropause",
		test_aeropause,
		NULL,
		NULL,
		MUNIT_TEST_OPTION_NONE,
		NULL
	},
	{
		"/pin_bright",
		test_pin_bright,
		NULL,
		NULL,
		MUNIT_TEST_OPTION_NONE,
		NULL
	},
	{
		"/request_payload",
		test_request_payload,
		NULL,
		NULL,
		MUNIT_TEST_OPTION_NONE,
		NULL
	},
	{ NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL }
};