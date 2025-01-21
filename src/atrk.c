#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <getopt.h>
#include "utils.h"


int wait_conn(int s, int w) {
    struct timeval tv; 
    fd_set fds_w; 
    int valopt; 
    int res;
    socklen_t lon; 

    tv.tv_sec = w; 
    tv.tv_usec = 0; 
    FD_ZERO(&fds_w); 
    FD_SET(s, &fds_w); 
    res = select(s+1, NULL, &fds_w, NULL, w==-1?NULL:&tv); 
    if (res < 0 && errno != EINTR) { 
        return -1;
    } 
    else if (res > 0) {
        lon = sizeof(int); 
        if (getsockopt(s, SOL_SOCKET, SO_ERROR, (void*)(&valopt), &lon) < 0) {
            return -1;
        }
        if (valopt) {
            return -1;
        } 
        return 0;
    } 
    else { 
        return -1;
    } 
}

int connect2(struct sockaddr_in *addr, socklen_t addlen, int timeout_sec) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        //perror("socket");
        return -2;
    }

    int flags = fcntl(sockfd, F_GETFL, 0);
    flags |= O_NONBLOCK;
    if (-1 == fcntl(sockfd, F_SETFL, flags)){
        //perror("fcntl");
        return -1;
    }

    int result;
    result = connect(sockfd, (struct sockaddr*)addr, addlen);
    if (result == 0) {
        return sockfd;
    }else if (errno == EINPROGRESS) {
        if (timeout_sec == -2) {
            return sockfd;
        }
        if (wait_conn(sockfd, timeout_sec) == 0)
            return sockfd;        
    }
    close(sockfd);
    return -1;
}

int connect1(const char* ip, int port, int timeout_sec) {
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip, &(server_addr.sin_addr)) <= 0) {
        //perror("inet_pton");
        return -1;
    }
    return connect2(&server_addr, sizeof(server_addr), timeout_sec);
}

int process_events(int epfd, int *ecount, int timeout_sec)
{
    int total = *ecount;
    struct epoll_event evls[256];
    struct epoll_event event;
    int nready = 0;
    int i;
    int s;
    struct sockaddr_in addr;
    socklen_t addr_len;
    while(total > 0)
    {
        nready = epoll_wait(epfd, evls, 256, timeout_sec==-1?-1:timeout_sec*1000);
        if(nready == -1)
        {
            perror("epoll_wait");
            *ecount = total;
            return -1;
        }
        if(nready == 0)
        {
            //printf("epoll_wait timed out\n.");
            break;
        }
        for(i = 0; i < nready; i++)
        {
            event = evls[i];
            switch(event.events)
            {
                case EPOLLOUT:
                    addr_len = sizeof(addr);
                    if (getpeername(event.data.fd, (struct sockaddr *)&addr, &addr_len) == 0){
                        s = connect2(&addr, addr_len, timeout_sec);
                        if (s >= 0) {
                            printf("%d\n", ntohs(addr.sin_port));
                            close(s);
                        }
                    }
                case EPOLLERR:
                default:
                    if (epoll_ctl(epfd, EPOLL_CTL_DEL,event.data.fd, &event) < 0) {
                        perror("epoll_ctl");
                        *ecount = total;
                        return -1;
                    }
                    total -= 1;
            }
        }
    }
    *ecount = total;
    return 0;
}

static void applet_tcp_printHelp()
{
    fprintf(stderr, "Usage: tcp [OPTIONS] <ip> <start_port> <end_port>\n");
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -w\n");
    fprintf(stderr, "  -h\n");
}

