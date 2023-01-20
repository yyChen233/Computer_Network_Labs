 #include <gtest/gtest.h>
#include <unistd.h>
#include <iostream>
#include <stdint.h>
#include <thread>
#include <fcntl.h>
#include<cstring>
#include "sender_def.h"
#include "receiver_def.h"
int diff_file(char *f1, char *f2)
{
    FILE* fp1 = fopen(f1, "rb");
    FILE* fp2 = fopen(f2, "rb");
    char f1_buffer[65536];
    char f2_buffer[65536];
    while (1)
    {
        int size1 = fread((void*)f1_buffer, sizeof(char), 65536, fp1);
        int size2 = fread((void*)f2_buffer, sizeof(char), 65536, fp2);
        if (size1 != size2)
        return -1;
        for (int i = 0 ; i < size1; i++)
        {
            if (f1_buffer[i] != f2_buffer[i])
            return -1;
        }
        if (feof(fp1))
        {
            if (!feof(fp2))
                return -1;
            else return 1;
        } 
    }
}
void normal_sender(char *message, char* window_size)
{
    if (initSender("127.0.0.1", 12346, atoi(window_size)) == 0)
    {
        sendMessage(message);
        terminateSender();
    }
}

void normal_opt_sender(char *message, char* window_size)
{
    if (initSender("127.0.0.1", 12346, atoi(window_size)) == 0)
    {
        sendMessageOpt(message);
        terminateSender();
    }
}


void normal_receiver(char *filename, char* window_size)
{
    if (initReceiver(12346, atoi(window_size)) == 0)
    {
        recvMessage(filename);
        terminateReceiver();
    }
}

void normal_opt_receiver(char *filename, char* window_size)
{
    if (initReceiver(12346, atoi(window_size)) == 0)
    {
        recvMessageOpt(filename);
        terminateReceiver();
    }
}

void test_sender(char *message, char* window_size, char* prob, char* err)
{
    char dest[100] = "./test_sender 127.0.0.1 12346 ";
    strcat(dest, window_size);
    strcat(dest, " ");
    strcat(dest, message);
    strcat(dest, " ");
    strcat(dest, prob);
    strcat(dest, " ");
    strcat(dest, err);
    system(dest);
}


void test_opt_sender(char *message, char* window_size, char* prob, char* err)
{
    char dest[100] = "./test_opt_sender 127.0.0.1 12346 ";
    strcat(dest, window_size);
    strcat(dest, " ");
    strcat(dest, message);
    strcat(dest, " ");
    strcat(dest, prob);
    strcat(dest, " ");
    strcat(dest, err);
    system(dest);
}

void test_receiver(char *filename, char* window_size, char* prob, char* err)
{
    char dest[100] = "./test_receiver 12346 ";
    strcat(dest, window_size);
    strcat(dest, " ");
    strcat(dest, filename);
    strcat(dest, " ");
    strcat(dest, prob);
    strcat(dest, " ");
    strcat(dest, err);
    system(dest);
}


void test_opt_receiver(char *filename, char* window_size, char* prob, char* err)
{
    char dest[100] = "./test_opt_receiver 12346 ";
    strcat(dest, window_size);
    strcat(dest, " ");
    strcat(dest, filename);
    strcat(dest, " ");
    strcat(dest, prob);
    strcat(dest, " ");
    strcat(dest, err);
    system(dest);
}

int easy_test(char *message, char *filename, char* window_size)
{
    std::thread receiver_thread(normal_receiver, filename, window_size);
    usleep(10000);
    std::thread sender_thread(normal_sender, message, window_size);
    receiver_thread.join();
    sender_thread.join();
    int res = diff_file(message, filename);
    char dest[100] = "rm ";
    strcat(dest, filename);
    system(dest);
    return res;
}

