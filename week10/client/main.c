#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "socket.h"
#include "log.h"
#include "ftp.h"

static int  m_socket_cmd;
static int  m_socket_data;
static char m_send_buffer[1024];
static char m_recv_buffer[1024];

//命令端口，发送命令
static int ftp_send_command(char *cmd)
{
	int ret;
	LOG_INFO("send command: %s\r\n", cmd);
	ret = socket_send(m_socket_cmd, cmd, (int)strlen(cmd));
	if(ret < 0)
	{
		LOG_INFO("failed to send command: %s",cmd);
		return 0;
	}
	return 1;
}

//命令端口，接收应答
static int ftp_recv_respond(char *resp, int len)
{
	int ret;
	int off;
	len -= 1;
	for(off=0; off<len; off+=ret)
	{
		ret = socket_recv(m_socket_cmd, &resp[off], 1);
		if(ret < 0)
		{
			LOG_INFO("recv respond error(ret=%d)!\r\n", ret);
			return 0;
		}
		if(resp[off] == '\n')
		{
			break;
		}
	}
	resp[off+1] = 0;
	LOG_INFO("respond:%s", resp);
	return atoi(resp);
}

//设置FTP服务器为被动模式，并解析数据端口
static int ftp_enter_pasv(char *ipaddr, int *port)
{
	int ret;
	char *find;
	int a,b,c,d;
	int pa,pb;
	ret = ftp_send_command("PASV\r\n");
	if(ret != 1)
	{
		return 0;
	}
	ret = ftp_recv_respond(m_recv_buffer, 1024);
	if(ret != 227)
	{
		return 0;
	}
	find = strrchr(m_recv_buffer, '(');
	sscanf(find, "(%d,%d,%d,%d,%d,%d)", &a, &b, &c, &d, &pa, &pb);
	sprintf(ipaddr, "%d.%d.%d.%d", a, b, c, d);
	*port = pa * 256 + pb;
	return 1;
}

//上传文件
int  ftp_upload(char *name, void *buf, int len)
{
	int  ret;
	char ipaddr[32];
	int  port;

	//查询数据地址
	ret=ftp_enter_pasv(ipaddr, &port);
	if(ret != 1)
	{
		return 0;
	}
	ret=socket_connect(m_socket_data, ipaddr, port);
	if(ret != 1)
	{
		return 0;
	}
	//准备上传
	sprintf(m_send_buffer, "STOR %s\r\n", name);
	ret = ftp_send_command(m_send_buffer);
	if(ret != 1)
	{
		return 0;
	}
	ret = ftp_recv_respond(m_recv_buffer, 1024);
	if(ret != 150)
	{
		socket_close(m_socket_data);
		return 0;
	}

	//开始上传
	ret = socket_send(m_socket_data, buf, len);
	if(ret != len)
	{
		LOG_INFO("send data error!\r\n");
		socket_close(m_socket_data);
		return 0;
	}
	socket_close(m_socket_data);

	//上传完成，等待回应
	ret = ftp_recv_respond(m_recv_buffer, 1024);
	return (ret==226);
}

//下载文件
int  ftp_download(char *name, void *buf, int len)
{
	int   i;
	int   ret;
	char  ipaddr[32];
	int   port;

	//查询数据地址
	ret = ftp_enter_pasv(ipaddr, &port);
	if(ret != 1)
	{
		return 0;
	}
	//连接数据端口
	ret = socket_connect(m_socket_data, ipaddr, port);
	if(ret != 1)
	{
		LOG_INFO("failed to connect data port\r\n");
		return 0;
	}

	//准备下载
	sprintf(m_send_buffer, "RETR %s\r\n", name);
	ret = ftp_send_command(m_send_buffer);
	if(ret != 1)
	{
		return 0;
	}
	ret = ftp_recv_respond(m_recv_buffer, 1024);
	if(ret != 150)
	{
		socket_close(m_socket_data);
		return 0;
	}

	//开始下载,读取完数据后，服务器会自动关闭连接
	for(i=0; i<len; i+=ret)
	{
		ret = socket_recv(m_socket_data, ((char *)buf) + i, len);
		LOG_INFO("download %d/%d.\r\n", i + ret, len);
		if(ret < 0)
		{
			LOG_INFO("download was interrupted.\r\n");
			break;
		}
	}
	//下载完成
	LOG_INFO("download %d/%d bytes complete.\r\n", i, len);
	socket_close(m_socket_data);
	ret = ftp_recv_respond(m_recv_buffer, 1024);
	return (ret==226);
}

//返回文件大小
int  ftp_filesize(char *name)
{
	int ret;
	int size;
	sprintf(m_send_buffer,"SIZE %s\r\n",name);
	ret = ftp_send_command(m_send_buffer);
	if(ret != 1)
	{
		return 0;
	}
	ret = ftp_recv_respond(m_recv_buffer, 1024);
	if(ret != 213)
	{
		return 0;
	}
	size = atoi(m_recv_buffer + 4);
	return size;
}

//登陆服务器
int ftp_login(char *addr, int port, char *username, char *password)
{
	int ret;
	LOG_INFO("connect...\r\n");
	ret = socket_connect(m_socket_cmd, addr, port);
	if(ret != 1)
	{
		LOG_INFO("connect server failed!\r\n");
		return 0;
	}
	LOG_INFO("connect ok.\r\n");
    //等待欢迎信息
	ret = ftp_recv_respond(m_recv_buffer, 1024);
	if(ret != 220)
	{
		LOG_INFO("bad server, ret=%d!\r\n", ret);
		socket_close(m_socket_cmd);
		return 0;
	}

	LOG_INFO("login...\r\n");
    //发送USER
	sprintf(m_send_buffer, "USER %s\r\n", username);
	ret = ftp_send_command(m_send_buffer);
	if(ret != 1)
	{
		socket_close(m_socket_cmd);
		return 0;
	}
	ret = ftp_recv_respond(m_recv_buffer, 1024);
	if(ret != 331)
	{
		socket_close(m_socket_cmd);
		return 0;
	}

    //发送PASS
	sprintf(m_send_buffer, "PASS %s\r\n", password);
	ret = ftp_send_command(m_send_buffer);
	if(ret != 1)
	{
		socket_close(m_socket_cmd);
		return 0;
	}
	ret = ftp_recv_respond(m_recv_buffer, 1024);
	if(ret != 230)
	{
		socket_close(m_socket_cmd);
		return 0;
	}
	LOG_INFO("login success.\r\n");

    //设置为二进制模式
	ret = ftp_send_command("TYPE I\r\n");
	if(ret != 1)
	{
		socket_close(m_socket_cmd);
		return 0;
	}
	ret = ftp_recv_respond(m_recv_buffer, 1024);
	if(ret != 200)
	{
		socket_close(m_socket_cmd);
		return 0;
	}
	return 1;
}

void ftp_quit(void)
{
	ftp_send_command("QUIT\r\n");
	socket_close(m_socket_cmd);
}

void ftp_init(void)
{
	m_socket_cmd = socket_create();
	m_socket_data= socket_create();
}
