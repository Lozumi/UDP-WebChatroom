/*
 * Copyright (c) 2018-2022 The UDP-WebChatroom project authors. All Rights Reserved.
 *
 * This file is part of UDP-WebChatroom(https://github.com/Lozumi/UDP-WebChatroom).
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

int main(int argc, char const *argv[])
{
    int sockfd;
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
    {
        perror("socket err.");
        return -1;
    }

    // 填充结构体
    struct sockaddr_in serveraddr;
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(atoi(argv[2]));
    serveraddr.sin_addr.s_addr = inet_addr(argv[1]);

    MSG_t msg;
    // UDP客户端不用bind地址，可以直接发送自己的登陆消息
    msg.type = Login; // 登录
    printf("please input login name>>");
    fgets(msg.name, sizeof(msg.name), stdin);
    if (msg.name[strlen(msg.name) - 1] == '\n')
        msg.name[strlen(msg.name) - 1] = '\0';

    sendto(sockfd, &msg, sizeof(msg), 0, (struct sockaddr *)&serveraddr,
           sizeof(serveraddr));
    // 创建父子进程：父进程收消息 子进程发送消息
    pid_t pid = fork();
    if (pid < 0)
    {
        perror("fork err.");
        return -1;
    }
    else if (pid == 0)
    {
        while (1) // 发
        {
            fgets(msg.text, sizeof(msg.text), stdin);
            if (msg.text[strlen(msg.text) - 1] == '\n')
                msg.text[strlen(msg.text) - 1] = '\0';
            // 判断发送的消息是否为“quit”退出消息
            if (strncmp(msg.text, "quit", 4) == 0)
            {
                msg.type = Quit;
                sendto(sockfd, &msg, sizeof(msg), 0, (struct sockaddr *)&serveraddr,
                       sizeof(serveraddr));
                // 杀死父进程
                kill(getppid(), SIGKILL);
                // 退出循环，子进程结束
                break;
            }
            else
            {
                msg.type = Chat;
                sendto(sockfd, &msg, sizeof(msg), 0, (struct sockaddr *)&serveraddr,
                       sizeof(serveraddr));
            }
        }
    }
    else
    {
        while (1) // 收
        {
            if (recvfrom(sockfd, &msg, sizeof(msg), 0, NULL, NULL) < 0)
            {
                perror("recvfrom err.");
                return -1;
            }
            printf("%s said %s\n", msg.name, msg.text);
        }
    }
    close(sockfd);
    return 0;
}