int easy_opt_test(char *message, char *filename, char* window_size)
{
    std::thread receiver_thread(normal_opt_receiver, filename, window_size);
    usleep(10000);
    std::thread sender_thread(normal_opt_sender, message, window_size);
    receiver_thread.join();
    sender_thread.join();
    int res = diff_file(message, filename);
    char dest[100] = "rm ";
    strcat(dest, filename);
    system(dest);
    return res;
}

int sender_test(char *message, char *filename, char* window_size, char* prob, char* err)
{
    std::thread receiver_thread(test_receiver, filename, window_size, prob, err);
    usleep(10000);
    std::thread sender_thread(normal_sender, message, window_size);
    receiver_thread.join();
    sender_thread.join();
    int res = diff_file(message, filename);
    char dest[100] = "rm ";
    strcat(dest, filename);
    system(dest);
    return res;
}

int receiver_test(char *message, char *filename, char* window_size, char* prob, char* err)
{
    std::thread receiver_thread(normal_receiver, filename, window_size);
    usleep(10000);
    std::thread sender_thread(test_sender, message, window_size, prob, err);
    receiver_thread.join();
    sender_thread.join();
    int res = diff_file(message, filename);
    char dest[100] = "rm ";
    strcat(dest, filename);
    system(dest);
    return res;
}

int opt_sender_test(char *message, char *filename, char* window_size, char* prob, char* err)
{
    std::thread receiver_thread(test_opt_receiver, filename, window_size, prob, err);
    usleep(10000);
    std::thread sender_thread(normal_opt_sender, message, window_size);
    receiver_thread.join();
    sender_thread.join();
    int res = diff_file(message, filename);
    char dest[100] = "rm ";
    strcat(dest, filename);
    system(dest);
    return res;
}

int opt_receiver_test(char *message, char *filename, char* window_size, char* prob, char* err)
{
    std::thread receiver_thread(normal_opt_receiver, filename, window_size);
    usleep(10000);
    std::thread sender_thread(test_opt_sender, message, window_size, prob, err);
    receiver_thread.join();
    sender_thread.join();
    int res = diff_file(message, filename);
    char dest[100] = "rm ";
    strcat(dest, filename);
    system(dest);
    return res;
}
TEST(RTP, NO_FAILURE)
{
    char* message = (char*)"testdata";
    char* recvfile = (char*)"recvfile";
    char* ws = (char*)"32";
    EXPECT_EQ(easy_test(message, recvfile, ws), 1);
}

TEST(RTP, NO_FAILURE_OPT)
{
    char* message = (char*)"testdata";
    char* recvfile = (char*)"recvfile";
    char* ws = (char*)"32";
    EXPECT_EQ(easy_opt_test(message, recvfile, ws), 1);
}

TEST(RTP, RECEIVER_TEST_LOST_DATA)
{
    char* message = (char*)"testdata";
    char* recvfile = (char*)"recvfile";
    char* ws = (char*)"256";
    char* pb = (char*)"10";
    char* er = (char*)"1";
    EXPECT_EQ(receiver_test(message, recvfile, ws, pb, er), 1);
}

TEST(RTP, RECEIVER_TEST_DUP_DATA)
{
    char* message = (char*)"testdata";
    char* recvfile = (char*)"recvfile";
    char* ws = (char*)"256";
    char* pb = (char*)"10";
    char* er = (char*)"2";
    EXPECT_EQ(receiver_test(message, recvfile, ws, pb, er), 1);
}

TEST(RTP, RECEIVER_TEST_WRONG_CHKSUM_DATA)
{
    char* message = (char*)"testdata";
    char* recvfile = (char*)"recvfile";
    char* ws = (char*)"256";
    char* pb = (char*)"10";
    char* er = (char*)"3";
    EXPECT_EQ(receiver_test(message, recvfile, ws, pb, er), 1);
}

TEST(RTP, RECEIVER_TEST_REORDER_DATA)
{
    char* message = (char*)"testdata";
    char* recvfile = (char*)"recvfile";
    char* ws = (char*)"256";
    char* pb = (char*)"10";
    char* er = (char*)"4";
    EXPECT_EQ(receiver_test(message, recvfile, ws, pb, er), 1);
}

