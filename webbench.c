#include "socket.c"
#include <unistd.h>
#include <sys/param.h>
#include <rpc/types.h>
#include <getopt.h>
#include <strings.h>
#include <time.h>
#include <signal.h>
#include <stdio.h>

/*values*/
volatile int timerexpired = 0;//是否达到指定时间
int speed = 0;  //记录服务器响应的数量
int failed = 0; //失败的数量
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
void build_request(const char* url);
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
            case 't': benchtime = atoi(optarg);break;
            case 'p': {
                tmp = strrchr(optarg,':');
                proxyhost = optarg;
                if(tmp == NULL) break;
                if(tmp == optarg) {
                    fprintf(stderr,"Error in option --proxy %s: Missing hostname.\n",optarg);
		            return 2;
                }
                if(tmp == optarg+strlen(optarg)-1) {
                    fprintf(stderr,"Error in option --proxy %s Port number is missing.\n",optarg);
                    return 2;
                }
                *tmp = '\0';
                proxyport = atoi(tmp+1);
                break;
            } 
            case ':':
            case 'h':
            case '?': usage(); return 2;break;
            case 'c': clients = atoi(optarg);break;
        }
    }
    if(optind == argc) {
        fprintf(stderr,"webbench: Missing URL!\n");
        usage();
        return 2;
    }
    if(clients == 0) clients = 1;
    if(benchtime == 0) benchtime = 60;
    build_request(argv[optind]);
    fprintf(stderr,"Webbench - Simple Web Benchmark "PROGRAM_VERSION"\n"
	 "Copyright (c) Radim Kolar 1997-2004, GPL Open Source Software.\n"
	);
    printf("\nBenchmarking: ");
    switch(method) {
        case METHOD_GET:
        default:
            printf("Get");break;
        case METHOD_HEAD:
            printf("HEAD");break;
        case METHOD_OPTIONS:
            printf("OPTION");break;
        case METHOD_TRACE:
            printf("TRACE");break;
    }
    printf(" %s",argv[optind]);
    switch(http10) {
        case 0: printf(" (using HTTP/0.9)");break;
        case 2: printf(" (using HTTP/1.1)");break;
    }
    printf("\n");
    if(clients == 1) printf("1 clien");
    else printf("%d clients",clients);
    printf(", running %d sec",benchtime);
    if(force) printf(". early socket close");
    if(proxyhost != NULL) printf(", via proxy server %s:%d",proxyhost,proxyport);
    if(force_reload) printf(", forcing reload");
    printf(".\n");
    return bench();
}

void build_request(const char* url) {
    char tmp[10];
    int i;
    bzero(host, MAXHOSTNAMELEN);
    bzero(request, REQUEST_SIZE);

    if(force_reload && proxyhost != NULL && http10 < 1) http10 = 1;
    if(method == METHOD_HEAD && http10 < 1) http10 = 1;
    if(method == METHOD_OPTIONS && http10 < 2) http10 = 2;
    if(method == METHOD_OPTIONS && http10 < 2) http10 = 2;

    switch(method) {
        default:
        case METHOD_GET: strcpy(request,"GET");break;
        case METHOD_HEAD: strcpy(request, "HEAD");break;
        case METHOD_OPTIONS: strcpy(request, "OPTIONS");break;
        case METHOD_TRACE: strcpy(request, "TRACE");break;
    }

    strcat(request, " ");

    if(strstr(url, "://") == NULL) {
        fprintf(stderr, "\n %s: is not a valid URL.\n",url);
        exit(2);
    }
    if(strlen(url) > 1500) {
        fprintf(stderr, "URL is too long.\n");
        exit(2);
    }
    if(proxyhost == NULL) {
        if(strncasecmp("http://",url,7) != 0) {
            fprintf(stderr,"\nOnly HTTP protocol is directly supported, set --proxy for others.\n");
            exit(2);
        }
    }

    i = strstr(url, "://") - url + 3;
    // printf("%s %d\n",url+i,i);

    if(strchr(url+i, '/') == NULL) {
        fprintf(stderr,"\nInvalid URL syntax - hostname don't ends with '/'.\n");
        exit(2);
    }
    if(proxyhost == NULL) {
        /** get port from hostname */
        if(strchr(url+i, ':') != NULL && strchr(url+i, ':') < strchr(url+i, '/')) {
            strncpy(host, url+i, strchr(url+i,':')-url-i);
            bzero(tmp, 10);
            strncpy(tmp, strchr(url+i,':')+1,strchr(url+i,'/')-strchr(url+i,':')-1);
            // printf("%s %s\n",host, tmp);
            proxyport = atoi(tmp);
            if(proxyport == 0) proxyport = 80;
        } else {
            strncpy(host, url+i, strcspn(url+i, "/"));
        }
        // printf("Host=%s\n",host);
        strcat(request+strlen(request), url+i+strcspn(url+i, "/"));
        // printf("request=%s\n",request);
    } else {
        strcat(request, url);
    }
    if(http10 == 1) strcat(request, " HTTP/1.0");
    else if(http10 == 2) strcat(request, " HTTP/1.1");
    strcat(request, "\r\n");
    if(http10 > 0)
        strcat(request, "User-Agent: WebBench "PROGRAM_VERSION"\r\n");
    if(proxyhost == NULL && http10 > 0) {
        strcat(request, "Host: ");
        strcat(request,host);
        strcat(request, "\r\n");
    }
    if(force_reload && proxyhost != NULL) {
        strcat(request, "Pragma: no-cache\r\n");
    }
    if(http10 > 1) {
        strcat(request, "Connection: close\r\n");
    }
    if(http10 > 0) strcat(request, "\r\n");
    // printf("request=%s\n",request);
}

