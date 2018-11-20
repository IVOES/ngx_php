/**
 *    Copyright(c) 2016-2017 rryqszq4
 *
 *
 */

#include "ngx_http_php_core.h"
#include "ngx_http_php_handler.h"
#include "ngx_http_php_module.h"
#include "ngx_http_php_request.h"
//#include "ngx_http_php_subrequest.h"

//#include "php/php_ngx_location.h"

#include "php/impl/php_ngx.h"
#include "php/impl/php_ngx_request.h"

static void ngx_http_php_rewrite_inline_uthread_routine(void *data);
static void ngx_http_php_rewrite_file_uthread_routine(void *data);

static void ngx_http_php_access_inline_uthread_routine(void *data);
static void ngx_http_php_access_file_uthread_routine(void *data);

static void ngx_http_php_content_inline_uthread_routine(void *data);
static void ngx_http_php_content_file_uthread_routine(void *data);

ngx_int_t
ngx_http_php_post_read_handler(ngx_http_request_t *r)
{
    ngx_http_php_ctx_t *ctx;
    
    ngx_php_request = r;
    ctx = ngx_http_get_module_ctx(r, ngx_http_php_module);
    if (ctx == NULL) {
        ctx = ngx_pcalloc(r->pool, sizeof(*ctx));
        if (ctx == NULL) {
            return NGX_ERROR;
        }
        ctx->rewrite_phase = 0;
        ctx->access_phase = 0;
        ctx->content_phase = 0;
        ctx->phase_status = NGX_DECLINED;
        ngx_memzero(&ctx->sleep, sizeof(ngx_event_t));
    }

    ngx_http_set_ctx(r, ctx, ngx_http_php_module);

    return NGX_OK;
}

void
ngx_http_php_request_cleanup_handler(void *data)
{
    return ;
}

static void
ngx_http_php_rewrite_file_uthread_routine(void *data)
{
    ngx_http_request_t *r;
    ngx_http_php_ctx_t *ctx;
    ngx_http_php_main_conf_t *pmcf;
    ngx_http_php_loc_conf_t *plcf;

    r = data;
    pmcf = ngx_http_get_module_main_conf(r, ngx_http_php_module);
    plcf = ngx_http_get_module_loc_conf(r, ngx_http_php_module);
    ctx = ngx_http_get_module_ctx(r, ngx_http_php_module);

    ctx->rewrite_phase = 1;
    ctx->phase_status = NGX_OK;

    ngx_php_request = r;

    ngx_php_set_request_status(NGX_DECLINED);
    zend_first_try {

        ngx_php_eval_file(r, pmcf->state, plcf->rewrite_code);
        
    }zend_end_try();
}

static void
ngx_http_php_rewrite_inline_uthread_routine(void *data)
{
    ngx_http_request_t *r;
    ngx_http_php_ctx_t *ctx;
    ngx_http_php_main_conf_t *pmcf;
    ngx_http_php_loc_conf_t *plcf;

    r = data;
    pmcf = ngx_http_get_module_main_conf(r, ngx_http_php_module);
    plcf = ngx_http_get_module_loc_conf(r, ngx_http_php_module);
    ctx = ngx_http_get_module_ctx(r, ngx_http_php_module);

    ctx->rewrite_phase = 1;
    ctx->phase_status = NGX_OK;

    ngx_php_request = r;

    ngx_php_set_request_status(NGX_DECLINED);
    zend_first_try {

        ngx_php_eval_code(r, pmcf->state, plcf->rewrite_inline_code);
    
    }zend_end_try();
}

ngx_int_t 
ngx_http_php_rewrite_handler(ngx_http_request_t *r)
{
    ngx_http_php_loc_conf_t *plcf;
    plcf = ngx_http_get_module_loc_conf(r, ngx_http_php_module);
    if (plcf->rewrite_handler == NULL){
        return NGX_DECLINED;
    }
    return plcf->rewrite_handler(r);
}