int applet_tcp(int argc, char **argv){
    int ret = 0;
    const char* ip = NULL;
    int start_port = 0;
    int end_port = 0;
    int timeout_sec = 10;
    int epfd;
    int s;
    int pending_sd[256];
    int nPending = 0;
    struct epoll_event event;
    int ecount=0;
    int option;

    while ((option = getopt(argc, argv, "w:h")) != -1) {
        switch (option) 
        {
        case 'w':
            timeout_sec = atoi(optarg);
            break;
        case 'h':
            applet_tcp_printHelp();
            return 0;
        case '?':
            return 1;
        default:
            break;
        }
    }

    if (optind+3 != argc) {
        applet_tcp_printHelp();
        return 1;
    }
    ip = argv[optind];
    start_port = atoi(argv[optind+1]);
    end_port = atoi(argv[optind+2]);
    if (start_port > end_port) {
        fprintf(stderr, "Start port should be less than or equal to end port.\n");
        return 1;
    }

    fprintf(stderr, "Scanning ports %d to %d on %s:\n", start_port, end_port, ip);

    epfd = epoll_create(256);

    for (int port = start_port; port <= end_port; ) {
        s = connect1(ip, port, -2);
        if (s >= 0) {
            pending_sd[nPending++] = s;
            memset(&event, 0, sizeof(event));
            event.events = EPOLLOUT|EPOLLERR;
            event.data.fd = s;
            if(epoll_ctl(epfd, EPOLL_CTL_ADD, s, &event) < 0)
            {
                perror("epoll_ctl");
                ret = 1;
                break;
            }
            ecount += 1;
        }

        if (s == -2 || ecount >= 256) {
            process_events(epfd, &ecount, timeout_sec);
            while (nPending > 0) {
                close(pending_sd[nPending-1]);
                nPending -= 1;
            }
            ecount = 0;            
        }
        if (s != -2)
            ++port;
    }
    process_events(epfd, &ecount, timeout_sec);
    while (nPending > 0) {
        close(pending_sd[nPending-1]);
        nPending -= 1;
    }
    close(epfd);
    //fprintf(stderr, "info: done\n");
    return ret;
}

int test_exists(const char* filename) {
    mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
    int fd = open(filename, O_CREAT | O_EXCL | O_WRONLY, mode);
    if (fd == -1) {
        if (errno == EEXIST){
            return 0;
        } else {
            return 1;
        }
    }
    close(fd);
    unlink(filename);
    return 2;
}

int applet_file(int argc, char **argv){
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <dir> <start> <end>\n", argv[0]);
        return 1;
    }

    char current_dir[512] = "";
    char *target_dir = argv[1];
    int start = atoi(argv[2]);
    int end = atoi(argv[3]);
    char snum[32];

    getcwd(current_dir, sizeof(current_dir));

    if (chdir(target_dir) != 0) {
        fprintf(stderr, "Failed to change directory\n");
        return EXIT_FAILURE;
    }
    for (int num = start; num <= end; ++num) {
        sprintf(snum, "%d", num);
        if (test_exists(snum) == 0){
            fprintf(stdout, "%d\n", num);
        }
    }
    chdir(current_dir);
    return 0;
}

int applet_access(int argc, char **argv){
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <fmt> <start> <end>\n", argv[0]);
        return 1;
    }

    int start = atoi(argv[2]);
    int end = atoi(argv[3]);
    char path[512];

    for (int num = start; num <= end; ++num) {
        sprintf(path, argv[1], num);
        if (access(path, F_OK) == 0) {
            fprintf(stdout, "%d\n", num);
        }
    }
    return 0;
}

int applet_kill(int argc, char **argv){
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <start> <end>\n", argv[0]);
        return 1;
    }

    int start = atoi(argv[1]);
    int end = atoi(argv[2]);

    for (int num = start; num <= end; ++num) {
        if (kill((pid_t)num, 0) == 0) {
            fprintf(stdout, "%d\n", num);
        }
    }
    return 0;
}

