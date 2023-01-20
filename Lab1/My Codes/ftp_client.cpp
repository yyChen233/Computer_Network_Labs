#include <iostream>
#include <cstdlib>
#include <sys/socket.h>
#include <cstring>
#include <string>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstddef>

using namespace std;

// Global variables
const char prompt[] = "Client> ";
const int MAXLINE = 1024;
const int MAXARGS = 128;
const int MAXFILELEN = 4080;
char cmdline[MAXLINE];
char array[MAXLINE];
int socketfd;
bool isSocket = 0;
bool isAuth = 0;

class cmdline_tokens
{
public:
    int argc;
    char* argv[MAXARGS];
    char* infile;
    char* outfile;
    enum builtin
    {
        BUILTIN_OPEN,
        BUILTIN_AUTH,
        BUILTIN_LS,
        BUILTIN_GET,
        BUILTIN_PUT,
        BUILTIN_QUIT,
        BUILTIN_NONE
    }builtins;

};
cmdline_tokens tok;

struct FTPpack {
    byte m_protocol[6]; /* protocol magic number (6 bytes) */
    byte m_type;                          /* type (1 byte) */
    byte m_status;                      /* status (1 byte) */
    uint32_t m_length;                    /* length (4 bytes) in Big endian*/
} __attribute__ ((packed));

char buf_send[MAXFILELEN] = {0};
char buf_read[MAXFILELEN] = {0};
char buf_file[MAXFILELEN] = {0};

//evaluate the input command line
void eval(const char* cmdline);
//parse the command line and build the argv array
int parseLine(const char* cmdline, cmdline_tokens* tok);
//open a connection
int open(cmdline_tokens* tok);
//close the connection
void closefd(void);
//quit the client process
void quit(void);
//authentication
int authen(cmdline_tokens* tok);
//ls command
int ls(cmdline_tokens* tok);
//put command
int myPut(cmdline_tokens* tok);
//get command
int myGet(cmdline_tokens* tok);
//check the FTP pack
int isMyFTP(char* p);


int main(int argc, char ** argv) {

    while(1)
    {
        cout << prompt;
        if((fgets(cmdline, MAXLINE, stdin) == NULL) && ferror(stdin))
        {
            fprintf(stdout, "fgets error\n");
            exit(-1);
        }

        if(feof(stdin))
        {
            printf("\n");
            exit(0);
        }

        cmdline[strlen(cmdline) - 1] = '\0';
        eval(cmdline);

        fflush(stdout);
    }

    return 0;
}

void eval(const char* cmdline)
{

    int bg = parseLine(cmdline, &tok);

    if(bg == -1)/* error */
    {
        return;
    }
    if(tok.argv[0] == NULL) /* ignore empty lines*/
    {
        return;
    }
    if(tok.builtins == cmdline_tokens::BUILTIN_QUIT)
    {
        //TODO: close the connection
        quit();
        exit(0);
    }
    else if(tok.builtins == cmdline_tokens::BUILTIN_OPEN)
    {
        //TODO: open the connection
        if(isSocket == 1)
        {
            fprintf(stdout, "You have opened a connection\n");
            return;
        }
        if(open(&tok) == -1)
            closefd();
    }
    else if(tok.builtins == cmdline_tokens::BUILTIN_AUTH)
    {
        //TODO: classify the username and password
        if(!isAuth)
        {
            int a;
            if ((a = authen(&tok)) == -1)
                closefd();
            if(a == 0)
                isAuth = 1;
        }
        //quit();
    }
    else if(tok.builtins == cmdline_tokens::BUILTIN_GET)
    {
        //TODO: get files from the server
        if(myGet(&tok) == -1)
            closefd();
    }
    else if(tok.builtins == cmdline_tokens::BUILTIN_PUT)
    {
        //TODO: put files to the server
        if(myPut(&tok) == -1)
            closefd();
    }
    else if(tok.builtins == cmdline_tokens::BUILTIN_LS)
    {
        //TODO: ls the dictionary of the server
        if(ls(&tok) == -1)
            closefd();
    }
    else if(tok.builtins == cmdline_tokens::BUILTIN_NONE)
    {
        fprintf(stdout, "Error: Command is not correct\n");
    }

    return;
}

