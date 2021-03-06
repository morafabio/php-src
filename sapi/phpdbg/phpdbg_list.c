/*
   +----------------------------------------------------------------------+
   | PHP Version 7                                                        |
   +----------------------------------------------------------------------+
   | Copyright (c) 1997-2016 The PHP Group                                |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.01 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available through the world-wide-web at the following url:           |
   | http://www.php.net/license/3_01.txt                                  |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Authors: Felipe Pena <felipe@php.net>                                |
   | Authors: Joe Watkins <joe.watkins@live.co.uk>                        |
   | Authors: Bob Weinand <bwoebi@php.net>                                |
   +----------------------------------------------------------------------+
*/

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#ifndef _WIN32
#	include <sys/mman.h>
#	include <unistd.h>
#endif
#include <fcntl.h>
#include "phpdbg.h"
#include "phpdbg_list.h"
#include "phpdbg_utils.h"
#include "phpdbg_prompt.h"
#include "php_streams.h"
#include "zend_exceptions.h"

ZEND_EXTERN_MODULE_GLOBALS(phpdbg);

#define PHPDBG_LIST_COMMAND_D(f, h, a, m, l, s, flags) \
	PHPDBG_COMMAND_D_EXP(f, h, a, m, l, s, &phpdbg_prompt_commands[12], flags)

const phpdbg_command_t phpdbg_list_commands[] = {
	PHPDBG_LIST_COMMAND_D(lines,     "lists the specified lines",    'l', list_lines,  NULL, "l", PHPDBG_ASYNC_SAFE),
	PHPDBG_LIST_COMMAND_D(class,     "lists the specified class",    'c', list_class,  NULL, "s", PHPDBG_ASYNC_SAFE),
	PHPDBG_LIST_COMMAND_D(method,    "lists the specified method",   'm', list_method, NULL, "m", PHPDBG_ASYNC_SAFE),
	PHPDBG_LIST_COMMAND_D(func,      "lists the specified function", 'f', list_func,   NULL, "s", PHPDBG_ASYNC_SAFE),
	PHPDBG_END_COMMAND
};

PHPDBG_LIST(lines) /* {{{ */
{
	if (!PHPDBG_G(exec) && !zend_is_executing()) {
		phpdbg_error("inactive", "type=\"execution\"", "Not executing, and execution context not set");
		return SUCCESS;
	}

	switch (param->type) {
		case NUMERIC_PARAM: {
			const char *char_file = phpdbg_current_file();
			zend_string *file = zend_string_init(char_file, strlen(char_file), 0);
			phpdbg_list_file(file, param->num < 0 ? 1 - param->num : param->num, (param->num < 0 ? param->num : 0) + zend_get_executed_lineno(), 0);
			efree(file);
		} break;

		case FILE_PARAM: {
			zend_string *file = zend_string_init(param->file.name, strlen(param->file.name), 0);
			phpdbg_list_file(file, param->file.line, 0, 0);
			efree(file);
		} break;

		phpdbg_default_switch_case();
	}

	return SUCCESS;
} /* }}} */

PHPDBG_LIST(func) /* {{{ */
{
	phpdbg_list_function_byname(param->str, param->len);

	return SUCCESS;
} /* }}} */

PHPDBG_LIST(method) /* {{{ */
{
	zend_class_entry *ce;

	if (phpdbg_safe_class_lookup(param->method.class, strlen(param->method.class), &ce) == SUCCESS) {
		zend_function *function;
		char *lcname = zend_str_tolower_dup(param->method.name, strlen(param->method.name));

		if ((function = zend_hash_str_find_ptr(&ce->function_table, lcname, strlen(lcname)))) {
			phpdbg_list_function(function);
		} else {
			phpdbg_error("list", "type=\"notfound\" method=\"%s::%s\"", "Could not find %s::%s", param->method.class, param->method.name);
		}

		efree(lcname);
	} else {
		phpdbg_error("list", "type=\"notfound\" class=\"%s\"", "Could not find the class %s", param->method.class);
	}

	return SUCCESS;
} /* }}} */

PHPDBG_LIST(class) /* {{{ */
{
	zend_class_entry *ce;

	if (phpdbg_safe_class_lookup(param->str, param->len, &ce) == SUCCESS) {
		if (ce->type == ZEND_USER_CLASS) {
			if (ce->info.user.filename) {
				phpdbg_list_file(ce->info.user.filename, ce->info.user.line_end - ce->info.user.line_start + 1, ce->info.user.line_start, 0);
			} else {
				phpdbg_error("list", "type=\"nosource\" class=\"%s\"", "The source of the requested class (%s) cannot be found", ZSTR_VAL(ce->name));
			}
		} else {
			phpdbg_error("list", "type=\"internalclass\" class=\"%s\"", "The class requested (%s) is not user defined", ZSTR_VAL(ce->name));
		}
	} else {
		phpdbg_error("list", "type=\"notfound\" class=\"%s\"", "The requested class (%s) could not be found", param->str);
	}

	return SUCCESS;
} /* }}} */