ngx_int_t 
ngx_http_php_rewrite_file_handler(ngx_http_request_t *r)
{
    ngx_int_t rc;
    ngx_http_php_rputs_chain_list_t *chain;
    ngx_http_php_ctx_t *ctx;

    ctx = ngx_http_get_module_ctx(r, ngx_http_php_module);

    if ( ctx == NULL ) {
        ctx = ngx_pcalloc(r->pool, sizeof(*ctx));
        if ( ctx == NULL ) {
            return NGX_ERROR;
        }
    }
    ctx->output_type = OUTPUT_CONTENT;
    ngx_http_set_ctx(r, ctx, ngx_http_php_module);

    ngx_php_request = r;

    if ( ctx->phase_status == NGX_DECLINED ) {
        ngx_http_php_rewrite_file_uthread_routine(r);

        if ( ctx->phase_status == NGX_AGAIN ) {
            
            r->main->count++;
            return NGX_AGAIN;
        
        } else {

            goto set_output;

        }
    }else {

        if ( ctx->phase_status == NGX_AGAIN ) {
            
            r->main->count++;
            return NGX_AGAIN;

        } else {

            goto set_output;

        }

    }

set_output:
    rc = ngx_php_get_request_status();

    ctx = ngx_http_get_module_ctx(r, ngx_http_php_module);

    if ( ctx->generator_closure ) {
        zval_ptr_dtor(ctx->generator_closure);
    }

    ctx->phase_status = NGX_DECLINED;

    if ( rc == NGX_OK || rc == NGX_HTTP_OK ) {

        chain = ctx->rputs_chain;

        if ( ctx->rputs_chain == NULL ) {
            return NGX_DECLINED;
        }

        if ( !r->headers_out.status ) {
            r->headers_out.status = NGX_HTTP_OK;
        }

        if ( r->method == NGX_HTTP_HEAD ) {
            rc = ngx_http_send_header(r);
            if ( rc != NGX_OK ) {
                return rc;
            }
        }

        if ( chain != NULL ) {
            (*chain->last)->buf->last_buf = 1;
        }

        rc = ngx_http_send_header(r);
        if ( rc != NGX_OK ) {
            return rc;
        }

        ngx_http_output_filter(r, chain->out);

        ngx_http_set_ctx(r, NULL, ngx_http_php_module);

        return NGX_OK;

    }

    if ( rc == NGX_ERROR || rc > NGX_OK ) {
        return rc;
    }

    return rc;

}

ngx_int_t 
ngx_http_php_rewrite_inline_handler(ngx_http_request_t *r)
{
    ngx_int_t rc;
    ngx_http_php_rputs_chain_list_t *chain;
    ngx_http_php_ctx_t *ctx;

    ctx = ngx_http_get_module_ctx(r, ngx_http_php_module);

    if ( ctx == NULL ) {
        ctx = ngx_pcalloc(r->pool, sizeof(*ctx));
        if ( ctx == NULL ) {
            return NGX_ERROR;
        }
    }
    ctx->output_type = OUTPUT_CONTENT;
    ngx_http_set_ctx(r, ctx, ngx_http_php_module);

    ngx_php_request = r;

    if ( ctx->phase_status == NGX_DECLINED ) {
        ngx_http_php_rewrite_inline_uthread_routine(r);
    
        if ( ctx->phase_status == NGX_AGAIN ) {

            r->main->count++;
            return NGX_AGAIN;
        
        } else {

            goto set_output;

        } 
    } else {

        if ( ctx->phase_status == NGX_AGAIN ) {
        
            r->main->count++;
            return NGX_AGAIN;
        
        } else {

            goto set_output;

        }
    }

set_output:
    rc = ngx_php_get_request_status();

    ctx = ngx_http_get_module_ctx(r, ngx_http_php_module);

    if ( ctx->generator_closure ) {
        zval_ptr_dtor(ctx->generator_closure);
    }

    ctx->phase_status = NGX_DECLINED;

    if ( rc == NGX_OK || rc == NGX_HTTP_OK ) {
        chain = ctx->rputs_chain;

        if ( ctx->rputs_chain == NULL ) {
            return NGX_DECLINED;
        }

        if ( !r->headers_out.status ) {
            r->headers_out.status = NGX_HTTP_OK;
        }

        if ( r->method == NGX_HTTP_HEAD ) {
            rc = ngx_http_send_header(r);
            if ( rc != NGX_OK ) {
                return rc;
            }
        }

        if ( chain != NULL ) {
            (*chain->last)->buf->last_buf = 1;
        }

        rc = ngx_http_send_header(r);
        if ( rc != NGX_OK ) {
            return rc;
        }

        ngx_http_output_filter(r, chain->out);

        ngx_http_set_ctx(r, NULL, ngx_http_php_module);

        return NGX_OK;

    }

    if ( rc == NGX_ERROR || rc > NGX_OK ) {
        return rc;
    }

    return rc;

}