int parseLine(const char* cmdline, cmdline_tokens* tok)
{
    const char delimiters[10] = " \t\r\n";
    char* buf = array;
    char* next;
    char* endbuf;

    if(cmdline == NULL)
    {
        fprintf(stdout, "Error: Command line is NULL\n");
        return -1;
    }

    strncpy(buf, cmdline, MAXLINE);
    endbuf = buf + strlen(buf);

    tok->infile = tok->outfile =NULL;
    tok->argc = 0;

    while(buf < endbuf)
    {

        buf += strspn(buf, delimiters);
        if(buf >= endbuf)
            break;

        next = buf + strcspn(buf, delimiters);
        *next = '\0';

        tok->argv[tok->argc++] = buf;
        if(tok->argc >= MAXARGS - 1)
            break;

        buf = next + 1;
    }

    tok->argv[tok->argc] = NULL;

    if(tok->argc == 0)
        return 1;

    if(!strcmp(tok->argv[0], "quit"))
    {
        tok->builtins = cmdline_tokens::BUILTIN_QUIT;
    }
    else if(!strcmp(tok->argv[0],"open"))
    {
        tok->builtins = cmdline_tokens::BUILTIN_OPEN;
    }
    else if(!strcmp(tok->argv[0],"put"))
    {
        tok->builtins = cmdline_tokens::BUILTIN_PUT;
    }
    else if(!strcmp(tok->argv[0],"get"))
    {
        tok->builtins = cmdline_tokens::BUILTIN_GET;
    }
    else if(!strcmp(tok->argv[0],"auth"))
    {
        tok->builtins = cmdline_tokens::BUILTIN_AUTH;
    }
    else if(!strcmp(tok->argv[0],"ls"))
    {
        tok->builtins = cmdline_tokens::BUILTIN_LS;
    }
    else
    {
        tok->builtins = cmdline_tokens::BUILTIN_NONE;
    }

    return 0;
}