void phpdbg_list_file(zend_string *filename, uint count, int offset, uint highlight) /* {{{ */
{
	uint line, lastline;
	phpdbg_file_source *data;
	char resolved_path_buf[MAXPATHLEN];
	const char *abspath;

	if (VCWD_REALPATH(ZSTR_VAL(filename), resolved_path_buf)) {
		abspath = resolved_path_buf;
	} else {
		abspath = ZSTR_VAL(filename);
	}

	if (!(data = zend_hash_str_find_ptr(&PHPDBG_G(file_sources), abspath, strlen(abspath)))) {
		phpdbg_error("list", "type=\"unknownfile\"", "Could not find information about included file...");
		return;
	}

	if (offset < 0) {
		count += offset;
		offset = 0;
	}

	lastline = offset + count;

	if (lastline > data->lines) {
		lastline = data->lines;
	}

	phpdbg_xml("<list %r file=\"%s\">", ZSTR_VAL(filename));

	for (line = offset; line < lastline;) {
		uint linestart = data->line[line++];
		uint linelen = data->line[line] - linestart;
		char *buffer = data->buf + linestart;

		if (!highlight) {
			phpdbg_write("line", "line=\"%u\" code=\"%.*s\"", " %05u: %.*s", line, linelen, buffer);
		} else {
			if (highlight != line) {
				phpdbg_write("line", "line=\"%u\" code=\"%.*s\"", " %05u: %.*s", line, linelen, buffer);
			} else {
				phpdbg_write("line", "line=\"%u\" code=\"%.*s\" current=\"current\"", ">%05u: %.*s", line, linelen, buffer);
			}
		}

		if (*(buffer + linelen - 1) != '\n' || !linelen) {
			phpdbg_out("\n");
		}
	}

	phpdbg_xml("</list>");
} /* }}} */

void phpdbg_list_function(const zend_function *fbc) /* {{{ */
{
	const zend_op_array *ops;

	if (fbc->type != ZEND_USER_FUNCTION) {
		phpdbg_error("list", "type=\"internalfunction\" function=\"%s\"", "The function requested (%s) is not user defined", ZSTR_VAL(fbc->common.function_name));
		return;
	}

	ops = (zend_op_array *) fbc;

	phpdbg_list_file(ops->filename, ops->line_end - ops->line_start + 1, ops->line_start, 0);
} /* }}} */

void phpdbg_list_function_byname(const char *str, size_t len) /* {{{ */
{
	HashTable *func_table = EG(function_table);
	zend_function* fbc;
	char *func_name = (char*) str;
	size_t func_name_len = len;

	/* search active scope if begins with period */
	if (func_name[0] == '.') {
		if (EG(scope)) {
			func_name++;
			func_name_len--;

			func_table = &EG(scope)->function_table;
		} else {
			phpdbg_error("inactive", "type=\"noclasses\"", "No active class");
			return;
		}
	} else if (!EG(function_table)) {
		phpdbg_error("inactive", "type=\"function_table\"", "No function table loaded");
		return;
	} else {
		func_table = EG(function_table);
	}

	/* use lowercase names, case insensitive */
	func_name = zend_str_tolower_dup(func_name, func_name_len);

	phpdbg_try_access {
		if ((fbc = zend_hash_str_find_ptr(func_table, func_name, func_name_len))) {
			phpdbg_list_function(fbc);
		} else {
			phpdbg_error("list", "type=\"nofunction\" function=\"%s\"", "Function %s not found", func_name);
		}
	} phpdbg_catch_access {
		phpdbg_error("signalsegv", "function=\"%s\"", "Could not list function %s, invalid data source", func_name);
	} phpdbg_end_try_access();

	efree(func_name);
} /* }}} */