static void
ngx_http_php_access_file_uthread_routine(void *data)
{
    ngx_http_request_t *r;
    ngx_http_php_ctx_t *ctx;
    ngx_http_php_main_conf_t *pmcf;
    ngx_http_php_loc_conf_t *plcf;

    r = data;
    pmcf = ngx_http_get_module_main_conf(r, ngx_http_php_module);
    plcf = ngx_http_get_module_loc_conf(r, ngx_http_php_module);
    ctx = ngx_http_get_module_ctx(r, ngx_http_php_module);

    ctx->access_phase = 1;
    ctx->phase_status = NGX_OK;
    ngx_php_request = r;

    ngx_php_set_request_status(NGX_DECLINED);
    zend_first_try {

        ngx_php_eval_file(r, pmcf->state, plcf->access_code);

    }zend_end_try();
}

static void
ngx_http_php_access_inline_uthread_routine(void *data)
{
    ngx_http_request_t *r;
    ngx_http_php_ctx_t *ctx;
    ngx_http_php_main_conf_t *pmcf;
    ngx_http_php_loc_conf_t *plcf;

    r = data;
    pmcf = ngx_http_get_module_main_conf(r, ngx_http_php_module);
    plcf = ngx_http_get_module_loc_conf(r, ngx_http_php_module);
    ctx = ngx_http_get_module_ctx(r, ngx_http_php_module);

    ctx->access_phase = 1;
    ctx->phase_status = NGX_OK;
    ngx_php_request = r;

    ngx_php_set_request_status(NGX_DECLINED);
    zend_first_try {

        ngx_php_eval_code(r, pmcf->state, plcf->access_inline_code);

    }zend_end_try();
}

ngx_int_t 
ngx_http_php_access_handler(ngx_http_request_t *r)
{
    ngx_http_php_loc_conf_t *plcf;
    plcf = ngx_http_get_module_loc_conf(r, ngx_http_php_module);
    if (plcf->access_handler == NULL){
        return NGX_DECLINED;
    }
    return plcf->access_handler(r);
}

ngx_int_t 
ngx_http_php_access_file_handler(ngx_http_request_t *r)
{
    ngx_int_t rc;
    ngx_http_php_rputs_chain_list_t *chain;
    ngx_http_php_ctx_t *ctx;

    ctx = ngx_http_get_module_ctx(r, ngx_http_php_module);

    if (ctx == NULL) {
        ctx = ngx_pcalloc(r->pool, sizeof(*ctx));
        if (ctx == NULL) {
            return NGX_ERROR;
        }
    }
    ctx->output_type = OUTPUT_CONTENT;
    ngx_http_set_ctx(r, ctx, ngx_http_php_module);

    ngx_php_request = r;

    if (ctx->phase_status == NGX_DECLINED) {

        ngx_http_php_access_file_uthread_routine(r);

        if (ctx->phase_status == NGX_AGAIN) {

            r->main->count++;
            return NGX_AGAIN;

        }else {

            goto set_output;

        }

    }else {

        if (ctx->phase_status == NGX_AGAIN) {
            
            r->main->count++;
            return NGX_AGAIN;

        }else {

            goto set_output;

        }

    }

set_output:
    rc = ngx_php_get_request_status();

    ctx = ngx_http_get_module_ctx(r, ngx_http_php_module);

    if (ctx->generator_closure) {
        zval_ptr_dtor(ctx->generator_closure);
    }

    ctx->phase_status = NGX_DECLINED;

    if (rc == NGX_OK || rc == NGX_HTTP_OK) {

        chain = ctx->rputs_chain;

        if (ctx->rputs_chain == NULL) {
            return NGX_DECLINED;
        }

        if (!r->headers_out.status) {
            r->headers_out.status = NGX_HTTP_OK;
        }

        if (r->method == NGX_HTTP_HEAD) {
            rc = ngx_http_send_header(r);
            if (rc != NGX_OK) {
                return rc;
            }
        }

        if (chain != NULL) {
            (*chain->last)->buf->last_buf = 1;
        }

        rc = ngx_http_send_header(r);
        if (rc != NGX_OK) {
            return rc;
        }

        ngx_http_output_filter(r, chain->out);

        ngx_http_set_ctx(r, NULL, ngx_http_php_module);

        return NGX_HTTP_OK;

    }

    if (rc == NGX_ERROR || rc > NGX_OK) {

        return rc;

    }

    return rc;

}