static int bench() {
    int i, j, k;
    pid_t pid = 0;
    FILE *file;
    i = Socket(proxyhost==NULL?host:proxyhost, proxyport);
    if(i < 0) {
        fprintf(stderr,"\nConnect to server failed. Aborting benchmark.\n");
        return 1;
    }
    close(i);
    if(pipe(mypipe)) {
        perror("pipe failed.");
        return 3;
    }

    for(i = 0; i < clients; i ++) {
        pid = fork();
        if(pid <= (pid_t)0) {  //使得创建的进程是clients个
            sleep(1);
            break;
        }
    }
    if(pid < (pid_t)0) {
        fprintf(stderr,"problems forking worker no. %d\n",i);
        perror("fork failed.");
        return 3;
    }

    if(pid == (pid_t)0) {
        // printf("%d client",pid);
        /** in child */
        benchcore(proxyhost==NULL?host:proxyhost, proxyport, request);
        file = fdopen(mypipe[1], "w");
        if(file == NULL) {
            perror("open pipe for writing failed.");
            return 3;
        }
        fprintf(file, "%d %d %d\n",speed, failed, bytes);
        fclose(file);
        // printf("%d client",pid);
        return 0;
    } else {
        file = fdopen(mypipe[0], "r");
        if(file == NULL) {
            perror("open pipe for reading failed.");
            return 3;
        }
        setvbuf(file, NULL, _IONBF, 0);
        speed = failed = bytes = 0;
        while(1) {
            pid = fscanf(file, "%d %d %d", &i, &j, &k);
            if(pid < 2) {
                fprintf(stderr,"Some of our childrens died.\n");
                break;
            }
            speed += i;
            failed += j;
            bytes += k;
            if(--clients == 0) break;
        }
        fclose(file);
        printf("\nSpeed=%d pages/min, %d bytes/sec.\nRequests: %d susceed, %d failed.\n",
		    (int)((speed+failed)/(benchtime/60.0f)),
		    (int)(bytes/(float)benchtime),
		    speed,failed);

    }
    return i;
}

void benchcore(const char *host, const int port, const char *req) {
    int rlen;
    char buf[1500];
    int s, i;
    struct sigaction sa;
    sa.sa_handler = alarm_handler;
    sa.sa_flags = 0;
    if(sigaction(SIGALRM, &sa, NULL))
        exit(3);
    alarm(benchtime);

    rlen = strlen(req);
    printf("%s\n",req);
    nexttry:while(1) {
        if(timerexpired) {
            if(failed > 0) {
                failed--;
            }
            return;
        }
        s = Socket(host, port);
        if(s < 0) {
            printf("failed one\n");
            failed++; 
            continue;
        }
        if(rlen != write(s, req, rlen)) {
            printf("failed two\n");
            failed++;
            close(s);
            continue;
        }
        if(http10 == 0)
            if(shutdown(s, 1)) {
                printf("failed five\n");
                failed++;close(s);
                continue;
            }
        if(force == 0) {
            while(1) {
                if(timerexpired) break;
                i = read(s, buf, 1500);
                if(i < 0) {
                    printf("failed three\n");
                    failed ++;
                    close(s);
                    goto nexttry;
                } else if(i == 0) break;
                else bytes += i;
            }
        }
        if(close(s)) {
            printf("failed four\n");
            failed ++;
            continue;
        }
        speed++;
    }
}