zend_op_array *phpdbg_compile_file(zend_file_handle *file, int type) {
	phpdbg_file_source data, *dataptr;
	zend_file_handle fake;
	zend_op_array *ret;
	char *filename = (char *)(file->opened_path ? ZSTR_VAL(file->opened_path) : file->filename);
	uint line;
	char *bufptr, *endptr;
	char resolved_path_buf[MAXPATHLEN];

	if (zend_stream_fixup(file, &bufptr, &data.len) == FAILURE) {
		return NULL;
	}

	data.buf = emalloc(data.len + ZEND_MMAP_AHEAD + 1);
	if (data.len > 0) {
		memcpy(data.buf, bufptr, data.len);
	}
	memset(data.buf + data.len, 0, ZEND_MMAP_AHEAD + 1);
	data.filename = filename;
	data.line[0] = 0;

	memset(&fake, 0, sizeof(fake));
	fake.type = ZEND_HANDLE_MAPPED;
	fake.handle.stream.mmap.buf = data.buf;
	fake.handle.stream.mmap.len = data.len;
	fake.free_filename = 0;
	fake.filename = filename;
	fake.opened_path = file->opened_path;

	*(dataptr = emalloc(sizeof(phpdbg_file_source) + sizeof(uint) * data.len)) = data;
	if (VCWD_REALPATH(filename, resolved_path_buf)) {
		filename = resolved_path_buf;
	}

	for (line = 0, bufptr = data.buf - 1, endptr = data.buf + data.len; ++bufptr < endptr;) {
		if (*bufptr == '\n') {
			dataptr->line[++line] = (uint)(bufptr - data.buf) + 1;
		}
	}
	dataptr->lines = ++line;
	dataptr->line[line] = endptr - data.buf;

	ret = PHPDBG_G(compile_file)(&fake, type);

	if (ret == NULL) {
		efree(data.buf);
		efree(dataptr);
		return NULL;
	}

	dataptr = erealloc(dataptr, sizeof(phpdbg_file_source) + sizeof(uint) * line);
	zend_hash_str_add_ptr(&PHPDBG_G(file_sources), filename, strlen(filename), dataptr);
	phpdbg_resolve_pending_file_break(filename);

	fake.opened_path = NULL;
	zend_file_handle_dtor(&fake);

	return ret;
}

zend_op_array *phpdbg_init_compile_file(zend_file_handle *file, int type) {
	char *filename = (char *)(file->opened_path ? ZSTR_VAL(file->opened_path) : file->filename);
	char resolved_path_buf[MAXPATHLEN];
	zend_op_array *op_array;
	phpdbg_file_source *dataptr;

	if (VCWD_REALPATH(filename, resolved_path_buf)) {
		filename = resolved_path_buf;
	}

	op_array = PHPDBG_G(init_compile_file)(file, type);

	if (op_array == NULL) {
		return NULL;
	}

	dataptr = zend_hash_str_find_ptr(&PHPDBG_G(file_sources), filename, strlen(filename));
	ZEND_ASSERT(dataptr != NULL);

	dataptr->op_array = *op_array;
	if (dataptr->op_array.refcount) {
		++*dataptr->op_array.refcount;
	}

	return op_array;
}

zend_op_array *phpdbg_compile_string(zval *source_string, char *filename) {
	zend_string *fake_name;
	zend_op_array *op_array;
	phpdbg_file_source *dataptr;
	uint line;
	char *bufptr, *endptr;

	if (PHPDBG_G(flags) & PHPDBG_IN_EVAL) {
		return PHPDBG_G(compile_string)(source_string, filename);
	}

	dataptr = emalloc(sizeof(phpdbg_file_source) + sizeof(uint) * Z_STRLEN_P(source_string));
	dataptr->buf = estrndup(Z_STRVAL_P(source_string), Z_STRLEN_P(source_string));
	dataptr->len = Z_STRLEN_P(source_string);
	dataptr->line[0] = 0;
	for (line = 0, bufptr = dataptr->buf - 1, endptr = dataptr->buf + dataptr->len; ++bufptr < endptr;) {
		if (*bufptr == '\n') {
			dataptr->line[++line] = (uint)(bufptr - dataptr->buf) + 1;
		}
	}
	dataptr->lines = ++line;
	dataptr->line[line] = endptr - dataptr->buf;

	op_array = PHPDBG_G(compile_string)(source_string, filename);

	if (op_array == NULL) {
		efree(dataptr->buf);
		efree(dataptr);
		return NULL;
	}

	fake_name = strpprintf(0, "%s\0%p", filename, op_array->opcodes);

	dataptr = erealloc(dataptr, sizeof(phpdbg_file_source) + sizeof(uint) * line);
	zend_hash_add_ptr(&PHPDBG_G(file_sources), fake_name, dataptr);

	dataptr->filename = estrndup(ZSTR_VAL(fake_name), ZSTR_LEN(fake_name));
	zend_string_release(fake_name);

	dataptr->op_array = *op_array;
	if (dataptr->op_array.refcount) {
		++*dataptr->op_array.refcount;
	}

	return op_array;
}

void phpdbg_free_file_source(zval *zv) {
	phpdbg_file_source *data = Z_PTR_P(zv);

	if (data->buf) {
		efree(data->buf);
	}

	destroy_op_array(&data->op_array);

	efree(data);
}

void phpdbg_init_list(void) {
	PHPDBG_G(compile_file) = zend_compile_file;
	PHPDBG_G(compile_string) = zend_compile_string;
	zend_hash_init(&PHPDBG_G(file_sources), 1, NULL, (dtor_func_t) phpdbg_free_file_source, 0);
	zend_compile_file = phpdbg_compile_file;
	zend_compile_string = phpdbg_compile_string;
}

void phpdbg_list_update(void) {
	PHPDBG_G(init_compile_file) = zend_compile_file;
	zend_compile_file = phpdbg_init_compile_file;
}
