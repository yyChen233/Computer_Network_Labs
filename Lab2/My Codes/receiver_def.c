#include "receiver_def.h"
#include "rtp.h"
#include <stdio.h>
#include <sys/socket.h>
#include <string.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/timerfd.h>
#include <time.h>
#include <sys/stat.h>

extern uint32_t crc32_for_byte(uint32_t r);
extern void crc32(const void *data, size_t n_bytes, uint32_t* crc);
extern uint32_t compute_checksum(const void* pkt, size_t n_bytes);

extern rtp_header_t make_rtp_header(int type, int length, uint32_t seq_num, int flag);
extern rtp_packet_t make_rtp_packet(int type, uint32_t seq_num, FILE* fp, int* len);

uint32_t window_recv = 0;
uint32_t recv_base = 0;
int socketfd_recv = 0;
char send_buffer_re[20480] = { 0 };
char recv_buffer_re[1472] = { 0 };
struct sockaddr_in cliaddr, seraddr;
rtp_packet_t packet_cache[80000];
char cached[80000] = { 0 };

int socket_and_bind(uint16_t port);
int recv_packet(int flag);

//----------------------------------------------------
int socket_and_bind(uint16_t port)
{
    socketfd_recv = socket(AF_INET, SOCK_DGRAM, 0);
    if(socketfd_recv < 0)
        return -1;
    bzero(&seraddr, sizeof(seraddr));
    seraddr.sin_family = AF_INET;
    seraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    seraddr.sin_port = htons(port);

    if(bind(socketfd_recv, (struct sockaddr*)&seraddr, sizeof(seraddr)) < 0)
        return -1;
    return 0;
}

int recv_packet(int flag)
{
    memset(recv_buffer_re, 0, sizeof(recv_buffer_re));
    socklen_t len=sizeof(cliaddr);
    int re = recvfrom(socketfd_recv, recv_buffer_re, sizeof(recv_buffer_re),
                      flag, (struct sockaddr*)&cliaddr,&len);
    if(re < 0)
        return -1;
    return re;
}

/**
 * @brief 开启receiver并在所有IP的port端口监听等待连接
 * 
 * @param port receiver监听的port
 * @param window_size window大小
 * @return -1表示连接失败，0表示连接成功
 */
int initReceiver(uint16_t port, uint32_t window_size)
{

    window_recv = window_size;

    //build socket
    if(socket_and_bind(port) < 0)
    {
        close(socketfd_recv);
        return -1;
    }

    //receive start packet

    struct timeval time_out;
    time_out.tv_sec = 10;
    time_out.tv_usec = 0;
    if (setsockopt(socketfd_recv, SOL_SOCKET, SO_RCVTIMEO,
                   &time_out, sizeof(time_out)) == -1) {
        perror("setsockopt failed:");
    }


    socklen_t len = sizeof(cliaddr);
    if(recvfrom(socketfd_recv, recv_buffer_re, sizeof(recv_buffer_re),
             0, (struct sockaddr*)&cliaddr, &len) < 11)
    {
        close(socketfd_recv);
        return -1;
    }


    time_out.tv_sec = 0;
    setsockopt(socketfd_recv, SOL_SOCKET, SO_RCVTIMEO,
               &time_out, sizeof(time_out));


    rtp_header_t start_header = *(rtp_header_t*) &recv_buffer_re;
    uint32_t my_check = start_header.checksum;
    start_header.checksum = 0;
    if(my_check != compute_checksum(&start_header, 11)
    || start_header.type != RTP_START)
    {
        close(socketfd_recv);
        return -1;
    }

    //send ack
    rtp_header_t ack_header = make_rtp_header(RTP_ACK, 0,
                                              start_header.seq_num, 1);

    memset(send_buffer_re, 0, sizeof(send_buffer_re));
    memcpy(send_buffer_re, (char*)&ack_header, 11);
    if(sendto(socketfd_recv, send_buffer_re, 11, 0,
           (struct sockaddr*)&cliaddr, len) < 0)
    {
        close(socketfd_recv);
        return -1;
    }
    //printf("Yes from recv init\n");

    return 0;
}

/**
 * @brief 用于接收数据并在接收完后断开RTP连接
 * @param filename 用于接收数据的文件名
 * @return >0表示接收完成后到数据的字节数 -1表示出现其他错误
 */