int isLittleEndian() {
    int num = 1;
    unsigned char* ptr = (unsigned char*)&num;
    return (int)(*ptr);
}
void hexStringToBinary(const char* hexString, char* buffer, int size) {
    int length = strlen(hexString);
    int i, j;
    char temp[3] = {0};

    for (i = 0, j = 0; i < length && j<size; i += 2, j++) {
        strncpy(temp, hexString + i, 2);
        buffer[j] = strtol(temp, NULL, 16);
    }
}
void hexToInAddr(const char* hexString, char* buffer, int size) {
    int length = strlen(hexString);
    int i, j;
    char temp[9] = {0};
    uint32_t dw;

    for (i = 0, j = 0; i < length && j<size; i += 8, j += 4) {
        strncpy(temp, hexString + i, 8);
        hexStringToBinary(temp, (char*)&dw, 4);
        if (isLittleEndian()) {
           dw = ((dw & 0xFF) << 24) | 
           ((dw & 0xFF00) << 8) |
           ((dw & 0xFF0000) >> 8) |
           ((dw & 0xFF000000) >> 24);
        }
        *(uint32_t*)&buffer[j] = dw;
    }
}
int parse_procnet(const char *nettcp, int v){
    FILE* file;
    char line[256];

    file = fopen(nettcp, "r");
    if (file == NULL) {
        perror("Failed to open nettcp");
        return EXIT_FAILURE;
    }

    fgets(line, sizeof(line), file);
    while (fgets(line, sizeof(line), file)) {
        char local_addr[64];
        unsigned int local_port;
        char rem_addr[64];
        unsigned int rem_port;
        unsigned int state;
        struct  in_addr addr4;
        struct  in6_addr addr6;
        char strip[INET_ADDRSTRLEN];
//   3: 0100007F:0277 00000000:0000 0A 00000000:00000000 00:00000000 00000000     0        0 21875 1 ffff8800395407c0 100 0 0 10 0                     
//   4: 0100007F:0019 00000000:0000 0A 00000000:00000000 00:00000000 00000000     0        0 22179 1 ffff880039540f80 100 0 0 10 0                     
//   0: 00000000000000000000000000000000:006F 00000000000000000000000000000000:0000 0A 00000000:00000000 00:00000000 00000000     0        0 16012 1 ffff880039230000 100 0 0 10 0
//   1: 00000000000000000000000000000000:0016 00000000000000000000000000000000:0000 0A 00000000:00000000 00:00000000 00000000     0        0 21143 1 ffff88002adf0000 100 0 0 10 0
        state = 0;
        if (v == 4) {
            sscanf(line, "%*s %8s:%X %8s:%X %X", local_addr, &local_port, rem_addr, &rem_port, &state);
            hexToInAddr(local_addr, (char*)&addr4, sizeof(addr4));
            inet_ntop(AF_INET, &addr4, strip, INET_ADDRSTRLEN);
        }
        else {
            sscanf(line, "%*s %32s:%X %32s:%X %X", local_addr, &local_port, rem_addr, &rem_port, &state);
            hexToInAddr(local_addr, (char*)&addr6, sizeof(addr6));
            inet_ntop(AF_INET6, &addr6, strip, INET_ADDRSTRLEN);
        }
        if (state == 0x0A) {
            printf("%s:%u\n", strip, local_port);
        }
    }

    fclose(file);
    return EXIT_SUCCESS;
}

int getlistening(int argc, char **argv) {

    parse_procnet("/proc/net/tcp", 4);
    parse_procnet("/proc/net/tcp6", 6);

    return EXIT_SUCCESS;
}


int get_udp_ports(const char *netudp, int v, unsigned short *ports) {
    FILE* file;
    char line[256];

    file = fopen(netudp, "r");
    if (file == NULL) {
        return EXIT_FAILURE;
    }

    fgets(line, sizeof(line), file);
    while (fgets(line, sizeof(line), file)) {
        char local_addr[64];
        unsigned int local_port;
        char rem_addr[64];
        unsigned int rem_port;
        unsigned int state;
        struct  in_addr addr4;
        struct  in6_addr addr6;
        char strip[INET_ADDRSTRLEN];
        state = 0;
        if (v == 4) {
            sscanf(line, "%*s %8s:%X %8s:%X %X", local_addr, &local_port, rem_addr, &rem_port, &state);
            hexToInAddr(local_addr, (char*)&addr4, sizeof(addr4));
            inet_ntop(AF_INET, &addr4, strip, INET_ADDRSTRLEN);
        }
        else {
            sscanf(line, "%*s %32s:%X %32s:%X %X", local_addr, &local_port, rem_addr, &rem_port, &state);
            hexToInAddr(local_addr, (char*)&addr6, sizeof(addr6));
            inet_ntop(AF_INET6, &addr6, strip, INET_ADDRSTRLEN);
        }
        ports[local_port & 0xffff] = 1;
    }

    fclose(file);
    return EXIT_SUCCESS;
}

