#include "ngx_http_proxy_config.h"

void ngx_str_to_cstr(ngx_str_t nstr, char* buf)
{
    ngx_memcpy(buf, nstr.data, nstr.len);
    buf[nstr.len] = '\0';
}

void set_socketaddr(struct sockaddr_in* addr, const char* ip, const u_short port)
{
    addr->sin_family = AF_INET;
    addr->sin_port   = htons(port);
    addr->sin_addr.s_addr = inet_addr(ip);
}
