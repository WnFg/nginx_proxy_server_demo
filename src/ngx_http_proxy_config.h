#ifndef NGX_HTTP_PROXY_CONFIG_H
#define NGX_HTTP_PROXY_CONFIG_H
#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include <nginx.h>
#include <ngx_string.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <ngx_alloc.h>

#define ngx_alloc(pool, obj) \
    (obj*) ngx_pcalloc((pool), sizeof(obj))

void ngx_str_to_cstr(ngx_str_t nstr, char* buf);
void set_socketaddr(struct sockaddr_in* addr, const char* ip, const u_short port);

// 自定义http请求上下文
typedef struct {
    ngx_http_status_t status;
}ngx_http_proxy_ctx_t;

#endif
