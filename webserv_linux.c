#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
//#include <iostream>
//using namespace std;

#define BUF_SIZE 1024
#define SMALL_BUF 100

void *request_handler(void *arg);
void send_data(FILE *fp, char *ct, char *file_name);
char *content_type(char *file);
void send_error(FILE *fp);
void error_handling(char *message);

int main(int argc, char *argv[])
{
    int serv_sock, clnt_sock;
    struct sockaddr_in serv_adr, clnt_adr;
    int clnt_adr_size;
    char buf[BUF_SIZE];
    pthread_t t_id;
    if (argc != 2)
    {
        printf("Usage : %s <port>\n", argv[0]);
        exit(1);
    }

    serv_sock = socket(PF_INET, SOCK_STREAM, 0);
    memset(&serv_adr, 0, sizeof(serv_adr));
    serv_adr.sin_family = AF_INET;
    serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_adr.sin_port = htons(atoi(argv[1]));
    if (bind(serv_sock, (struct sockaddr *)&serv_adr, sizeof(serv_adr)) == -1)
        error_handling("bind() error");
    if (listen(serv_sock, 20) == -1)
        error_handling("listen() error");

    while (1)
    {
        clnt_adr_size = sizeof(clnt_adr);
        clnt_sock = accept(serv_sock, (struct sockaddr *)&clnt_adr, &clnt_adr_size);//
        printf("Connection Request : %s:%d\n",
               inet_ntoa(clnt_adr.sin_addr), ntohs(clnt_adr.sin_port));
        pthread_create(&t_id, NULL, request_handler, &clnt_sock);
        pthread_detach(t_id);
    }
    close(serv_sock);
    return 0;
}

void *request_handler(void *arg)
{
    int clnt_sock = *((int *)arg);
    char req_line[SMALL_BUF];
    FILE *clnt_read;
    FILE *clnt_write;

    char method[10];
    char ct[15];
    char file_name[30];

    clnt_read = fdopen(clnt_sock, "r");//将客户端套接字转换成FILE*型，且是写模式的指针，对于我这个服务端，我利用这个套接字可以实现读或者写，也就是说向客户端发送或者接收信息，但我现在将两种功能分离、
    clnt_write = fdopen(dup(clnt_sock), "w");//r型指针搭配fgets():意思是从clnt_read中读取数据到缓冲区req_line；w型指针则搭配fputs()
    fgets(req_line, SMALL_BUF, clnt_read);
    if (strstr(req_line, "HTTP/") == NULL)//在字符串req_line中查找第一次出现"HTTP:/"的位置，返回的是出现的位置，如果没有找到就返回NULL
    {
        send_error(clnt_write);
        fclose(clnt_read);
        fclose(clnt_write);
        return;
    }
  //  cout<<"req_line: "<<req_line<<endl;
    printf("the req_line is: %s\n",req_line);

    strcpy(method, strtok(req_line, " /"));//将第二个参数（是个指针）所指向的字符串复制到method上。
    //strtok的含义是找到req_line中可能存在"/"位置，并将req_line中"/"替换尘成"\0"，返回被分割的第一个子字符串
    //但为什么我们说它是一个分割函数，第一次调用形式str* a = strtok(req_line,"/"),则返回"/"前一部分的字符串，而再一次以下面这种形式调用:strtok(NULL,"/")则将第二个"/"前面的字符串返回,如果没有则返回当前字符串
    //所以这里的实现其实就是将//前后两部分字符串分别保存在method和filename中，前部分表示通信协议，后部分表示客户端想访问的文件名。
    char *tmp = strtok(NULL, " /");
    printf("the tmp is: %s\n",tmp);
    strcpy(file_name, tmp);

  //  cout<<"the communicaiton protocol is:"<<method<<endl;
    printf("the method is: %s\n",method);
 //  cout<<"the filename which the client want to visit:"<<file_name;
        printf("the filename is: %s\n",file_name);
    strcpy(ct, content_type(file_name));//这里的ct的值"text/html"
    printf("the ct is : %s\n",ct);
    if (strcmp(method, "GET") != 0)
    {
        send_error(clnt_write);
        fclose(clnt_read);
        fclose(clnt_write);
        return;
    }
    fclose(clnt_read);
    send_data(clnt_write, ct, file_name);//这是打算向客户端套接字发送，
}
void send_data(FILE *fp, char *ct, char *file_name)
{
    char protocol[] = "HTTP/1.0 200 OK\r\n";
    char server[] = "Server:Linux Web Server \r\n";
    char cnt_len[] = "Content-length:2048\r\n";
    char cnt_type[SMALL_BUF];
    char buf[BUF_SIZE];
    FILE *send_file;

    sprintf(cnt_type, "Content-type:%s\r\n\r\n", ct);
    send_file = fopen(file_name, "r");
    if (send_file == NULL)
    {
        send_error(fp);
        return;
    }

    //传输头信息
    fputs(protocol, fp);
    fputs(server, fp);
    fputs(cnt_len, fp);
    fputs(cnt_type, fp);

    //传输请求数据
    while (fgets(buf, BUF_SIZE, send_file) != NULL)
    {
        fputs(buf, fp);
        fflush(fp);//刷新缓冲区
    }
    fflush(fp);
    fclose(fp);
}
char *content_type(char *file)
{
    char extension[SMALL_BUF];
    char file_name[SMALL_BUF];
    strcpy(file_name, file);
    //这里应该是实现将index.html通过.分开，后面的则是extension，也就是拓展名
    strtok(file_name, ".");
    strcpy(extension, strtok(NULL, "."));

    if (!strcmp(extension, "html") || !strcmp(extension, "htm"))//两者相等，则返回0
        return "text/html";
    else
        return "text/plain";
}
void send_error(FILE *fp)
{
    char protocol[] = "HTTP/1.0 400 Bad Request\r\n";
    char server[] = "Server:Linux Web Server \r\n";
    char cnt_len[] = "Content-length:2048\r\n";
    char cnt_type[] = "Content-type:text/html\r\n\r\n";
    char content[] = "<html><head><title>NETWORK</title></head>"
                     "<body><font size=+5><br>发生错误！ 查看请求文件名和请求方式!"
                     "</font></body></html>";
    fputs(protocol, fp);
    fputs(server, fp);
    fputs(cnt_len, fp);
    fputs(cnt_type, fp);
    fflush(fp);
}
void error_handling(char *message)
{
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}