TEST(RTP, RECEIVER_TEST_MIXED_1)
{
    char* message = (char*)"testdata";
    char* recvfile = (char*)"recvfile";
    char* ws = (char*)"256";
    char* pb = (char*)"10";
    char* er = (char*)"73";
    EXPECT_EQ(receiver_test(message, recvfile, ws, pb, er), 1);
}

TEST(RTP, RECEIVER_TEST_MIXED_2)
{
    char* message = (char*)"testdata";
    char* recvfile = (char*)"recvfile";
    char* ws = (char*)"256";
    char* pb = (char*)"3";
    char* er = (char*)"73";
    EXPECT_EQ(receiver_test(message, recvfile, ws, pb, er), 1);
}

TEST(RTP, SENDER_TEST_LOST_ACK)
{
    char* message = (char*)"testdata";
    char* recvfile = (char*)"recvfile";
    char* ws = (char*)"256";
    char* pb = (char*)"10";
    char* er = (char*)"1";
    EXPECT_EQ(sender_test(message, recvfile, ws, pb, er), 1);
}

TEST(RTP, SENDER_TEST_DUP_ACK)
{
    char* message = (char*)"testdata";
    char* recvfile = (char*)"recvfile";
    char* ws = (char*)"256";
    char* pb = (char*)"10";
    char* er = (char*)"2";
    EXPECT_EQ(sender_test(message, recvfile, ws, pb, er), 1);
}

TEST(RTP, SENDER_TEST_WRONG_CHKSUM_ACK)
{
    char* message = (char*)"testdata";
    char* recvfile = (char*)"recvfile";
    char* ws = (char*)"256";
    char* pb = (char*)"10";
    char* er = (char*)"3";
    EXPECT_EQ(sender_test(message, recvfile, ws, pb, er), 1);
}

TEST(RTP, SENDER_TEST_REORDER_ACK)
{
    char* message = (char*)"testdata";
    char* recvfile = (char*)"recvfile";
    char* ws = (char*)"256";
    char* pb = (char*)"10";
    char* er = (char*)"4";
    EXPECT_EQ(sender_test(message, recvfile, ws, pb, er), 1);
}

TEST(RTP, SENDER_TEST_MIXED_1)
{
    char* message = (char*)"testdata";
    char* recvfile = (char*)"recvfile";
    char* ws = (char*)"256";
    char* pb = (char*)"10";
    char* er = (char*)"73";
    EXPECT_EQ(sender_test(message, recvfile, ws, pb, er), 1);
}

TEST(RTP, SENDER_TEST_MIXED_2)
{
    char* message = (char*)"testdata";
    char* recvfile = (char*)"recvfile";
    char* ws = (char*)"256";
    char* pb = (char*)"3";
    char* er = (char*)"73";
    EXPECT_EQ(sender_test(message, recvfile, ws, pb, er), 1);
}

TEST(RTP, OPT_RECEIVER_TEST_LOST_DATA)
{
    char* message = (char*)"testdata";
    char* recvfile = (char*)"recvfile";
    char* ws = (char*)"256";
    char* pb = (char*)"10";
    char* er = (char*)"1";
    EXPECT_EQ(opt_receiver_test(message, recvfile, ws, pb, er), 1);
}

TEST(RTP, OPT_RECEIVER_TEST_DUP_DATA)
{
    char* message = (char*)"testdata";
    char* recvfile = (char*)"recvfile";
    char* ws = (char*)"256";
    char* pb = (char*)"10";
    char* er = (char*)"2";
    EXPECT_EQ(opt_receiver_test(message, recvfile, ws, pb, er), 1);
}

TEST(RTP, OPT_RECEIVER_TEST_WRONG_CHKSUM_DATA)
{
    char* message = (char*)"testdata";
    char* recvfile = (char*)"recvfile";
    char* ws = (char*)"256";
    char* pb = (char*)"10";
    char* er = (char*)"3";
    EXPECT_EQ(opt_receiver_test(message, recvfile, ws, pb, er), 1);
}

