#include "ngx_http_proxy_upstream.h"

extern ngx_module_t my_http_proxy_module;

static u_char* __fill_header(u_char* buf, const ngx_table_elt_t* hh) {
    buf = ngx_cpymem(buf, hh->key.data, hh->key.len);
    *buf++ = ':';
    buf = ngx_cpymem(buf, hh->value.data, hh->value.len);
    *buf++ = '\r';
    *buf++ = '\n';
    return buf;
}

ngx_int_t my_upstream_create_request(ngx_http_request_t* r) 
{
    r->upstream->request_bufs = ngx_alloc_chain_link(r->pool);
    
    // 透传http报文头
    ngx_int_t len = r->header_in->end - r->header_in->start;
    ngx_buf_t* b = ngx_create_temp_buf(r->pool, len);
    char* cur;
    for (cur = (char*)r->header_in->start; *cur && *cur != '\r'; ++cur) {
        *b->last++ = *cur;
    }
    *b->last++ = '\r';
    *b->last++ = '\n';
    //b->last = b->pos + len;
    //ngx_memcpy(b->pos, r->header_in->start, len);
    ngx_list_part_t* part = &r->headers_in.headers.part;
    ngx_table_elt_t* hh   = (ngx_table_elt_t*)part->elts;
    int i;
    for (i = 0; ; i++) {
        if (i >= (int)part->nelts) {
            if (part->next == NULL) {
                break;
            }
            
            part = part->next;
            hh   = (ngx_table_elt_t*)part->elts;
            i    = 0;
        }
        
        if (hh[i].hash == 0) {
            continue;
        }
        
        b->last = __fill_header(b->last, &hh[i]);
    }
    *b->last++ = '\r';
    *b->last++ = '\n';
    *b->last++ = '\0';
    r->upstream->request_bufs->buf = b;
    r->upstream->request_bufs->next = NULL;

    r->upstream->request_sent = 0;
    r->upstream->header_sent  = 0;

    // header_hash不可以为0
    r->header_hash = 1;
    return NGX_OK;
}

ngx_int_t my_upstream_process_status_line(ngx_http_request_t* r)
{
    size_t len;
    ngx_int_t rc;
    ngx_http_upstream_t *u;

    // 取出http请求的上下文
    ngx_http_proxy_ctx_t *http_ctx = (ngx_http_proxy_ctx_t *)ngx_http_get_module_ctx(r, my_http_proxy_module);
    if (http_ctx == NULL) {
        return NGX_ERROR;
    }
    
    u = r->upstream;
    rc = ngx_http_parse_status_line(r, &u->buffer, &http_ctx->status);
    if (rc == NGX_AGAIN)
        return rc;

    if (rc == NGX_ERROR) {
        //todo 添加日志
        return NGX_OK;
    }

    if (u->state) {
        u->state->status = http_ctx->status.code;
    }
    
    // upstream中的headers_in将被发送给下游
    u->headers_in.status_n = http_ctx->status.code;
    len = http_ctx->status.end - http_ctx->status.start;
    u->headers_in.status_line.len = len;
    u->headers_in.status_line.data = ngx_pnalloc(r->pool, len);
    // todo 内存分配错误处理
    
    ngx_memcpy(u->headers_in.status_line.data, http_ctx->status.start, len);
        
    // 状态行处理完成，重置响应头处理函数
    u->process_header = my_upstream_process_header;
    
    return my_upstream_process_header(r);
}

static ngx_int_t __process_header_line(ngx_http_request_t* r,
                             ngx_http_upstream_main_conf_t* umcf) 
{
    ngx_table_elt_t *h = ngx_list_push(&r->upstream->headers_in.headers);
    if (h == NULL) 
        return NGX_ERROR;
  
    ngx_http_upstream_header_t *hh;
      
    h->hash = r->header_hash;
    h->key.len = r->header_name_end - r->header_name_start;
    h->value.len = r->header_end - r->header_start;
    h->key.data = ngx_pnalloc(r->pool, h->key.len * 2 + h->value.len + 3);
    if (h->key.data == NULL)
        return NGX_ERROR;

    u_char* key_start = h->key.data;
    u_char* value_start = key_start + h->key.len + 1;
    h->value.data = value_start;
    u_char* lowkey_start = value_start + h->value.len + 1;
    h->lowcase_key = lowkey_start;

    ngx_memcpy(key_start, r->header_name_start, h->key.len);
    key_start[h->key.len] = '\0';
    ngx_memcpy(value_start, r->header_start, h->value.len);
    value_start[h->value.len] = '\0';
    
    if (h->key.len == r->lowcase_index) {
        ngx_memcpy(lowkey_start, r->lowcase_header, h->key.len);
    } else {
        ngx_strlow(lowkey_start, key_start, h->key.len);
    }
    
    // upstream模块对头部处理
    hh = ngx_hash_find(&umcf->headers_in_hash, h->hash, h->lowcase_key, h->key.len);
    if (hh && hh->handler(r, h, hh->offset) != NGX_OK) {
        return NGX_ERROR;
    }

    return NGX_OK;
}

ngx_int_t my_upstream_process_header(ngx_http_request_t* r) {
    ngx_int_t rc;
    ngx_http_upstream_main_conf_t *umcf = (ngx_http_upstream_main_conf_t*)ngx_http_get_module_main_conf(r, ngx_http_upstream_module);
    
    for(;;) {
        rc = ngx_http_parse_header_line(r, &r->upstream->buffer, 1);
        if (rc == NGX_OK) {
            if (__process_header_line(r, umcf) == NGX_OK)
                continue;
            return NGX_ERROR;
        }

        if (rc == NGX_HTTP_PARSE_HEADER_DONE) {
            return NGX_OK;
        }

        if (rc == NGX_AGAIN) {
            return rc;
        }

        return NGX_HTTP_UPSTREAM_INVALID_HEADER;
    }
}


void my_upstream_finalize_request(ngx_http_request_t* r, ngx_int_t rc) {

}