int applet_hiddenudp1(int argc, char **argv){

    unsigned short *ports = malloc(sizeof(unsigned short) * 65536);
    memset(ports, 0, sizeof(unsigned short) * 65536);

    get_udp_ports("/proc/net/udp", 4, ports);
    get_udp_ports("/proc/net/udp6", 6, ports);

    for (int port = 1; port <= 65535; ++port) {
        int sock = socket(AF_INET, SOCK_DGRAM, 0);
        if (sock < 0) continue;

        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port);

        if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
            if (!ports[port & 0xffff]) {
                printf("%d\n", port);
            }
        }
        close(sock);
    }

    free(ports);
    return 0;
}

int applet_echo(int argc, char **argv){
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <string>\n", argv[0]);
        return 1;
    }
    fprintf(stdout, "%s\n", argv[1]);
    return 0;
}

static
int vector_string_index(struct Vector* vector, const char *name)
{
    int n;
    char *item;
    for (n=0; n<vector->size; n++) {
        item = (char *)vector_getp(vector, n);
        if (strcmp(name, item) == 0) {
            return n;
        }
    }
    return -1;
}

static
void vector_string_cmp(struct Vector* vector1, struct Vector* vector2, struct Vector* vector_in1_notin2) {
    int n;
    char *item;
    int index;
    for (n=0; n<vector1->size; n++) {
        item = (char*)vector_getp(vector1, n);
        index = vector_string_index(vector2, item);
        if (index == -1) {
            vector_pushBackp(vector_in1_notin2, item);
        }
    }
}

int applet_diffs(int argc, char **argv){
    struct Vector sptr1;
    struct Vector sptr2;
    struct Vector dptr;
    int npart = 0;
    char line[1024];
    size_t length;
    size_t i;
    char *item;

    vector_init(&sptr1);
    vector_init(&sptr2);
    vector_init(&dptr);
    
    while (fgets(line, sizeof(line), stdin)) {
        length = strlen(line);
        while (length > 0 && (line[length - 1] == '\r' || line[length - 1] == '\n')) {
            line[length - 1] = '\0';
            length -= 1;
        }
        if (!length) {
            continue;
        }
        if (length == 36 && strcmp(line, "21552e15-c081-4e48-9b0d-747de203c68a") == 0) {
            npart = 1;
            continue;
        }

        if (npart == 0) {
            vector_pushBackp(&sptr1, strdup(line));
        }else{
            vector_pushBackp(&sptr2, strdup(line));
        }
    }
    vector_string_cmp(&sptr1, &sptr2, &dptr);
    for (i=0; i<dptr.size; i++) {
        item = (char*)vector_getp(&dptr, i);
        fprintf(stdout, "%s\n", item);
    }
    vector_free(&dptr, 0);
    vector_free(&sptr1, 1);
    vector_free(&sptr2, 1);
    return 0;
}

#ifdef FHF_MAIN
extern int fhf_main(int argc, char **argv);
int applet_fhf(int argc, char **argv) {
    return fhf_main(argc, argv);
}
#endif

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <applet>\n", argv[0]);
        fprintf(stderr, \
        "  tcp\n"
        "  file\n"
        "  access\n"
        "  kill\n"
        "  listen\n"
        "  diffs\n"
        "  fhudp1\n"
#ifdef FHF_MAIN        
        "  fhf\n"
#endif
        );
        return 1;
    }

    if (strcmp(argv[1], "echo") == 0){
        return applet_echo(argc-1, argv+1);
    }else if (strcmp(argv[1], "tcp") == 0){
        return applet_tcp(argc-1, argv+1);
    }else if (strcmp(argv[1], "file") == 0){
        return applet_file(argc-1, argv+1);
    }else if (strcmp(argv[1], "access") == 0){
        return applet_access(argc-1, argv+1);
    }else if (strcmp(argv[1], "kill") == 0){
        return applet_kill(argc-1, argv+1);
    }else if (strcmp(argv[1], "listen") == 0){
        return getlistening(argc-1, argv+1);
    }else if (strcmp(argv[1], "diffs") == 0){
        return applet_diffs(argc-1, argv+1);
    }else if (strcmp(argv[1], "fhudp1") == 0){
        return applet_hiddenudp1(argc-1, argv+1);
    }
#ifdef FHF_MAIN
    else if (strcmp(argv[1], "fhf") == 0){
        return applet_fhf(argc-1, argv+1);
    }
#endif
    else{
        fprintf(stderr, "applet not found\n");
        return 1;
    }
}
