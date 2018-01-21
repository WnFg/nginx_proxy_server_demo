#ifndef NGX_HTTP_PROXY_UPSTREAM_H
#define NGX_HTTP_PROXY_UPSTREAM_H
#include "ngx_http_proxy_config.h"

// 构造请求
ngx_int_t my_upstream_create_request(ngx_http_request_t* r);
// 处理状态行
ngx_int_t my_upstream_process_status_line(ngx_http_request_t* r);
// 处理响应头
ngx_int_t my_upstream_process_header(ngx_http_request_t* r);
// 与上游服务器通话结束
void my_upstream_finalize_request(ngx_http_request_t* r, ngx_int_t rc);
// 
ngx_int_t my_http_proxy_create_request(ngx_http_request_t *r);
#endif

