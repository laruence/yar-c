/**
 * Yar C test suite - minimal assertion framework
 *
 * Copyright (C) 2026 Xinchen Hui <laruence at gmail dot com>
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#ifndef YAR_TEST_H
#define YAR_TEST_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int yar_tests_run = 0;
static int yar_tests_failed = 0;
static int yar_current_failed = 0;

/* run one test function and account for the result */
#define YAR_RUN(test_fn) \
	do { \
		yar_current_failed = 0; \
		printf("TEST %-44s ", #test_fn); \
		fflush(stdout); \
		test_fn(); \
		yar_tests_run++; \
		if (yar_current_failed) { \
			yar_tests_failed++; \
		} else { \
			printf("PASS\n"); \
			fflush(stdout); \
		} \
	} while (0)

/* assert a condition; on failure mark the test failed, print diagnostics and
   return from the test function */
#define YAR_ASSERT(cond, ...) \
	do { \
		if (!(cond)) { \
			yar_current_failed = 1; \
			printf("FAIL\n  ASSERT %s:%d: %s\n    ", __FILE__, __LINE__, #cond); \
			printf(__VA_ARGS__); \
			printf("\n"); \
			fflush(stdout); \
			return; \
		} \
	} while (0)

#define YAR_SUMMARY() \
	do { \
		printf("=====\n%d tests, %d failed\n", yar_tests_run, yar_tests_failed); \
		fflush(stdout); \
	} while (0)

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