int recvMessage(char* filename)
{

    FILE* fp = fopen(filename, "w");
    if(fp == NULL)
        return -1;

    memset(packet_cache, 0, sizeof(packet_cache));
    memset(cached, 0, sizeof(cached));

    //timer
    int timer_fd;
    struct itimerspec timer_spec;
    timer_spec.it_value.tv_sec = 10;
    timer_spec.it_value.tv_nsec = 0;
    timer_spec.it_interval.tv_sec = 0;
    timer_spec.it_interval.tv_nsec = 0;

    timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC | TFD_NONBLOCK);
    timerfd_settime(timer_fd,0,&timer_spec,NULL);




    while(1)
    {
        char timer_buffer[10] = { 0 };
        int read_num = read(timer_fd,timer_buffer,8);
        if(read_num > 0)
            return -1;


        //receive rtp packet with no block
        //printf("recv_base = %d\n",recv_base);
        int rec = recv_packet(MSG_DONTWAIT);
        if(rec > 0)
        {
            timerfd_settime(timer_fd,0,&timer_spec,NULL);
            //printf("rec = %d\n",rec);
            rtp_packet_t recv_p = *(rtp_packet_t*)recv_buffer_re;
            if(recv_p.rtp.type == RTP_DATA)
            {
                //printf("data\n\n");
                uint32_t Check = recv_p.rtp.checksum;
                recv_p.rtp.checksum = 0;
                int data_len = recv_p.rtp.length;
                if(Check == compute_checksum((char*)&recv_p,11 + data_len))
                {
                    //printf("CHECK\n\n");
                    if(recv_base + window_recv > recv_p.rtp.seq_num &&
                        recv_base <= recv_p.rtp.seq_num )
                    {
                        //printf("seq=%d\n",recv_p.rtp.seq_num);
                        //seq_num legal
                        //cache
                        packet_cache[recv_p.rtp.seq_num] = recv_p;
                        cached[recv_p.rtp.seq_num] = 1;
                    }
                }
            }
            else if(recv_p.rtp.type == RTP_END && recv_p.rtp.seq_num <= recv_base)
            {
                //END
                int Check = recv_p.rtp.checksum;
                recv_p.rtp.checksum = 0;
                int data_len = recv_p.rtp.length;
                if(Check == compute_checksum(&recv_p,11 + data_len))
                {
                    rtp_header_t ACK = make_rtp_header(RTP_ACK, 0, recv_base, 1);
                    socklen_t len = sizeof(cliaddr);
                    sendto(socketfd_recv, (char *) &ACK, 11,
                           0, (struct sockaddr *) &cliaddr, len);
                    break;
                }
            }

            while(cached[recv_base] == 1)
            {
                fwrite((char*)(packet_cache[recv_base].payload), 1,
                       packet_cache[recv_base].rtp.length, fp);
                recv_base += 1;
            }

            rtp_header_t ACK = make_rtp_header(RTP_ACK, 0, recv_base, 1);
            socklen_t len = sizeof(cliaddr);
            sendto(socketfd_recv, (char*) &ACK,11,
                   0, (struct sockaddr *) &cliaddr, len);
        }



    }
    close(socketfd_recv);
    fclose(fp);

    //printf("Bye from recv\n");
    struct stat statfile;
    if(stat(filename, &statfile) < 0)
        return -1;
    return statfile.st_size;
}

/**
 * @brief 用于接收数据失败时断开RTP连接以及关闭UDP socket
 */
void terminateReceiver()
{
    close(socketfd_recv);
}




/**
 * @brief 用于接收数据并在接收完后断开RTP连接(优化版本的RTP)
 * @param filename 用于接收数据的文件名
 * @return >0表示接收完成后到数据的字节数 -1表示出现其他错误
 */
int recvMessageOpt(char* filename)
{
    FILE* fp = fopen(filename, "w");
    if(fp == NULL)
        return -1;

    memset(packet_cache, 0, sizeof(packet_cache));
    memset(cached, 0, sizeof(cached));

    //timer
    int timer_fd;
    struct itimerspec timer_spec;
    timer_spec.it_value.tv_sec = 10;
    timer_spec.it_value.tv_nsec = 0;
    timer_spec.it_interval.tv_sec = 0;
    timer_spec.it_interval.tv_nsec = 0;

    timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC | TFD_NONBLOCK);
    timerfd_settime(timer_fd,0,&timer_spec,NULL);


    while(1)
    {
        char timer_buffer[10] = { 0 };
        int read_num = read(timer_fd,timer_buffer,8);
        if(read_num > 0)
            return -1;


        //receive rtp packet with no block
        //printf("recv_base = %d\n",recv_base);
        int rec = recv_packet(MSG_DONTWAIT);
        if(rec > 0)
        {
            timerfd_settime(timer_fd,0,&timer_spec,NULL);
            //printf("rec = %d\n",rec);
            rtp_packet_t recv_p = *(rtp_packet_t*)recv_buffer_re;
            if(recv_p.rtp.type == RTP_DATA)
            {
                //printf("data\n\n");
                uint32_t Check = recv_p.rtp.checksum;
                recv_p.rtp.checksum = 0;
                int data_len = recv_p.rtp.length;
                if(Check == compute_checksum((char*)&recv_p,11 + data_len))
                {
                    //printf("CHECK\n\n");
                    if(recv_base + window_recv > recv_p.rtp.seq_num &&
                       recv_base <= recv_p.rtp.seq_num )
                    {
                        //printf("seq=%d\n",recv_p.rtp.seq_num);
                        //seq_num legal
                        //cache
                        packet_cache[recv_p.rtp.seq_num] = recv_p;
                        cached[recv_p.rtp.seq_num] = 1;
                    }
                    rtp_header_t ACK = make_rtp_header(RTP_ACK, 0, recv_p.rtp.seq_num, 1);
                    socklen_t len = sizeof(cliaddr);
                    sendto(socketfd_recv, (char*) &ACK,11,
                           0, (struct sockaddr *) &cliaddr, len);
                }

            }
            else if(recv_p.rtp.type == RTP_END && recv_p.rtp.seq_num <= recv_base)
            {
                //END
                int Check = recv_p.rtp.checksum;
                recv_p.rtp.checksum = 0;
                int data_len = recv_p.rtp.length;
                if(Check == compute_checksum(&recv_p,11 + data_len))
                {
                    rtp_header_t ACK = make_rtp_header(RTP_ACK, 0, recv_base, 1);
                    socklen_t len = sizeof(cliaddr);
                    sendto(socketfd_recv, (char *) &ACK, 11,
                           0, (struct sockaddr *) &cliaddr, len);
                    break;
                }
            }

            while(cached[recv_base] == 1)
            {
                fwrite((char*)(packet_cache[recv_base].payload), 1,
                       packet_cache[recv_base].rtp.length, fp);
                recv_base += 1;
            }


        }



    }
    close(socketfd_recv);
    fclose(fp);

    //printf("Bye from recv\n");
    struct stat statfile;
    if(stat(filename, &statfile) < 0)
        return -1;
    return statfile.st_size;
}



