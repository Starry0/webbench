#include "socket.c"
#include <unistd.h>
#include <sys/param.h>
#include <rpc/types.h>
#include <getopt.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <stdio.h>

/*values*/
volatile int timerexpired = 0;//是否达到指定时间
int speed = 0;  //记录服务器响应的数量
int falied = 0; //失败的数量
int bytes = 0;  //读取成功的字节数

/*globals*/
int http10 = 1; /*0 - http/0.9, 1-http/1.0 2-http/1.1*/
/*Allow: GET, HEAD, OPTIONS, TRACE */
#define METHOD_GET 0
#define METHOD_HEAD 1
#define METHOD_OPTIONS 2
#define METHOD_TRACE 3
#define PROGRAM_VERSION "1.5"
int method = METHOD_GET; //请求方法
int clients = 1;    //客户端数量
int force = 0;
int force_reload = 0;
int proxyport = 80;
char *proxyhost = NULL;
int benchtime = 30;
/* internal */
int mypipe[2];
char host[MAXHOSTNAMELEN];
#define REQUEST_SIZE 2048
char request[REQUEST_SIZE];

static const struct option long_options[]=
{
 {"force",no_argument,&force,1},
 {"reload",no_argument,&force_reload,1},
 {"time",required_argument,NULL,'t'},
 {"help",no_argument,NULL,'?'},
 {"http09",no_argument,NULL,'9'},
 {"http10",no_argument,NULL,'1'},
 {"http11",no_argument,NULL,'2'},
 {"get",no_argument,&method,METHOD_GET},
 {"head",no_argument,&method,METHOD_HEAD},
 {"options",no_argument,&method,METHOD_OPTIONS},
 {"trace",no_argument,&method,METHOD_TRACE},
 {"version",no_argument,NULL,'V'},
 {"proxy",required_argument,NULL,'p'},
 {"clients",required_argument,NULL,'c'},
 {NULL,0,NULL,0}
};

static void alarm_handler(int signal) {
    timerexpired = 1;
}
static void usage(void) {
    fprintf(stderr,
	"webbench [option]... URL\n"
	"  -f|--force               Don't wait for reply from server.\n"
	"  -r|--reload              Send reload request - Pragma: no-cache.\n"
	"  -t|--time <sec>          Run benchmark for <sec> seconds. Default 30.\n"
	"  -p|--proxy <server:port> Use proxy server for request.\n"
	"  -c|--clients <n>         Run <n> HTTP clients at once. Default one.\n"
	"  -9|--http09              Use HTTP/0.9 style requests.\n"
	"  -1|--http10              Use HTTP/1.0 protocol.\n"
	"  -2|--http11              Use HTTP/1.1 protocol.\n"
	"  --get                    Use GET request method.\n"
	"  --head                   Use HEAD request method.\n"
	"  --options                Use OPTIONS request method.\n"
	"  --trace                  Use TRACE request method.\n"
	"  -?|-h|--help             This information.\n"
	"  -V|--version             Display program version.\n"
	);
}
void build_request(void);
static int bench(void);
void benchcore(const char *host, const int port, const char *req);

int main(int argc, char *argv[]) {
    int opt = 0;
    int options_index = 0;
    char *tmp = NULL;

    if(argc == 1) {
        usage();
        return 2;
    }
    while ((opt = getopt_long(argc, argv, "912Vfrt:p:c:?h",long_options,&options_index)) != EOF) {
        switch(opt) {
            case 0: break;
            case 'f': force = 1;break;
            case 'r': force_reload = 1;break;
            case '9': http10 = 0;break;
            case '1': http10 = 1;break;
            case '2': http10 = 2;break;
            case 'V': printf(PROGRAM_VERSION"\n");exit(0);
            case 'p': {
                tmp = strrchr(optarg,':');
                proxyhost = optarg;
                if(tmp == NULL) break;
                if(tmp == optarg) {
                    fprintf(stderr,"Error in option --proxy %s: Missing hostname.\n",optarg);
		            return 2;
                }
                
            } 
        }
    }
    

    return 0;
}