ngx_int_t 
ngx_http_php_access_inline_handler(ngx_http_request_t *r)
{
    ngx_int_t rc;
    ngx_http_php_rputs_chain_list_t *chain;
    ngx_http_php_ctx_t *ctx;

    ctx = ngx_http_get_module_ctx(r, ngx_http_php_module);

    if (ctx == NULL) {
        ctx = ngx_pcalloc(r->pool, sizeof(*ctx));
        if (ctx == NULL) {
            return NGX_ERROR;
        }
    }
    ctx->output_type = OUTPUT_CONTENT;
    ngx_http_set_ctx(r, ctx, ngx_http_php_module);

    ngx_php_request = r;

    if (ctx->phase_status == NGX_DECLINED) {

        ngx_http_php_access_inline_uthread_routine(r);

        if (ctx->phase_status == NGX_AGAIN) {

            r->main->count++;
            return NGX_AGAIN;

        }else {

            goto set_output;

        }

    }else {

        if (ctx->phase_status == NGX_AGAIN) {

            r->main->count++;
            return NGX_AGAIN;

        }else {

            goto set_output;

        }

    }

set_output:
    rc = ngx_php_get_request_status();

    ctx = ngx_http_get_module_ctx(r, ngx_http_php_module);

    if (ctx->generator_closure) {
        zval_ptr_dtor(ctx->generator_closure);
    }

    ctx->phase_status = NGX_DECLINED;

    if (rc == NGX_OK || rc == NGX_HTTP_OK) {

        chain = ctx->rputs_chain;

        if (ctx->rputs_chain == NULL) {
            return NGX_DECLINED;
        }

        if (!r->headers_out.status) {
            r->headers_out.status = NGX_HTTP_OK;
        }

        if (r->method == NGX_HTTP_HEAD) {
            rc = ngx_http_send_header(r);
            if (rc != NGX_OK) {
                return rc;
            }
        }

        if (chain != NULL) {
            (*chain->last)->buf->last_buf = 1;
        }

        rc = ngx_http_send_header(r);
        if (rc != NGX_OK) {
            return rc;
        }

        ngx_http_output_filter(r, chain->out);

        ngx_http_set_ctx(r, NULL, ngx_http_php_module);

        return NGX_HTTP_OK;

    }

    if (rc == NGX_ERROR || rc > NGX_OK) {
        return rc;
    }

    return rc;

}

static void
ngx_http_php_content_file_uthread_routine(void *data)
{
    ngx_http_request_t *r;
    ngx_http_php_ctx_t *ctx;
    ngx_http_php_main_conf_t *pmcf;
    ngx_http_php_loc_conf_t *plcf;

    r = data;
    pmcf = ngx_http_get_module_main_conf(r, ngx_http_php_module);
    plcf = ngx_http_get_module_loc_conf(r, ngx_http_php_module);
    ctx = ngx_http_get_module_ctx(r, ngx_http_php_module);

    ctx->content_phase = 1;
    ctx->phase_status = NGX_OK;
    ngx_php_request = r;

    ngx_php_set_request_status(NGX_OK);
    zend_first_try {

        ngx_php_eval_file(r, pmcf->state, plcf->content_code);

    }zend_end_try();
}

static void
ngx_http_php_content_inline_uthread_routine(void *data)
{
    ngx_http_request_t *r;
    ngx_http_php_ctx_t *ctx;
    ngx_http_php_main_conf_t *pmcf;
    ngx_http_php_loc_conf_t *plcf;

    r = data;
    pmcf = ngx_http_get_module_main_conf(r, ngx_http_php_module);
    plcf = ngx_http_get_module_loc_conf(r, ngx_http_php_module);
    ctx = ngx_http_get_module_ctx(r, ngx_http_php_module);

    ctx->content_phase = 1;
    ctx->phase_status = NGX_OK;
    ngx_php_request = r;

    ngx_php_set_request_status(NGX_OK);
    zend_first_try {

        ngx_php_eval_code(r, pmcf->state, plcf->content_inline_code);

    }zend_end_try();
}

ngx_int_t
ngx_http_php_content_handler(ngx_http_request_t *r)
{
    ngx_http_php_loc_conf_t *plcf;
    plcf = ngx_http_get_module_loc_conf(r, ngx_http_php_module);
    if (plcf->content_handler == NULL){
        return NGX_DECLINED;
    }
    return plcf->content_handler(r);
}

