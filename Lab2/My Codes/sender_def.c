#include "sender_def.h"
#include "util.c"
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

uint32_t send_base = 0;
uint32_t next_seq_num = 0;
int window = 0;
char acked[100000] = { 0 };
int socketfd;
struct sockaddr_in servaddr;
char send_buffer[20480] = { 0 };
char recv_buffer[20480] = { 0 };


int build_connection(const char* receiver_ip, uint16_t receiver_port);
rtp_header_t make_rtp_header(int type, int length, uint32_t seq_num, int flag);
int rtp_send(char* data, unsigned int size);
int rtp_recv(int timeout, int usec, int flag);
int file_statics(const char* filename);
rtp_packet_t make_rtp_packet(int type, uint32_t seq_num, FILE* f, int* len);
void send_again(FILE* f);

//---------------------
int build_connection(const char* receiver_ip, uint16_t receiver_port)
{
    socketfd = socket(AF_INET,SOCK_DGRAM,0);
    if(socketfd < 0)
        return -1;
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    inet_pton(AF_INET, receiver_ip, &servaddr.sin_addr);
    servaddr.sin_port = htons(receiver_port);
    return 0;
}

rtp_header_t make_rtp_header(int type, int length, uint32_t seq_num, int flag)
{
    rtp_header_t Header;
    Header.type = type;
    Header.length = length;
    Header.seq_num = seq_num;
    Header.checksum = 0;
    if(flag)
        Header.checksum = compute_checksum(&Header, sizeof(rtp_header_t));
    return Header;
}

int rtp_send(char* data, unsigned int size)
{
    memset(send_buffer, 0, sizeof(send_buffer));
    memcpy(send_buffer, data, size);
    int re = sendto(socketfd, send_buffer, size, 0,
           (struct sockaddr*)&servaddr,sizeof(servaddr));
    return re;
}

int rtp_recv(int timeout, int usec, int flag)
{
    memset(recv_buffer, 0 ,sizeof(recv_buffer));
    struct timeval time_out;
    if(timeout)
    {//set time out
        time_out.tv_sec = 0;
        time_out.tv_usec = usec;
        if (setsockopt(socketfd, SOL_SOCKET, SO_RCVTIMEO,
                       &time_out, sizeof(time_out)) == -1) {
            perror("setsockopt failed:");
        }
    }
    //receive
    int re = recvfrom(socketfd,recv_buffer,sizeof(recv_buffer),
                      flag,NULL,NULL);
    if(timeout) {
        //cancel time out
        time_out.tv_usec = 0;
        setsockopt(socketfd, SOL_SOCKET, SO_RCVTIMEO,
                   &time_out, sizeof(time_out));
    }
    return re;
}

int file_statics(const char* filename)
{
    struct stat statbuf;
    stat(filename, &statbuf);
    return (statbuf.st_size / 1461) + 1;
}

rtp_packet_t make_rtp_packet(int type, uint32_t seq_num, FILE* f, int* len)
{
    char data[1461] = { 0 };
    int length = fread(data, 1, 1461, f);
    (*len) = length;
    rtp_header_t Head = make_rtp_header(type, length, seq_num, 0);
    rtp_packet_t packet;
    packet.rtp = Head;
    memset(packet.payload, 0, sizeof(packet.payload));
    memcpy(packet.payload, data, length);
    packet.rtp.checksum = compute_checksum(&packet, 11 + length);
    return packet;
}

void send_again(FILE* f)
{
    for(uint32_t p = send_base; p < next_seq_num; p++)
    {
        if(acked[p] == 0)
        {
            fseek(f, p * 1461, SEEK_SET);
            int length = 0;
            rtp_packet_t packet = make_rtp_packet(RTP_DATA,p, f,&length);
            //sendto
            rtp_send((char*)&packet, length + 11);
        }
    }
}


/**
 * @brief 用于建立RTP连接
 * @param receiver_ip receiver的IP地址
 * @param receiver_port receiver的端口
 * @param window_size window大小
 * @return -1表示连接失败，0表示连接成功
 **/