int open(cmdline_tokens* tok)
{
    struct sockaddr_in servaddr;
    if(isSocket == 1)
    {
        fprintf(stdout, "You have already opened a connection.\n");
        return -2;
    }

    if(tok->argc != 3)
    {
        fprintf(stdout, "too little parameters\n");
        return -2;
    }
    socketfd = socket(AF_INET, SOCK_STREAM, 0);
    if(socketfd < 0)
    {
        fprintf(stdout, "cannot socket\n");
        return -1;
    }


    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(atoi(tok->argv[2]));
    inet_pton(AF_INET, tok->argv[1], &servaddr.sin_addr);



    if(connect(socketfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
    {
        fprintf(stdout, "connect error\n");
        fflush(stdout);
        return -1;
    }
    isSocket = 1;


    FTPpack requestPack;
    strcpy((char*)requestPack.m_protocol,"\xe3myft");
    requestPack.m_protocol[5] = (byte)'p';
    requestPack.m_type = (byte)0xA1;
    requestPack.m_status = (byte)0;
    requestPack.m_length = htonl(12);

    memset(buf_send,0,sizeof(buf_send));
    memset(buf_read,0,sizeof(buf_read));

    memcpy(buf_send, &requestPack, sizeof(FTPpack));
    if(send(socketfd, buf_send, sizeof(FTPpack), 0) < 0)
    {
        fprintf(stdout, "send error\n");
        return -1;
    }

    if(recv(socketfd, buf_read, sizeof(FTPpack), 0) < 0)
    {
        fprintf(stdout, "recv error\n");
        return -1;
    }

    FTPpack* revcPack = (FTPpack*) buf_read;
    if(!isMyFTP((char*)revcPack->m_protocol))
    {
        fprintf(stdout, "protocol error\n");
        return -1;
    }

    if(revcPack->m_status != (byte)1)
    {
        //TODO: error, close the connection
        cout<<"No!"<<endl;
        return -1;
    }

    cout<<"You have successfully connected!"<<endl;
    return 0;

}

void closefd(void)
{
    if(isSocket = 1)
        close(socketfd);
    isSocket = 0;
    return;
}

void quit(void)
{
    if(isSocket == 0)
        return;

    FTPpack requestPack;
    strcpy((char*)requestPack.m_protocol,"\xe3myft");
    requestPack.m_protocol[5] = (byte)'p';
    requestPack.m_type = (byte)0xAB;
    requestPack.m_status = (byte)0;
    requestPack.m_length = htonl(12);

    memset(buf_send,0,sizeof(buf_send));
    memset(buf_read,0,sizeof(buf_read));

    memcpy(buf_send, &requestPack, sizeof(FTPpack));

    send(socketfd, buf_send, sizeof(FTPpack), 0);

    recv(socketfd, buf_read, sizeof(FTPpack), 0);
    FTPpack* revcPack = (FTPpack*) buf_read;
    if(!isMyFTP((char*)revcPack->m_protocol))
    {
        fprintf(stdout, "protocol error\n");
        return;
    }
    if(revcPack->m_type == (byte)0xAC)
        closefd();

    cout<<"Goodbye, wish to see you next time!"<<endl;
    return;
}

int authen(cmdline_tokens* tok)
{
    if(isSocket == 0)
    {
        fprintf(stdout, "You haven't open a connection yet\n");
        return -2;
    }
    if(isAuth == 1)
    {
        fprintf(stdout, "You have already authenticated.\n");
        return -2;
    }

    if(tok->argc != 3)
    {
        fprintf(stdout, "too little parameters\n");
        return -2;
    }

    char data[MAXLINE]={0};
    strcpy(data,(const char*)tok->argv[1]);
    data[strlen(tok->argv[1])] = ' ';
    strcpy(data + strlen(tok->argv[1]) + 1 , (const char*)tok->argv[2]);


    FTPpack requestPack;
    strcpy((char*)requestPack.m_protocol,"\xe3myft");
    requestPack.m_protocol[5] = (byte)'p';
    requestPack.m_type = (byte)0xA3;
    requestPack.m_status = (byte)0;
    requestPack.m_length = htonl(12 + strlen(data) + 1);

    memset(buf_send,0,sizeof(buf_send));
    memset(buf_read,0,sizeof(buf_read));

    memcpy(buf_send, &requestPack, sizeof(FTPpack));
    memcpy(buf_send + sizeof(FTPpack), &data, strlen(data) + 1);

    int a;
    if(((a=send(socketfd, buf_send, sizeof(FTPpack) + strlen(data) + 1, 0) )< 0))
    {
        fprintf(stdout, "send error\n");
        return -1;
    }
    //cout<<a<<endl;

    if(recv(socketfd, buf_read, sizeof(FTPpack), 0) < 0)
    {
        fprintf(stdout, "recv error\n");
        return -1;
    }
    FTPpack* revcPack = (FTPpack*) buf_read;
    if(!isMyFTP((char*)revcPack->m_protocol))
    {
        fprintf(stdout, "protocol error\n");
        return -1;
    }
    if(revcPack->m_type != (byte)0xA4)
    {
        fprintf(stdout, "error\n");
        return -1;
    }


    if(revcPack->m_status == (byte)0)
    {
        //TODO: close the connection
        cout<<"No!"<<endl;
        return -1;
    }

    cout<<"You have successfully authened!"<<endl;
    return 0;


}

int ls(cmdline_tokens* tok)
{
    if(isSocket == 0)
    {
        fprintf(stdout, "You haven't opened a connection yet.\n");
        return -2;
    }
    else if(isAuth == 0)
    {
        fprintf(stdout, "You haven't logged in.\n");
        return -2;
    }
    
    else if(tok->argc > 1)
    {
        fprintf(stdout, "too many parameters.\n");
        return -2;
    }

    FTPpack requestPack;
    strcpy((char*)requestPack.m_protocol,"\xe3myft");
    requestPack.m_protocol[5] = (byte)'p';
    requestPack.m_type = (byte)0xA5;
    requestPack.m_status = (byte)0;
    requestPack.m_length = htonl(12);

    int listLen;
    memset(buf_send,0,sizeof(buf_send));
    memset(buf_read,0,sizeof(buf_read));

    memcpy(buf_send, &requestPack, sizeof(FTPpack));

    if(send(socketfd, buf_send, sizeof(FTPpack), 0) < 0)
    {
        fprintf(stdout, "send error\n");
        return -1;
    }

    if(recv(socketfd, buf_read, 2060, 0) < 0)
    {
        fprintf(stdout, "recv error\n");
        return -1;
    }


    FTPpack* revcPack = (FTPpack*) buf_read;
    if(!isMyFTP((char*)revcPack->m_protocol))
    {
        fprintf(stdout, "protocol error\n");
        return -1;
    }
    if(revcPack->m_type != (byte)0xA6)
    {
        fprintf(stdout, "error\n");
        return -1;
    }

    cout<<"Get data successfully!"<<endl;

    listLen = ntohl(revcPack->m_length) - sizeof(FTPpack) - 1;
    //cout<<listLen<<endl;
    char* lists = buf_read + sizeof(FTPpack);

    cout<<"------------lists of files below------------"<<endl;
    for(int i = 0; i < listLen; i++)
    {
        cout<<lists[i];
    }
    cout<<"------------lists of files above------------"<<endl;
    return 0;

}

int myPut(cmdline_tokens* tok)
{
    if(isSocket == 0)
    {
        fprintf(stdout, "You haven't opened a connection yet.\n");
        return -2;
    }
    else if(isAuth == 0)
    {
        fprintf(stdout, "You haven't logged in.\n");
        return -2;
    }

    if(tok->argc != 2)
    {
        fprintf(stdout, "parameter number error\n");
        return -2;
    }

    const char* filename = tok->argv[1];
    cout<<"File name : "<<filename<<endl;
    FILE* fp = fopen(filename, "r");
    if(fp == NULL)
    {
        fprintf(stdout, "there is no file named %s\n",filename);
        return -2;
    }

    FTPpack requestPack;
    strcpy((char*)requestPack.m_protocol,"\xe3myft");
    requestPack.m_protocol[5] = (byte)'p';
    requestPack.m_type = (byte)0xA9;
    requestPack.m_status = (byte)0;
    requestPack.m_length = htonl(12 + strlen(filename) + 1);

    memset(buf_send,0,sizeof(buf_send));
    memset(buf_read,0,sizeof(buf_read));

    memcpy(buf_send, &requestPack, sizeof(FTPpack));
    memcpy(buf_send + sizeof(FTPpack), filename, strlen(filename) + 1);

    if(send(socketfd, buf_send, sizeof(FTPpack) + strlen(filename) + 1, 0) < 0)
    {
        fprintf(stdout, "send error\n");
        return -1;
    }

    if(recv(socketfd, buf_read, sizeof(FTPpack), 0) < 0)
    {
        fprintf(stdout, "recv error\n");
        return -1;
    }
    FTPpack* revcPack = (FTPpack*) buf_read;
    if(!isMyFTP((char*)revcPack->m_protocol))
    {
        fprintf(stdout, "protocol error\n");
        return -1;
    }
    if(revcPack->m_type != (byte)0xAA)
    {
        fprintf(stdout, "error\n");
        return -1;
    }

    memset(buf_send,0,sizeof(buf_send));
    memset(buf_file, 0 ,sizeof(buf_file));
    int nCount;
    int fCount = 0;
    int first = 0;
    while( (nCount = fread(buf_file, 1, MAXFILELEN, fp)) > 0 ){
        fCount += nCount;
    }
    cout<<"FileLen : "<<fCount<<endl;
    requestPack.m_type = (byte)0xFF;
    requestPack.m_length = htonl(12 + fCount);
    fclose(fp);

    FILE* fp1 = fopen(filename, "r");

    memcpy(buf_send,&requestPack, sizeof(FTPpack));
    send(socketfd, buf_send, 12, 0);

    nCount = fCount = 0;
    first = 0;
    while( (nCount = fread(buf_file, 1, MAXFILELEN, fp1)) > 0 ){
        fCount += nCount;
        first ++;
        memcpy(buf_send , buf_file, nCount);
        send(socketfd, buf_send, nCount, 0);
    }
    fclose(fp1);

    cout<<"Transform finished!"<<endl;


    return 0;
}

int myGet(cmdline_tokens* tok)
{
    if(isSocket == 0)
    {
        fprintf(stdout, "You haven't opened a connection yet.\n");
        return -2;
    }
    else if(isAuth == 0)
    {
        fprintf(stdout, "You haven't logged in.\n");
        return -2;
    }

    if(tok->argc != 2)
    {
        fprintf(stdout, "parameter number error\n");
        return -2;
    }

    const char* filename = tok->argv[1];

    FTPpack requestPack;
    strcpy((char*)requestPack.m_protocol,"\xe3myft");
    requestPack.m_protocol[5] = (byte)'p';
    requestPack.m_type = (byte)0xA7;
    requestPack.m_status = (byte)0;
    requestPack.m_length = htonl(12 + strlen(filename) + 1);

    memset(buf_send,0,sizeof(buf_send));
    memset(buf_read,0,sizeof(buf_read));

    memcpy(buf_send, &requestPack, sizeof(FTPpack));
    memcpy(buf_send + sizeof(FTPpack), filename, strlen(filename) + 1);

    if(send(socketfd, buf_send, sizeof(FTPpack) + strlen(filename) + 1, 0) < 0)
    {
        fprintf(stdout, "send error\n");
        return -1;
    }
    if(recv(socketfd, buf_read, 12, 0) < 0)
    {
        fprintf(stdout, "recv error\n");
        return -1;
    }

    FTPpack* revcPack = (FTPpack*) buf_read;
    if(!isMyFTP((char*)revcPack->m_protocol))
    {
        fprintf(stdout, "protocol error\n");
        return -1;
    }
    if(revcPack->m_type != (byte)0xA8)
    {
        fprintf(stdout, "error\n");
        return -1;
    }
    if(revcPack->m_status == (byte)0)
    {
        fprintf(stdout, "There is no file named %s in server's dictionary.\n", filename);
        return -2;
    }

    memset(buf_file, 0 ,sizeof(buf_file));
    FILE* fp = fopen(filename, "wb");
    cout<<"File name : "<<filename<<endl;

    int first = 0;
    int bread ;
    int cnt = 0;
    int fileLen = 0;
    while(1)
    {
        bread = recv(socketfd, buf_file, MAXFILELEN, 0);
        cnt += bread;
        if(cnt == 0)
            continue;
        first ++;
        if(first == 1)
        {
            FTPpack recvp = *(FTPpack*)buf_file;
            if(recvp.m_type == (byte)0xFF)
            {
                fileLen = ntohl(recvp.m_length) - 12;
                cout<< "FileLen : "<<fileLen <<endl;
            }
            else
                cout<<"error\n"<<endl;
            int acnt = fwrite(buf_file + sizeof(FTPpack), 1, bread - 12, fp);
            //cout<<acnt<<endl;
        }
        else
        {
            int acnt = fwrite(buf_file, 1, bread, fp);
            //cout<<acnt<<endl;
        }
        if(cnt == fileLen + sizeof(FTPpack))
            break;
    }
    fclose(fp);
    cout<<"Transform finished!"<<endl;
    return 0;

}

int isMyFTP(char* p)
{
    if((p[0]=='\xe3'&&p[1]=='m'&&p[2]=='y'&&p[3]=='f'
        &&p[4]=='t'&&p[5]=='p'))
        return 1;
    return 0;
}