ngx_int_t 
ngx_http_php_content_file_handler(ngx_http_request_t *r)
{
    ngx_int_t rc;
    ngx_http_php_rputs_chain_list_t *chain;
    ngx_http_php_ctx_t *ctx;

    ctx = ngx_http_get_module_ctx(r, ngx_http_php_module);

    if (ctx == NULL) {
        ctx = ngx_pcalloc(r->pool, sizeof(*ctx));
        if (ctx == NULL) {
            return NGX_ERROR;
        }
    }

    ctx->output_type = OUTPUT_CONTENT;

    ctx->request_body_more = 1;
    ngx_http_set_ctx(r, ctx, ngx_http_php_module);

    ngx_php_request = r;

    ngx_php_set_request_status(NGX_OK);

    if (r->method == NGX_HTTP_POST) {
        return ngx_http_php_content_post_handler(r);
    }

    if (ctx->phase_status == NGX_DECLINED) {

        ngx_http_php_content_file_uthread_routine(r);

        if (ctx->phase_status == NGX_AGAIN) {

            r->main->count++;
            return NGX_AGAIN;

        }else {

            goto set_output;

        }

    }else {

        if (ctx->phase_status == NGX_AGAIN) {

            r->main->count++;
            return NGX_AGAIN;

        }else {

            goto set_output;

        }

    }

set_output:
    rc = ngx_php_get_request_status();

    ctx = ngx_http_get_module_ctx(r, ngx_http_php_module);

    if (ctx->generator_closure) {
        zval_ptr_dtor(ctx->generator_closure);
    }

    ctx->phase_status = NGX_DECLINED;

    if (rc == NGX_OK || rc == NGX_DECLINED) {

        chain = ctx->rputs_chain;

        if (ctx->rputs_chain == NULL){
            ngx_buf_t *b;
            ngx_str_t ns;
            u_char *u_str;
            ns.data = (u_char *)" ";
            ns.len = 1;
            
            chain = ngx_pcalloc(r->pool, sizeof(ngx_http_php_rputs_chain_list_t));
            chain->out = ngx_alloc_chain_link(r->pool);
            chain->last = &chain->out;
        
            b = ngx_calloc_buf(r->pool);
            (*chain->last)->buf = b;
            (*chain->last)->next = NULL;

            u_str = ngx_pstrdup(r->pool, &ns);
            //u_str[ns.len] = '\0';
            (*chain->last)->buf->pos = u_str;
            (*chain->last)->buf->last = u_str + ns.len;
            (*chain->last)->buf->memory = 1;
            ctx->rputs_chain = chain;

            if (r->headers_out.content_length_n == -1){
                r->headers_out.content_length_n += ns.len + 1;
            }else {
                r->headers_out.content_length_n += ns.len;
            }
        }

        if (!r->headers_out.status){
            r->headers_out.status = NGX_HTTP_OK;
        }

        if (r->method == NGX_HTTP_HEAD){
            rc = ngx_http_send_header(r);
            if (rc != NGX_OK){
                return rc;
            }
        }

        if (chain != NULL){
            (*chain->last)->buf->last_buf = 1;
        }

        rc = ngx_http_send_header(r);
        if (rc != NGX_OK){
            return rc;
        }
        
        ngx_http_output_filter(r, chain->out);

        ngx_http_set_ctx(r, NULL, ngx_http_php_module);

        return NGX_OK;

    }

    if (rc == NGX_ERROR || rc > NGX_OK) {

        return rc;

    }

    return NGX_OK;
}