int initSender(const char* receiver_ip, uint16_t receiver_port, uint32_t window_size)
{
    //set window
    window = window_size;

    //build start rtp_header
    int start_seq = rand();
    rtp_header_t Header = make_rtp_header(RTP_START, 0, start_seq, 1);

    //build connection
    if(build_connection(receiver_ip, receiver_port) < 0)
    {
        close(socketfd);
        return -1;
    }

    //send start rtp_header
    int send_num = rtp_send((char*)&Header, sizeof(rtp_header_t));
    if(send_num < 0)
    {
        close(socketfd);
        return -1;
    }

    //set timeout and wait for ACK
    int recv_num = rtp_recv(1, 100000, 0);
    if(recv_num < sizeof(rtp_header_t))
    {
        //time out or error exists
        rtp_header_t End = make_rtp_header(RTP_END, 0, 0, 1);
        rtp_send((char*)&End, sizeof(rtp_header_t));
        close(socketfd);
        return -1;
    }
    rtp_header_t recvHeader = *(rtp_header_t*)recv_buffer;
    uint32_t check = recvHeader.checksum;
    recvHeader.checksum = 0;
    if(recvHeader.seq_num != start_seq || recvHeader.type != RTP_ACK ||
    check != compute_checksum(&recvHeader, sizeof(rtp_header_t)))
    {
        //ACK error
        rtp_header_t End = make_rtp_header(RTP_END, 0, 0, 1);
        rtp_send((char*)&End, sizeof(rtp_header_t));
        close(socketfd);
        return -1;
    }

    //connect successfully
    //printf("Yes! from send init\n");
    return 0;

}


/**
 * @brief 用于发送数据
 * @param message 要发送的文件名
 * @return -1表示发送失败，0表示发送成功
 **/
int sendMessage(const char* message)
{

    FILE* fp = fopen(message,"r");
    int seg_len = file_statics(message);


    //printf("Seg_len = %d\n",seg_len);
    //timer
    int timer_fd;
    struct itimerspec timer_spec;
    timer_spec.it_value.tv_sec = 0;
    timer_spec.it_value.tv_nsec = 100000000;
    timer_spec.it_interval.tv_sec = 0;
    timer_spec.it_interval.tv_nsec = 0;

    timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC | TFD_NONBLOCK);

    int first = 1;
    while(1)
    {
        if(send_base >= seg_len)
            break;
        //send
        if(next_seq_num < send_base + window && next_seq_num >= send_base)
        {
            fseek(fp, next_seq_num * 1461, SEEK_SET);
            int length = 0;
            rtp_packet_t packet = make_rtp_packet(RTP_DATA,next_seq_num,fp,&length);
            //sendto
            rtp_send((char*)&packet, length + 11);
            //next++
            next_seq_num++;
            if(first)
            {
                first = 0;
                //start timer
                timerfd_settime(timer_fd,0,&timer_spec,NULL);
            }
        }

        //receive with no block
        int send_base_flag = 0;
        int recv_len = rtp_recv(0,0,MSG_DONTWAIT);
        int cnt = 0;
        while(recv_len >= 11)
        {
            //处理ACK
            rtp_header_t ack = *(rtp_header_t*)(recv_buffer + cnt * 11);
            uint32_t check = ack.checksum;
            ack.checksum = 0;
            if(ack.type == RTP_ACK && check == compute_checksum(&ack, sizeof(rtp_header_t)))
            {
                //seq_num ?= 0
                //for
                // ack.seq_num为期望的下一个包的seq_num
                if(send_base < ack.seq_num)
                {
                    send_base_flag = 1;
                    send_base = ack.seq_num;
                }
            }
            cnt++;
            recv_len -= 11;
        }

        if(send_base_flag)
            //restart timer
            timerfd_settime(timer_fd,0,&timer_spec,NULL);

        //定时器是否超时
        char timer_buffer[10] = { 0 };
        int read_num = read(timer_fd,timer_buffer,8);
        if(read_num > 0)
        {
            //超时
            //重传
            //send_again();
            next_seq_num = send_base;
            //restart timer
            timerfd_settime(timer_fd,0,&timer_spec,NULL);
        }

    }

    timer_spec.it_value.tv_nsec = 0;
    timerfd_settime(timer_fd,0,&timer_spec,NULL);
    fclose(fp);
    //printf("Yes from sender\n");
    return 0;
}


