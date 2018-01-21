#include "ngx_http_proxy_config.h"
#include "ngx_http_proxy_upstream.h"

/*
 * 1. http核心模块定义的Http接口
 */
typedef struct {
    ngx_http_upstream_conf_t upstream_conf;
} ngx_http_proxy_conf_t;

static void *ngx_http_proxy_create_srv_conf(ngx_conf_t *cf)
{
    ngx_http_proxy_conf_t *pcf;
    pcf = ngx_alloc(cf->pool, ngx_http_proxy_conf_t);
    if (pcf == NULL) {
        return pcf;
    }

    pcf->upstream_conf.connect_timeout = 60000;
    pcf->upstream_conf.send_timeout    = 60000;
    pcf->upstream_conf.read_timeout    = 60000;
    pcf->upstream_conf.store_access    = 600;

    pcf->upstream_conf.buffering = 0;
    pcf->upstream_conf.bufs.num  = 8;
    pcf->upstream_conf.bufs.size = ngx_pagesize;
    pcf->upstream_conf.buffer_size = ngx_pagesize;

    pcf->upstream_conf.hide_headers = (ngx_array_t *)NGX_CONF_UNSET_PTR;
    pcf->upstream_conf.pass_headers = (ngx_array_t *)NGX_CONF_UNSET_PTR;
    
    return pcf;
}

//用于初始化hide_headers成员，作为ngx_http_upstream_hide_headers_hash函数的参数
static ngx_str_t  ngx_http_proxy_hide_headers[] =
{
    ngx_string("X-Pad"),
    ngx_string("X-Accel-Expires"),
    ngx_string("X-Accel-Redirect"),
    ngx_string("X-Accel-Limit-Rate"),
    ngx_string("X-Accel-Buffering"),
    ngx_string("X-Accel-Charset"),
    ngx_null_string
};

static char *my_http_proxy_merge_srv_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_http_proxy_conf_t *prev = (ngx_http_proxy_conf_t*)parent;
    ngx_http_proxy_conf_t *conf = (ngx_http_proxy_conf_t*)child;
    ngx_hash_init_t  hash;
    hash.max_size = 100;
    hash.bucket_size = 1024;
    hash.name = "proxy_headers_hash";
    //初始化hide_headers
    if (ngx_http_upstream_hide_headers_hash(cf, &conf->upstream_conf, &prev->upstream_conf, ngx_http_proxy_hide_headers, &hash)!= NGX_OK)
    {
        return NGX_CONF_ERROR;
    }

    return NGX_CONF_OK;
}

static ngx_http_module_t my_http_proxy_module_ctx = {
    NULL,
    NULL,
    NULL,
    NULL,
    ngx_http_proxy_create_srv_conf,  // 创建server配置存储
    my_http_proxy_merge_srv_conf,
    NULL,
    NULL
};

// 2. 实现模块感兴趣的配置项
// 声明处理函数 
// return: char*
// params: ngx_conf_t*  解析配置文件时的上下文
//         ngx_command_t*  当前配置项
//         void* conf      存储配置内容的结构体
static char* ngx_http_proxy_set_conf(ngx_conf_t* cf, ngx_command_t* cmd, void *conf);
static ngx_command_t ngx_http_proxy_commands[] = {
    {
        ngx_string("http_proxy"),
        NGX_HTTP_LOC_CONF | NGX_HTTP_SRV_CONF | NGX_CONF_ANY,
        ngx_http_proxy_set_conf,
        NGX_HTTP_LOC_CONF_OFFSET,
        0,
        NULL,
    },

    ngx_null_command
};

// 3. 构造标准的nginx模块
ngx_module_t my_http_proxy_module = {
    NGX_MODULE_V1,
    &my_http_proxy_module_ctx,
    ngx_http_proxy_commands,
    NGX_HTTP_MODULE,
    NULL,   // 注册在master-worker生命周期中的回调函数
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NGX_MODULE_V1_PADDING
};

// 定义配置项处理函数
// 定义相应内容生成函数
// return: 错误码
// params: http请求内容
static ngx_int_t ngx_http_proxy_handler(ngx_http_request_t *r);

static char* ngx_http_proxy_set_conf(ngx_conf_t* cf, ngx_command_t* cmd, void* conf) {
    // 核心模块定义的对应location的结构体
    ngx_http_core_loc_conf_t *clcf;
    
    clcf = (ngx_http_core_loc_conf_t*)ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);

    // 核心模块location结构体中有回调指针，作用于NGX_HTTP_CONTENT_PHASE阶段，可以用来生成响应内容
    clcf->handler = ngx_http_proxy_handler;
    
    return NGX_CONF_OK;
}

// 定义响应内容处理函数

static ngx_int_t ngx_http_proxy_handler(ngx_http_request_t *r) 
{
    // 使用upstream机制，需要初始化r->upstream成员
    ngx_http_proxy_ctx_t *myctx = ngx_http_get_module_ctx(r, my_http_proxy_module);
    if (myctx == NULL) {
        myctx = ngx_alloc(r->pool, ngx_http_proxy_ctx_t);
	ngx_http_set_ctx(r, myctx, my_http_proxy_module);
    }
    if (ngx_http_upstream_create(r) != NGX_OK) {
        //todo 错误时打上日志
        return NGX_ERROR;
    }

    // 配置upstream
    ngx_http_proxy_conf_t *my_conf = (ngx_http_proxy_conf_t*) ngx_http_get_module_srv_conf(r, my_http_proxy_module);
    ngx_http_upstream_t* u = r->upstream;
    u->conf = &my_conf->upstream_conf;
    u->buffering = (my_conf->upstream_conf).buffering;
        
    // 构造上游服务器配置
    u->resolved = ngx_alloc(r->pool, ngx_http_upstream_resolved_t);
    //todo 内存分配错误处理
    
    //从请求中取出上游服务器host
    ngx_http_headers_in_t headers = r->headers_in;
    char buf[100];
    ngx_str_to_cstr(headers.host->value, buf);
    struct hostent *pHost = gethostbyname(buf);
    
    // 获取ip
    char* ip = inet_ntoa(*(struct in_addr*)(pHost->h_addr_list[0]));
    struct sockaddr_in* up_srv_addr = ngx_alloc(r->pool, struct sockaddr_in);
    set_socketaddr(up_srv_addr, ip, 80);
    
    // 设置upstream地址
    u->resolved->sockaddr = (struct sockaddr*)up_srv_addr;
    u->resolved->socklen  = sizeof(struct sockaddr_in);
    u->resolved->naddrs   = 1;
    u->resolved->port     = 80;

    u->create_request = my_upstream_create_request;
    u->process_header = my_upstream_process_status_line;
    u->finalize_request = my_upstream_finalize_request;
    // 将count+1，作为引用计数，防止请求被销毁
    //r->main->count++;
    // 启动upstream_init
    ngx_http_read_client_request_body(r, ngx_http_upstream_init);
    //ngx_http_upstream_init(r);
    // 讲控制权交还给nginx
    return NGX_DONE;
}