ngx_int_t 
ngx_http_php_content_inline_handler(ngx_http_request_t *r)
{
    ngx_int_t rc;
    ngx_http_php_rputs_chain_list_t *chain;
    ngx_http_php_ctx_t *ctx;

    ctx = ngx_http_get_module_ctx(r, ngx_http_php_module);

    if (ctx == NULL) {
        ctx = ngx_pcalloc(r->pool, sizeof(*ctx));
        if (ctx == NULL) {
            return NGX_ERROR;
        }
    }

    ctx->output_type = OUTPUT_CONTENT;

    ctx->request_body_more = 1;
    ngx_http_set_ctx(r, ctx, ngx_http_php_module);

    ngx_php_request = r;

    ngx_php_set_request_status(NGX_OK);

    if (r->method == NGX_HTTP_POST) {
        return ngx_http_php_content_post_handler(r);
    }

    if (ctx->phase_status == NGX_DECLINED) {

        ngx_http_php_content_inline_uthread_routine(r);

        if (ctx->phase_status == NGX_AGAIN) {

            r->main->count++;
            return NGX_AGAIN;

        }else {

            goto set_output;

        }

    }else {

        if (ctx->phase_status == NGX_AGAIN) {

            r->main->count++;
            return NGX_AGAIN;

        }else {

            goto set_output;

        }

    }

set_output:
    rc = ngx_php_get_request_status();

    ctx = ngx_http_get_module_ctx(r, ngx_http_php_module);

    if (ctx->generator_closure) {
        zval_ptr_dtor(ctx->generator_closure);
    }

    ctx->phase_status = NGX_DECLINED;

    if (rc == NGX_OK || rc == NGX_DECLINED) {

        chain = ctx->rputs_chain;

        if (ctx->rputs_chain == NULL){
            ngx_buf_t *b;
            ngx_str_t ns;
            u_char *u_str;
            ns.data = (u_char *)" ";
            ns.len = 1;
            
            chain = ngx_pcalloc(r->pool, sizeof(ngx_http_php_rputs_chain_list_t));
            chain->out = ngx_alloc_chain_link(r->pool);
            chain->last = &chain->out;
        
            b = ngx_calloc_buf(r->pool);
            (*chain->last)->buf = b;
            (*chain->last)->next = NULL;

            u_str = ngx_pstrdup(r->pool, &ns);
            //u_str[ns.len] = '\0';
            (*chain->last)->buf->pos = u_str;
            (*chain->last)->buf->last = u_str + ns.len;
            (*chain->last)->buf->memory = 1;
            ctx->rputs_chain = chain;

            if (r->headers_out.content_length_n == -1){
                r->headers_out.content_length_n += ns.len + 1;
            }else {
                r->headers_out.content_length_n += ns.len;
            }
        }

        if (!r->headers_out.status){
            r->headers_out.status = NGX_HTTP_OK;
        }

        if (r->method == NGX_HTTP_HEAD){
            rc = ngx_http_send_header(r);
            if (rc != NGX_OK){
                return rc;
            }
        }

        if (chain != NULL){
            (*chain->last)->buf->last_buf = 1;
        }

        rc = ngx_http_send_header(r);
        if (rc != NGX_OK){
            return rc;
        }

        ngx_http_output_filter(r, chain->out);

        ngx_http_set_ctx(r, NULL, ngx_http_php_module);

        return NGX_OK;

    }

    if (rc == NGX_ERROR || rc > NGX_OK) {

        return rc;

    }

    return NGX_OK;

}

ngx_int_t 
ngx_http_php_opcode_handler(ngx_http_request_t *r)
{
    ngx_http_php_loc_conf_t *plcf;
    plcf = ngx_http_get_module_loc_conf(r, ngx_http_php_module);
    if (plcf->opcode_handler == NULL){
        return NGX_DECLINED;
    }
    return plcf->opcode_handler(r);
}

ngx_int_t 
ngx_http_php_opcode_inline_handler(ngx_http_request_t *r)
{
    ngx_http_php_main_conf_t *pmcf = ngx_http_get_module_main_conf(r, ngx_http_php_module);
    ngx_http_php_loc_conf_t *plcf = ngx_http_get_module_loc_conf(r, ngx_http_php_module);

    ngx_int_t rc;
    ngx_http_php_ctx_t *ctx;
    ctx = ngx_http_get_module_ctx(r, ngx_http_php_module);

    if (ctx == NULL){
        ctx = ngx_pcalloc(r->pool, sizeof(*ctx));
        if (ctx == NULL){
            return NGX_ERROR;
        }
    }

    ctx->output_type = OUTPUT_OPCODE;
    ctx->opcode_logo = 0;

    ctx->request_body_more = 1;
    ngx_http_set_ctx(r, ctx, ngx_http_php_module);

    ngx_php_request = r;

    /*if (r->method == NGX_HTTP_POST){
        return ngx_http_php_content_post_handler(r);
    }*/

    NGX_HTTP_PHP_NGX_INIT;
        // location opcode
        ngx_php_ngx_run(r, pmcf->state, plcf->opcode_inline_code);
    NGX_HTTP_PHP_NGX_SHUTDOWN;

    ngx_http_php_rputs_chain_list_t *chain;
    
    ctx = ngx_http_get_module_ctx(r, ngx_http_php_module);
    chain = ctx->rputs_chain;

    if (ctx->rputs_chain == NULL){
        ngx_buf_t *b;
        ngx_str_t ns;
        u_char *u_str;
        ns.data = (u_char *)" ";
        ns.len = 1;
        
        chain = ngx_pcalloc(r->pool, sizeof(ngx_http_php_rputs_chain_list_t));
        chain->out = ngx_alloc_chain_link(r->pool);
        chain->last = &chain->out;
    
        b = ngx_calloc_buf(r->pool);
        (*chain->last)->buf = b;
        (*chain->last)->next = NULL;

        u_str = ngx_pstrdup(r->pool, &ns);
        //u_str[ns.len] = '\0';
        (*chain->last)->buf->pos = u_str;
        (*chain->last)->buf->last = u_str + ns.len;
        (*chain->last)->buf->memory = 1;
        ctx->rputs_chain = chain;

        if (r->headers_out.content_length_n == -1){
            r->headers_out.content_length_n += ns.len + 1;
        }else {
            r->headers_out.content_length_n += ns.len;
        }
    }

    //r->headers_out.content_type.len = sizeof("text/html") - 1;
    //r->headers_out.content_type.data = (u_char *)"text/html";
    if (!r->headers_out.status){
        r->headers_out.status = NGX_HTTP_OK;
    }

    if (r->method == NGX_HTTP_HEAD){
        rc = ngx_http_send_header(r);
        if (rc != NGX_OK){
            return rc;
        }
    }

    if (chain != NULL){
        (*chain->last)->buf->last_buf = 1;
    }

    rc = ngx_http_send_header(r);
    if (rc != NGX_OK){
        return rc;
    }

    ngx_http_output_filter(r, chain->out);

    ngx_http_set_ctx(r, NULL, ngx_http_php_module);

    return NGX_OK;
}

