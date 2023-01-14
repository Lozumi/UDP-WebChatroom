/*
 * Copyright (c) 2018-2022 Lozumi Everkine. All Rights Reserved.
 *
 * This file is part of UDP-WebChatroom(https://github.com/Lozumi/UDP-WebChatroom).
 *
 * NAME:server.c
 * DISP:The server program of the chatroom.
 * Version:1.0
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <signal.h>

// 消息类型
enum type_t
{
    Login = 1, // 登录
    Chat,      // 聊天
    Quit,      // 退出
};

// 定义描述消息结构体
typedef struct msg_t
{
    int type;       // 消息类型:登录  聊天  退出
    char name[32];  // 姓名
    char text[128]; // 消息正文
} MSG_t;

// 链表的节点结构体
typedef struct node_t
{
    struct sockaddr_in addr; // 数据域
    struct node_t *next;     // 指针域
} link_t;

link_t *createLink(void);
void client_login(int sockfd, link_t *p, struct sockaddr_in clientaddr, MSG_t msg);
void client_chat(int sockfd, link_t *p, struct sockaddr_in clientaddr, MSG_t msg);
void client_quit(int sockfd, link_t *p, struct sockaddr_in clientaddr, MSG_t msg);

int main(int argc, char const *argv[])
{
    int sockfd;
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
    {
        perror("socket err.");
        return -1;
    }

    // 填充服务器的ip和port
    struct sockaddr_in serveraddr, clientaddr;
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(atoi(argv[1]));
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);

    socklen_t len = sizeof(clientaddr);

    if (bind(sockfd, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0)
    {
        perror("bind err.");
        return -1;
    }
    MSG_t msg;
    // 创建子进程，父进程接收客户端的信息并处理，子进程转发消息
    pid_t pid = fork();
    if (pid < 0)
    {
        perror("fork err.");
        return -1;
    }
    else if (pid == 0)
    {
        // 创建一个空的有头单向链表
        link_t *p = createLink();
        while (1) // 收到客户端的请求，处理请求
        {
            if (recvfrom(sockfd, &msg, sizeof(msg), 0, (struct sockaddr *)&clientaddr, &len) < 0)
            {
                perror("recvfrom err.");
                return -1;
            }
            switch (msg.type)
            {
            case Login: // 登录
                client_login(sockfd, p, clientaddr, msg);
                break;
            case Chat:
                client_chat(sockfd, p, clientaddr, msg);
                break;
            case Quit:
                client_quit(sockfd, p, clientaddr, msg);
                break;
            }
        }
    }
    else
    {
        while (1) // 服务器发通知
        {
            msg.type = Chat;
            // 给结构体中的数组成员变量赋值，一般使用strcpy进行赋值
            strcpy(msg.name, "server");
            // 获取终端输入
            fgets(msg.text, sizeof(msg.text), stdin);
            // 解决发送信息时，会将换行符发送过去的问题
            if (msg.text[strlen(msg.text) - 1] == '\n')
                msg.text[strlen(msg.text) - 1] = '\0';
            // 将信息发送给同一局域网的其他客户端
            sendto(sockfd, &msg, sizeof(msg), 0, (struct sockaddr *)&serveraddr,
                   sizeof(serveraddr));
        }
    }
    // 程序结束时，关闭套接字描述符
    close(sockfd);
    return 0;
}

// 链表函数 -- 创建一个空的有头单向链表
link_t *createLink(void)
{
    link_t *p = (link_t *)malloc(sizeof(link_t));
    if (p == NULL)
    {
        perror("malloc head node err.");
        return NULL;
    }
    p->next = NULL;
    return p;
}

// 登录函数 -- 将客户端的clientaddr保存到链表中，循环链表告诉其他用户谁登录了
void client_login(int sockfd, link_t *p, struct sockaddr_in clientaddr, MSG_t msg)
{
    // 1.告诉其他用户登录的新用户是谁
    strcpy(msg.name, "server");                // 发送消息的人是服务端
    sprintf(msg.text, "%s login！", msg.name); // 服务端发送新登陆的用户名
    // 循环发送给之前已经登陆的用户
    while (p->next != NULL)
    {
        p = p->next;
        sendto(sockfd, &msg, sizeof(msg), 0, (struct sockaddr *)&(p->addr), sizeof(p->addr));
    }
    // 上面的代码运行结束后，此时链表指针已经指向了最后一个用户
    // 2.创建一个新节点保存新连接的客户端地址 ，连接到链表结尾
    link_t *pnew = (link_t *)malloc(sizeof(link_t));
    if (pnew == NULL)
    {
        perror("malloc new node err.");
    }
    // 初始化
    pnew->addr = clientaddr;
    pnew->next = NULL;
    // 链接  p是最后一个节点的地址
    p->next = pnew;
}

// 聊天信息发送函数 -- 将消息转发给所有的用户，除去发送消息的自己
void client_chat(int sockfd, link_t *p, struct sockaddr_in clientaddr, MSG_t msg)
{
    // 从链表头开始遍历
    while (p->next != NULL)
    {
        p = p->next;
        // memcmp函数，比较内存区域a和b的前n个字节
        // 参数--区域a,区域b,比较字节数n
        // 返回值--a<b返回负数，a=b返回0，a<b返回正数
        if (memcmp(&(p->addr), &clientaddr, sizeof(clientaddr)) != 0)
        {
            // 只要判断出用户地址和发送消息的用户地址不同，就将消息发送给该用户
            sendto(sockfd, &msg, sizeof(msg), 0, (struct sockaddr *)&(p->addr),
                   sizeof(p->addr));
        }
    }
    // 在服务器中打印发送的消息
    printf("%s said %s\n", msg.name, msg.text);
}

// 退出函数 -- 将客户端的clientaddr从链表中删除，循环链表告诉其他用户谁退出了
void client_quit(int sockfd, link_t *p, struct sockaddr_in clientaddr, MSG_t msg)
{
    link_t *pdel = NULL;
    // 从头开始遍历查找要删除的节点
    while (p->next != NULL)
    {
        // 如果循环到的地址是要删除的用户，则删除
        if (memcmp(&(p->next->addr), &clientaddr, sizeof(clientaddr)) == 0)
        {
            // 删除指定用户
            pdel = p->next;
            p->next = pdel->next;

            free(pdel);
            pdel = NULL;
        }
        else
        {
            // 如果不是要删除的用户，则向其发送指定用户要删除的消息
            sendto(sockfd, &msg, sizeof(msg), 0, (struct sockaddr *)&(p->next->addr),
                   sizeof(p->next->addr));
            p = p->next;
        }
    }
}