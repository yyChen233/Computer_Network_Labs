#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstddef>
#include <cstring>
#include <unistd.h>
using namespace std;

struct FTPpack {
    byte m_protocol[6]; /* protocol magic number (6 bytes) */
    byte m_type;                          /* type (1 byte) */
    byte m_status;                      /* status (1 byte) */
    uint32_t m_length;                    /* length (4 bytes) in Big endian*/
} __attribute__ ((packed));

const int MAXFILELEN = 4080;
char buffer_recv[MAXFILELEN] = {0};
char buffer_send[MAXFILELEN] = {0};
char buffer_file[MAXFILELEN] = {0};
char* filename;
int fileLen = 0;

int isMyFTP(char* p);

int isMyFTP(char* p)
{
    if((p[0]=='\xe3'&&p[1]=='m'&&p[2]=='y'&&p[3]=='f'
            &&p[4]=='t'&&p[5]=='p'))
        return 1;
    return 0;
}



int main(int argc, char ** argv)
{
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;
    addr.sin_port = htons(atoi(argv[2]));
    addr.sin_family = AF_INET; 
    inet_pton(AF_INET, argv[1], &addr.sin_addr);
    bind(sock, (struct sockaddr*)&addr, sizeof(addr));
    listen(sock, 128);
Here:
    int client = accept(sock, nullptr, nullptr);

    while(1)
    {
        memset(buffer_recv,0,sizeof(buffer_recv));
        memset(buffer_send,0,sizeof(buffer_send));

        int dd = recv(client, buffer_recv, sizeof(FTPpack), 0);
        FTPpack recvPack = *(FTPpack *) buffer_recv;

        if (isMyFTP((char*)recvPack.m_protocol))
        {
            if(recvPack.m_type == (byte)0xA1)
            {
                recvPack.m_type = (byte) 0xA2;
                recvPack.m_status = (byte) 1;
                memcpy(buffer_send, &recvPack, sizeof(FTPpack));

                send(client, buffer_send, sizeof(recvPack), 0);
            }
            else if(recvPack.m_type == (byte)0xA3)
            {
                int dataLen = ntohl(recvPack.m_length) - sizeof(FTPpack);
                recv(client, buffer_recv + sizeof(FTPpack), dataLen, 0);

                recvPack.m_type = (byte) 0xA4;
                recvPack.m_length = htonl(12);

                char* data = buffer_recv + sizeof(FTPpack);
                if(!strcmp(data,"user 123123\0"))
                {
                    recvPack.m_status = (byte)1;
                }
                else
                {
                    recvPack.m_status = (byte)0;
                }

                memcpy(buffer_send, &recvPack, sizeof(FTPpack));
                send(client, buffer_send, sizeof(FTPpack), 0);

                if(strcmp(data,"user 123123\0"))
                    goto Here;
            }
            else if(recvPack.m_type == (byte)0xAB)
            {
                recvPack.m_type = (byte) 0xAC;

                memcpy(buffer_send, &recvPack, sizeof(FTPpack));
                send(client, buffer_send, sizeof(recvPack), 0);

                goto Here;
            }
            else if(recvPack.m_type == (byte)0xA5)
            {
                recvPack.m_type = (byte)0xA6;

                char payload[2080] = {0};

                FILE* fp = popen("ls","r");
                int num = 0;
                char* temp = payload;
                while(fgets(temp, sizeof(temp), fp))
                {
                    num += strlen(temp) ;
                    temp += strlen(temp) ;
                }

                payload[num] = '\0';

                recvPack.m_length = htonl(12 + num + 1);
                memcpy(buffer_send, &recvPack, sizeof(FTPpack));
                memcpy(buffer_send + sizeof(FTPpack), &payload, num + 1);
                
                send(client, buffer_send, sizeof(FTPpack) + num + 1, 0);

            }
            else if(recvPack.m_type == (byte)0xA9)
            {
                int filenameLen = ntohl(recvPack.m_length) - sizeof(FTPpack);
                recv(client, buffer_recv + sizeof(FTPpack), filenameLen, 0);

                filename = buffer_recv + sizeof(FTPpack);
                FILE* fp = fopen(filename,"wb");

                recvPack.m_type = (byte)0xAA;
                recvPack.m_length = htonl(12);
                memcpy(buffer_send, &recvPack, sizeof(FTPpack));
                send(client, buffer_send, sizeof(FTPpack), 0);
                memset(buffer_file,0,sizeof(buffer_file));

                int first = 0;
                int bread ;
                int cnt = 0;
                while(1)
                {
                    bread = recv(client, buffer_file, MAXFILELEN, 0);
                    cnt += bread;

                    first ++;
                    if(first == 1)
                    {
                        FTPpack recvp = *(FTPpack*)buffer_file;
                        if(recvp.m_type == (byte)0xFF)
                        {
                            fileLen = ntohl(recvp.m_length) - 12;
                            //cout<< fileLen <<endl;
                        }
                        else
                            cout<<"error\n"<<endl;
                        int acnt = fwrite(buffer_file + sizeof(FTPpack), 1, bread - 12, fp);
                        //cout<<acnt<<endl;
                    }
                    else
                    {
                        int acnt = fwrite(buffer_file, 1, bread, fp);
                        //cout<<acnt<<endl;
                    }
                    if(cnt == fileLen + sizeof(FTPpack))
                        break;
                }


                fclose(fp);

                //cout<<"Yes!"<<endl;
            }
            else if(recvPack.m_type == (byte)0xA7)
            {
                int filenameLen = ntohl(recvPack.m_length) - sizeof(FTPpack);
                recv(client, buffer_recv + sizeof(FTPpack), filenameLen, 0);

                recvPack.m_type = (byte)0xA8;
                recvPack.m_length = htonl(12);
                filename = buffer_recv + sizeof(FTPpack);

                FILE* fp = fopen(filename,"rb");

                if(fp == NULL)
                {
                    recvPack.m_status = (byte)0;
                    memcpy(buffer_send, &recvPack, sizeof(FTPpack));
                    send(client, buffer_send, sizeof(FTPpack), 0);
                }
                else
                {
                    recvPack.m_status = (byte)1;
                    memcpy(buffer_send, &recvPack, sizeof(FTPpack));
                    send(client, buffer_send, sizeof(FTPpack), 0);

                    memset(buffer_send,0,sizeof(buffer_send));
                    memset(buffer_file, 0 ,sizeof(buffer_file));
                    int nCount;
                    int fCount = 0;
                    while( (nCount = fread(buffer_file, 1, MAXFILELEN, fp)) > 0 ){
                        fCount += nCount;
                    }
                    //cout<<fCount<<endl;
                    fclose(fp);
                    FTPpack requestPack;
                    strcpy((char*)requestPack.m_protocol,"\xe3myft");
                    requestPack.m_protocol[5] = (byte)'p';
                    requestPack.m_type = (byte)0xFF;
                    requestPack.m_status = (byte)0;
                    requestPack.m_length = htonl(12 + fCount);
                    memcpy(buffer_send,&requestPack, sizeof(FTPpack));
                    send(client, buffer_send, 12, 0);

                    FILE* fp1 = fopen(filename, "r");

                    nCount = fCount = 0;
                    int first = 0;
                    while( (nCount = fread(buffer_file, 1, MAXFILELEN, fp1)) > 0 ){
                        fCount += nCount;
                        first ++;
                        memcpy(buffer_send , buffer_file, nCount);
                        send(client, buffer_send, nCount, 0);
                    }
                    fclose(fp1);

                    //cout<<"Yes!"<<endl;



                }
            }
        }
    }
}