ngx_int_t 
ngx_http_php_stack_handler(ngx_http_request_t *r)
{
    ngx_http_php_loc_conf_t *plcf;
    plcf = ngx_http_get_module_loc_conf(r, ngx_http_php_module);
    if (plcf->stack_handler == NULL){
        return NGX_DECLINED;
    }
    return plcf->stack_handler(r);
}

ngx_int_t 
ngx_http_php_stack_inline_handler(ngx_http_request_t *r)
{
    ngx_http_php_main_conf_t *pmcf = ngx_http_get_module_main_conf(r, ngx_http_php_module);
    ngx_http_php_loc_conf_t *plcf = ngx_http_get_module_loc_conf(r, ngx_http_php_module);

    ngx_int_t rc;
    ngx_http_php_ctx_t *ctx;
    ctx = ngx_http_get_module_ctx(r, ngx_http_php_module);

    if (ctx == NULL){
        ctx = ngx_pcalloc(r->pool, sizeof(*ctx));
        if (ctx == NULL){
            return NGX_ERROR;
        }
    }

    ctx->output_type = OUTPUT_STACK;
    ctx->stack_depth = 0;
    ctx->stack_logo = 0;

    ctx->request_body_more = 1;
    ngx_http_set_ctx(r, ctx, ngx_http_php_module);

    ngx_php_request = r;

    /*if (r->method == NGX_HTTP_POST){
        return ngx_http_php_content_post_handler(r);
    }*/

    NGX_HTTP_PHP_NGX_INIT;
        // location opcode
        ngx_php_ngx_run(r, pmcf->state, plcf->stack_inline_code);
    NGX_HTTP_PHP_NGX_SHUTDOWN;

    ngx_http_php_rputs_chain_list_t *chain;
    
    ctx = ngx_http_get_module_ctx(r, ngx_http_php_module);
    chain = ctx->rputs_chain;

    if (ctx->rputs_chain == NULL){
        ngx_buf_t *b;
        ngx_str_t ns;
        u_char *u_str;
        ns.data = (u_char *)" ";
        ns.len = 1;
        
        chain = ngx_pcalloc(r->pool, sizeof(ngx_http_php_rputs_chain_list_t));
        chain->out = ngx_alloc_chain_link(r->pool);
        chain->last = &chain->out;
    
        b = ngx_calloc_buf(r->pool);
        (*chain->last)->buf = b;
        (*chain->last)->next = NULL;

        u_str = ngx_pstrdup(r->pool, &ns);
        //u_str[ns.len] = '\0';
        (*chain->last)->buf->pos = u_str;
        (*chain->last)->buf->last = u_str + ns.len;
        (*chain->last)->buf->memory = 1;
        ctx->rputs_chain = chain;

        if (r->headers_out.content_length_n == -1){
            r->headers_out.content_length_n += ns.len + 1;
        }else {
            r->headers_out.content_length_n += ns.len;
        }
    }

    //r->headers_out.content_type.len = sizeof("text/html") - 1;
    //r->headers_out.content_type.data = (u_char *)"text/html";
    if (!r->headers_out.status){
        r->headers_out.status = NGX_HTTP_OK;
    }

    if (r->method == NGX_HTTP_HEAD){
        rc = ngx_http_send_header(r);
        if (rc != NGX_OK){
            return rc;
        }
    }

    if (chain != NULL){
        (*chain->last)->buf->last_buf = 1;
    }

    rc = ngx_http_send_header(r);
    if (rc != NGX_OK){
        return rc;
    }

    ngx_http_output_filter(r, chain->out);

    ngx_http_set_ctx(r, NULL, ngx_http_php_module);

    return NGX_OK;
}

