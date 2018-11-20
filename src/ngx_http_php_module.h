/**
 *    Copyright(c) 2016-2018 rryqszq4
 *
 *
 */

#ifndef NGX_HTTP_PHP_MODULE_H
#define NGX_HTTP_PHP_MODULE_H

#include <ngx_core.h>
#include <ngx_http.h>
#include <ngx_config.h>
#include <nginx.h>

#if defined(NDK) && NDK
#include <ndk.h>
#endif

#include "ngx_http_php_core.h"

#define NGX_HTTP_PHP_MODULE_NAME "ngx_php7"
#define NGX_HTTP_PHP_MODULE_VERSION  "0.0.11"

extern ngx_module_t ngx_http_php_module;
ngx_http_request_t *ngx_php_request;

typedef struct {

    ngx_str_t ini_path;
    ngx_http_php_code_t *init_code;
    ngx_http_php_code_t *init_inline_code;

    unsigned enabled_rewrite_handler:1;
    unsigned enabled_access_handler:1;
    unsigned enabled_content_handler:1;
    unsigned enabled_content_async_handler:1;
    unsigned enabled_log_handler:1;

    unsigned enabled_opcode_handler:1;
    unsigned enabled_stack_handler:1;

    ngx_http_php_state_t *state;

} ngx_http_php_main_conf_t;

typedef struct {
    ngx_str_t document_root;

    ngx_http_php_code_t *rewrite_code;
    ngx_http_php_code_t *rewrite_inline_code;
    ngx_http_php_code_t *access_code;
    ngx_http_php_code_t *access_inline_code;
    ngx_http_php_code_t *content_code;
    ngx_http_php_code_t *content_inline_code;
    ngx_http_php_code_t *content_async_inline_code;
    ngx_http_php_code_t *log_code;
    ngx_http_php_code_t *log_inline_code;

    ngx_http_php_code_t *opcode_inline_code;
    ngx_http_php_code_t *stack_inline_code;

    ngx_int_t (*rewrite_handler)(ngx_http_request_t *r);
    ngx_int_t (*access_handler)(ngx_http_request_t *r);
    ngx_int_t (*content_handler)(ngx_http_request_t *r);
    ngx_int_t (*content_async_handler)(ngx_http_request_t *r);
    ngx_int_t (*log_handler)(ngx_http_request_t *r);

    ngx_int_t (*opcode_handler)(ngx_http_request_t *r);
    ngx_int_t (*stack_handler)(ngx_http_request_t *r);

    unsigned enabled_rewrite_inline_compile:1;
    unsigned enabled_access_inline_compile:1;
    unsigned enabled_content_inline_compile:1;
    
} ngx_http_php_loc_conf_t;


#endif