/*
  +----------------------------------------------------------------------+
  | PHP Version 5                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2007 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author:  Liexusong <280259971@qq.com>                                |
  +----------------------------------------------------------------------+
*/

/* $Id: header,v 1.16.2.1.2.1 2007/01/01 19:32:09 iliaa Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "php_ukg.h"

#define UKG_EXT_VERSION "0.1"

/* If you declare any globals in php_ukg.h uncomment this:
ZEND_DECLARE_MODULE_GLOBALS(ukg)
*/

/* True global resources - no need for thread safety here */
static int le_ukg;

/* {{{ ukg_functions[]
 *
 * Every user visible function must have an entry in ukg_functions[].
 */
zend_function_entry ukg_functions[] = {
    PHP_FE(ukg_getkey,    NULL)
    PHP_FE(ukg_key2info,  NULL)
    {NULL, NULL, NULL}    /* Must be the last line in ukg_functions[] */
};
/* }}} */

/* {{{ ukg_module_entry
 */
zend_module_entry ukg_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
    STANDARD_MODULE_HEADER,
#endif
    "ukg",
    ukg_functions,
    PHP_MINIT(ukg),
    PHP_MSHUTDOWN(ukg),
    NULL,
    NULL,
    PHP_MINFO(ukg),
#if ZEND_MODULE_API_NO >= 20010901
    UKG_EXT_VERSION, /* Replace with version number for your extension */
#endif
    STANDARD_MODULE_PROPERTIES
};
/* }}} */

#ifdef COMPILE_DL_UKG
ZEND_GET_MODULE(ukg)
#endif


/* {{{ PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(ukg)
{
    /* If you have INI entries, uncomment these lines 
    REGISTER_INI_ENTRIES();
    */
    return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION
 */
PHP_MSHUTDOWN_FUNCTION(ukg)
{
    /* uncomment this line if you have INI entries
    UNREGISTER_INI_ENTRIES();
    */
    return SUCCESS;
}
/* }}} */

/* Remove if there's nothing to do at request start */
/* {{{ PHP_RINIT_FUNCTION
 */
PHP_RINIT_FUNCTION(ukg)
{
    return SUCCESS;
}
/* }}} */

/* Remove if there's nothing to do at request end */
/* {{{ PHP_RSHUTDOWN_FUNCTION
 */
PHP_RSHUTDOWN_FUNCTION(ukg)
{
    return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION
 */
PHP_MINFO_FUNCTION(ukg)
{
    php_info_print_table_start();
    php_info_print_table_header(2, "ukg support", "enabled");
    php_info_print_table_row(2, "author", "Liexusong");
    php_info_print_table_row(2, "version", UKG_EXT_VERSION);
    php_info_print_table_end();
}
/* }}} */


PHP_FUNCTION(ukg_getkey)
{
    php_stream *stream = NULL;
    struct timeval tv;
    char *host, real_host[128];
    int host_len, real_host_len;
    int port;
    char *errstr = NULL;
    int errno;
    char *recv_key;
    int recv_len;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sl", &host, &host_len,
                                                            &port) == FAILURE) {
        RETURN_FALSE;
    }

    real_host_len = sprintf(real_host, "%s:%d", host, port);
    if (real_host_len <= 0) {
        RETURN_FALSE;
    }

    tv.tv_sec = 1;
    tv.tv_usec = 0;

    stream = php_stream_xport_create(real_host, real_host_len,
      ENFORCE_SAFE_MODE|REPORT_ERRORS, STREAM_XPORT_CLIENT|STREAM_XPORT_CONNECT,
      NULL, &tv, NULL, &errstr, &errno);

    if (stream == NULL) {
        php_error_docref(NULL TSRMLS_CC, E_WARNING, "Unable connect to %s (%s)",
                          real_host, errstr == NULL ? "Unknown error" : errstr);
        RETURN_FALSE;
    }

    recv_key = php_stream_get_line(stream, NULL, 0, &recv_len);
    if (recv_key == NULL) {
        php_error_docref(NULL TSRMLS_CC, E_WARNING, "Unable get key from server");
        php_stream_close(stream);
        RETURN_FALSE;
    }

    php_stream_close(stream);
    RETURN_STRINGL(recv_key, recv_len, 0);
}


#define TWEPOCH                (1288834974657ULL)
#define WORKER_ID_SHIFT        (12)
#define DATACENTER_ID_SHIFT    (12 + 5)
#define TIMESTAMP_LEFT_SHIFT   (12 + 5 + 5)
#define SEQUENCE_MASK          (-1 ^ (-1 << 12))


static unsigned long long ukg_key2uint64(char *key)
{
    unsigned long long retval;

    if (sscanf(key, "%llu", &retval) == 0)
        return 0;
    return retval;
}


PHP_FUNCTION(ukg_key2info)
{
    unsigned long long id;
    int datacenter, worker;
    char *key;
    int len;
    int timestamp;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &key,
                                                   &len TSRMLS_CC) == FAILURE) {
        RETURN_FALSE;
    }

    id = ukg_key2uint64(key);
    if (!id) {
        RETURN_FALSE;
    }

    worker = (id >> WORKER_ID_SHIFT) & 0x1FULL;
    datacenter = (id >> DATACENTER_ID_SHIFT) & 0x1FULL;
    timestamp = ((id >> TIMESTAMP_LEFT_SHIFT) + TWEPOCH) / 1000ULL;

    array_init(return_value);
    add_assoc_long(return_value, "worker", worker);
    add_assoc_long(return_value, "datacenter", datacenter);
    add_assoc_long(return_value, "timestamp", timestamp);

    return;
}


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