ngx_int_t 
ngx_http_php_content_post_handler(ngx_http_request_t *r)
{
    TSRMLS_FETCH();

    ngx_http_php_main_conf_t *pmcf = ngx_http_get_module_main_conf(r, ngx_http_php_module);
    ngx_http_php_loc_conf_t *plcf = ngx_http_get_module_loc_conf(r, ngx_http_php_module);

    ngx_int_t rc;
    ngx_http_php_ctx_t *ctx;
    ctx = ngx_http_get_module_ctx(r, ngx_http_php_module);

    if (ctx == NULL){
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "Get ngx_http_php_ctx_t fail");
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    if (ctx->request_body_more){
        rc = ngx_http_php_request_read_body(r);
        return rc;
    }

    NGX_HTTP_PHP_NGX_INIT;
        // main init
        if (pmcf->init_inline_code != NGX_CONF_UNSET_PTR){
            ngx_php_ngx_run(r, pmcf->state, pmcf->init_inline_code);
        }
        if (pmcf->init_code != NGX_CONF_UNSET_PTR){
            ngx_php_ngx_run(r, pmcf->state, pmcf->init_code);
        }
        // location rewrite
        if (plcf->rewrite_code != NGX_CONF_UNSET_PTR){
            ngx_php_ngx_run(r, pmcf->state, plcf->rewrite_code);
        }
        if (plcf->rewrite_inline_code != NGX_CONF_UNSET_PTR){
            ngx_php_ngx_run(r, pmcf->state, plcf->rewrite_inline_code);
        }
        // location access
        if (plcf->access_code != NGX_CONF_UNSET_PTR){
            ngx_php_ngx_run(r, pmcf->state, plcf->access_code);
        }
        if (plcf->access_inline_code != NGX_CONF_UNSET_PTR){
            ngx_php_ngx_run(r, pmcf->state, plcf->access_inline_code);
        }
        // location content
        ngx_php_ngx_run(r, pmcf->state, plcf->content_inline_code);
    NGX_HTTP_PHP_NGX_SHUTDOWN;

    ngx_http_php_rputs_chain_list_t *chain;
    
    ctx = ngx_http_get_module_ctx(r, ngx_http_php_module);
    chain = ctx->rputs_chain;
    
    if (ctx->rputs_chain == NULL){
        ngx_buf_t *b;
        ngx_str_t ns;
        u_char *u_str;
        ns.data = (u_char *)" ";
        ns.len = 1;
        
        chain = ngx_pcalloc(r->pool, sizeof(ngx_http_php_rputs_chain_list_t));
        chain->out = ngx_alloc_chain_link(r->pool);
        chain->last = &chain->out;
    
        b = ngx_calloc_buf(r->pool);
        (*chain->last)->buf = b;
        (*chain->last)->next = NULL;

        u_str = ngx_pstrdup(r->pool, &ns);
        //u_str[ns.len] = '\0';
        (*chain->last)->buf->pos = u_str;
        (*chain->last)->buf->last = u_str + ns.len;
        (*chain->last)->buf->memory = 1;
        ctx->rputs_chain = chain;

        if (r->headers_out.content_length_n == -1){
            r->headers_out.content_length_n += ns.len + 1;
        }else {
            r->headers_out.content_length_n += ns.len;
        }
    }

    //r->headers_out.content_type.len = sizeof("text/html") - 1;
    //r->headers_out.content_type.data = (u_char *)"text/html";
    if (!r->headers_out.status){
        r->headers_out.status = NGX_HTTP_OK;
    }

    if (r->method == NGX_HTTP_HEAD){
        rc = ngx_http_send_header(r);
        if (rc != NGX_OK){
            return rc;
        }
    }

    if (chain != NULL){
        (*chain->last)->buf->last_buf = 1;
    }

    rc = ngx_http_send_header(r);
    if (rc != NGX_OK){
        return rc;
    }

    ngx_http_output_filter(r, chain->out);

    ngx_http_set_ctx(r, NULL, ngx_http_php_module);

    return NGX_OK;
}

