#include <stdio.h>
#include <winsock2.h>

#define SERVER_PORT 8888

int main(int argc, char* argv[])
{
    WSADATA wsaData;
    struct sockaddr_in server_addr;
    SOCKET client;
    struct sockaddr_in remoteAddr;
    int nAddrlen = sizeof(remoteAddr);
    int len;
    char buffer[256], recv_buffer[128];

    if (WSAStartup(0x202, &wsaData) != 0)
    {
        return 0;
    }

    //创建套接字
    SOCKET server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(server == INVALID_SOCKET)
    {
        printf("socket error !");
        return 0;
    }

    //绑定IP和端口
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.S_un.S_addr = INADDR_ANY;
    if(bind(server, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR)
    {
        printf("bind error !");
    }

    //开始监听
    if(listen(server, 5) == SOCKET_ERROR)
    {
        printf("listen error !");
        return 0;
    }

    printf("等待连接...\n");
    client = accept(server, (SOCKADDR *)&remoteAddr, &nAddrlen);
    if (client != INVALID_SOCKET)
    {
        printf("接收到一个来自%s的client连接\n", inet_ntoa(remoteAddr.sin_addr));

        while(1)
        {
            //接收数据
            len = recv(client, recv_buffer, sizeof(recv_buffer), 0);
            if (len <= 0)
            {
                printf("client disconnected!\n");
                closesocket(client);
                break;
            }

            recv_buffer[len] = 0;
            printf("Client says: %s\n", recv_buffer);

            //发送数据
            sprintf(buffer, "Server received your message %s\n", recv_buffer);
            send(client, buffer, strlen(buffer), 0);
        }
    }

    closesocket(server);
    WSACleanup();

    return 0;
}