TEST(RTP, OPT_RECEIVER_TEST_REORDER_DATA)
{
    char* message = (char*)"testdata";
    char* recvfile = (char*)"recvfile";
    char* ws = (char*)"256";
    char* pb = (char*)"10";
    char* er = (char*)"4";
    EXPECT_EQ(opt_receiver_test(message, recvfile, ws, pb, er), 1);
}

TEST(RTP, OPT_RECEIVER_TEST_MIXED_1)
{
    char* message = (char*)"testdata";
    char* recvfile = (char*)"recvfile";
    char* ws = (char*)"256";
    char* pb = (char*)"10";
    char* er = (char*)"73";
    EXPECT_EQ(opt_receiver_test(message, recvfile, ws, pb, er), 1);
}

TEST(RTP, OPT_RECEIVER_TEST_MIXED_2)
{
    char* message = (char*)"testdata";
    char* recvfile = (char*)"recvfile";
    char* ws = (char*)"256";
    char* pb = (char*)"3";
    char* er = (char*)"73";
    EXPECT_EQ(opt_receiver_test(message, recvfile, ws, pb, er), 1);
}

TEST(RTP, OPT_SENDER_TEST_LOST_ACK)
{
    char* message = (char*)"testdata";
    char* recvfile = (char*)"recvfile";
    char* ws = (char*)"256";
    char* pb = (char*)"10";
    char* er = (char*)"1";
    EXPECT_EQ(opt_sender_test(message, recvfile, ws, pb, er), 1);
}

TEST(RTP, OPT_SENDER_TEST_DUP_ACK)
{
    char* message = (char*)"testdata";
    char* recvfile = (char*)"recvfile";
    char* ws = (char*)"256";
    char* pb = (char*)"10";
    char* er = (char*)"2";
    EXPECT_EQ(opt_sender_test(message, recvfile, ws, pb, er), 1);
}

TEST(RTP, OPT_SENDER_TEST_WRONG_CHKSUM_ACK)
{
    char* message = (char*)"testdata";
    char* recvfile = (char*)"recvfile";
    char* ws = (char*)"256";
    char* pb = (char*)"10";
    char* er = (char*)"3";
    EXPECT_EQ(opt_sender_test(message, recvfile, ws, pb, er), 1);
}

TEST(RTP, OPT_SENDER_TEST_REORDER_ACK)
{
    char* message = (char*)"testdata";
    char* recvfile = (char*)"recvfile";
    char* ws = (char*)"256";
    char* pb = (char*)"10";
    char* er = (char*)"4";
    EXPECT_EQ(opt_sender_test(message, recvfile, ws, pb, er), 1);
}

TEST(RTP, OPT_SENDER_TEST_MIXED_1)
{
    char* message = (char*)"testdata";
    char* recvfile = (char*)"recvfile";
    char* ws = (char*)"256";
    char* pb = (char*)"10";
    char* er = (char*)"73";
    EXPECT_EQ(opt_sender_test(message, recvfile, ws, pb, er), 1);
}

TEST(RTP, OPT_SENDER_TEST_MIXED_2)
{
    char* message = (char*)"testdata";
    char* recvfile = (char*)"recvfile";
    char* ws = (char*)"256";
    char* pb = (char*)"3";
    char* er = (char*)"73";
    EXPECT_EQ(opt_sender_test(message, recvfile, ws, pb, er), 1);
}

int main(int argc, char **argv)
{
    
    //  /** CLOSE STDOUT **/
    // int dup_stdout = dup(STDOUT_FILENO);
    // int null_fd = open("/dev/null", O_WRONLY);
    // dup2(null_fd, 1);
    // close(null_fd);
    
    // /** REOPEN STDOUT **/
    // dup2(dup_stdout, STDOUT_FILENO);
    // close(dup_stdout);
    // /** REOPEN STDOUT **/
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