/**
 * @brief 用于断开RTP连接以及关闭UDP socket
 **/
void terminateSender()
{
    //printf("send_base = %d\n",send_base);
    rtp_header_t Header = make_rtp_header(RTP_END, 0, send_base, 1);
    rtp_send((char*)&Header, sizeof(rtp_header_t));
    int recv_num = rtp_recv(1,100000,0);
    close(socketfd);

    //printf("YT1\n");
    if(recv_num < 11)
        return;
    //printf("YT2\n");
    rtp_header_t recvHeader = *(rtp_header_t*)recv_buffer;
    uint32_t check = recvHeader.checksum;
    recvHeader.checksum = 0;
    if(recvHeader.type != RTP_ACK ||
       check != compute_checksum(&recvHeader, sizeof(rtp_header_t)))
        return;
    //printf("YT\n");
    //printf("Bye from sender\n");
}


/**
 * @brief 用于发送数据(优化版本的RTP)
 * @param message 要发送的文件名
 * @return -1表示发送失败，0表示发送成功
 **/
int sendMessageOpt(const char* message)
{
    FILE* fp = fopen(message,"r");
    int seg_len = file_statics(message);

    memset(acked, 0, sizeof(acked));

    //timer
    int timer_fd;
    struct itimerspec timer_spec;
    timer_spec.it_value.tv_sec = 0;
    timer_spec.it_value.tv_nsec = 100000000;
    timer_spec.it_interval.tv_sec = 0;
    timer_spec.it_interval.tv_nsec = 0;

    timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC | TFD_NONBLOCK);

    int first = 1;
    while(1)
    {
        if (send_base >= seg_len)
            break;

        if (next_seq_num < send_base)
            next_seq_num = send_base;
        //send
        if (next_seq_num < send_base + window)
        {
            fseek(fp, next_seq_num * 1461, SEEK_SET);
            int length = 0;
            rtp_packet_t packet = make_rtp_packet(RTP_DATA, next_seq_num, fp, &length);
            //sendto
            rtp_send((char *) &packet, length + 11);
            //next++
            next_seq_num++;
            if (first)
            {
                first = 0;
                //start timer
                timerfd_settime(timer_fd, 0, &timer_spec, NULL);
            }
        }

        //receive with no block
        int send_base_flag = 0;
        int recv_len = rtp_recv(0, 0, MSG_DONTWAIT);
        int cnt = 0;
        while (recv_len >= 11)
        {
            //处理ACK
            rtp_header_t ack = *(rtp_header_t *) (recv_buffer + cnt * 11);
            uint32_t check = ack.checksum;
            ack.checksum = 0;
            if (ack.type == RTP_ACK && check == compute_checksum(&ack, sizeof(rtp_header_t)))
            {
                //seq_num
                acked[ack.seq_num] = 1;
            }
            cnt++;
            recv_len -= 11;
        }

        //change send_base
        while (acked[send_base] == 1)
        {
            send_base++;
            send_base_flag = 1;
        }


        if (send_base_flag)
            //restart timer
            timerfd_settime(timer_fd, 0, &timer_spec, NULL);

        //定时器是否超时
        char timer_buffer[10] = {0};
        int read_num = read(timer_fd, timer_buffer, 8);
        if (read_num > 0)
        {
            //超时
            //重传
            send_again(fp);
            //restart timer
            timerfd_settime(timer_fd, 0, &timer_spec, NULL);
        }
    }

    //printf("Yes!\n");
    return 0;
}
