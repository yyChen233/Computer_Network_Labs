# POSIX Threads

*本文翻译自 [《Getting Started With POSIX Threads》](https://github.com/N2Sys-EDU/Lab1-Documentation/releases/download/resources/pthreads.pdf) by Tom Wagner and Don Towsley, in 1995* 。

## Introduction: What is a thread and what is it good for?

线程通常被称为轻量级进程，虽然这样说有点过度简化，但它是一个很好的出发点。线程类似于 UNIX 进程，但又和进程不同。为了理解这种区别，我们可以考察 UNIX 进程和 Mach 任务与线程的关系。

在 UNIX 中，一个进程包含一个执行中的程序和若干 resources，如文件描述符表和地址空间等。而在 Mach 中，一个任务只包含 resources，而所有的执行操作都由线程完成。一个 Mach 任务可以和任意数量的线程相关联，每个线程都必须和某个任务相关联。与同一个任务相关联的所有线程将共享任务的 resources。所以一个线程基本上应该包括程序计数器 (program counter) ，运行时栈 (stack) 以及一组寄存器 (register) ，其他所有的数据结构都应该属于任务。因此， UNIX 进程在 Mach 中可以被认为是一个由单个线程执行的任务。

由于线程与进程相比非常的小，创建线程带来的 CPU 开销就相对的更小。另一方面，由于所有的线程可以共享 resources，而每个进程都需要自己的 resources，因此线程也同样更节约内存。 Mach 线程使得程序员可以编写能在单处理器或者多处理器上运行的并发应用程序，并且具体的处理器分配对程序员而言是透明的。并发应用程序不仅可以自发的利用多处理器资源，也能提高在程序在单处理器情况下的性能，例如当应用程序执行可能导致阻塞或延迟的操作如文件或 socket 读写时。

我们将介绍 POSIX threads 的部分标准以及它的一个具体实现 。 POSIX threads 被称为 pthreads 。

## Hello World

`pthread_create` 函数用于创建一个新的线程，它需要四个参数，包括一个 `pthread_t` 变量用于存储线程，一个 `pthread_attr_t` 变量用于指明线程属性，一个线程初始化函数及该函数的参数。线程初始化函数会在线程执行时被调用。
``` c
pthread_t thread;
pthread_addr_t thread_attribute;
void thread_function(void *argument);
char* argument;

pthread_create(&thread, thread_attribute, (void*)&thread_function, (void*)&argument);
```
大多数情况下，线程属性参数用来指明最小栈空间，可以使用 `pthread_attr_default` 来使用默认参数, 在未来可能会有更多的用法。与 UNIX 进程的 `fork` 从当前程序的相同位置开始执行不同，线程会从 `pthread_create` 中指明的初始化函数处开始执行。这样做的原因很简单，如果线程也从当前程序位置开始执行，那么可能有多个线程使用相同的 resources 执行同样的指令。

现在我们知道了如何创建线程。让我们来设计一个多线程应用在 `stdout` 上输出被深爱的 `Hello World` 吧。首先，我们需要两个 `pthread_t` 变量，以及一个初始化函数。我们还需要一个方法来让每个线程打印不同的信息。一个方法是将输出分解成若干字符串，并给每个线程一个不同的字符串作为初始化函数的参数。可以参考以下代码：
``` c
void print_message_function(void* ptr);
main() {
    pthread_t thread1, thread2;
    char* message1 = "Hello";
    char* message2 = "World";
    pthread_create(&thread1, pthread_attr_default, (void*)&print_message_fuction, (void*)message1);
    pthread_create(&thread2, pthread_attr_default, (void*)&print_message_fuction, (void*)message2);
    exit(0);
}
void print_message_function(void* ptr) {
    char* message;
    message = (char*)ptr;
    printf("%s ", message);
}
```
*译者注：如果你想要编译并执行这段代码，请参考 Pragmatics 一节。*

注意 `print_message_function` 的原型以及在调用时的强制类型转换。这段程序首先通过 `pthread_create` 创建第一个线程，并将 `Hello` 作为初始化参数传入。第二个线程的初始化参数是 `World` 。第一个线程将从 `print_message_function` 的第一行开始执行，它将输出 `Hello` 并退出。一个线程会在离开初始化函数时被关闭，因此第一个线程将会在输出 `Hello` 后关闭。同样的，第二个线程会在输出 `World` 后关闭。尽管这段代码看起来很合理，它实际上存在两个重要的缺陷。

第一个也是最重要的问题是，线程是并发执行的。因此并不能保证第一个线程先于第二个线程到达 `printf` 函数。因此我们可能会看到 `World Hello` 而不是 `Hello World` 。

另一个更微妙的问题是，注意到在父线程（最初的线程，尽管每个线程都是一样的，我们仍习惯于这样称呼）中调用了 `exit` 函数。如果父线程在两个子线程执行 `printf` 之前就调用了 `exit` ，那么将不会有任何输出。这是因为 `exit` 函数将退出进程（释放任务），因而将结束所有线程。因此，任一线程，不论是父线程或是子线程，只要调用 `exit` 就将结束所有其他线程和该进程。如果线程希望明确的终止，它必须使用 `pthread_exit` 函数来避免这个问题。

因此可以看到，我们的 `Hello World` 程序有两个竞争情况。一个是由 `exit` 调用产生的竞争，另一个是由谁先到达 `printf` 产生的竞争。让我们使用一点疯狂的胶水和胶带来解决这些竞争。既然我们希望每个子线程在父线程退出之前完成，让我们在父线程中插入一些延迟来给子线程更多时间。为了保证第一个子线程先执行 `printf` ，让我们在第二次 `pthread_create` 调用前插入一些延迟。这样我们的代码修改为：
``` c
void print_message_function(void* ptr);
main() {
    pthread_t thread1, thread2;
    char* message1 = "Hello";
    char* message2 = "World";
    pthread_create(&thread1, pthread_attr_default, (void*)&print_message_fuction, (void*)message1);
    sleep(10);
    pthread_create(&thread2, pthread_attr_default, (void*)&print_message_fuction, (void*)message2);
    sleep(10);
    exit(0);
}
void print_message_function(void* ptr) {
    char* message;
    message = (char*)ptr;
    printf("%s ", message);
    pthread_exit(0);
}
```
这段代码能达到我们的目标吗？并不一定。依靠时间上的延迟来执行同步是不安全的。*（译者注：我们不能保证第一个线程在多得到 10 秒时间后就能先于线程二完成 `printf`，同样也不能保证父线程在延迟后不会先于子线程执行 `exit`，这取决于操作系统的调度情况）* 这里的竞争情况实际上和我们在分布式应用和共享资源中遇到的情况一样。共享资源就是这里的 `stdout` ，分布式计算单元就是这里的三个线程。线程一必须在线程二之前输出到 `stdout` 并且两者都需要在父线程调用 `exit` 前完成工作。

除了我们试图使用延迟来进行同步之外，我们还犯了另一个错误。 `sleep` 函数和 `exit` 函数一样都作用于进程。当一个线程调用 `sleep` 时，整个进程都将挂起，也就是说所有的线程都将被挂起。因此我们现在的情况实际上和不添加 `sleep` 时完全一样，除了程序会多运行 20 秒。想要使一个线程延时，正确的函数应该是 `pthread_delay_np` （ np 意为 not process ），例如，将一个线程延迟 2 秒可以使用：
``` c
struct timespec delay;
delay.tv_sec = 2;
delay.tv_nsec = 0;
pthread_delay_np(&delay);
```
*译者注： `struct timespec` 在 `time.h` 中定义。*

本节包含的函数有 `pthread_create()` ， `pthread_exit()` ， `pthread_delay_np()` 。

## Thread Synchronization

POSIX 提供了两种线程同步的原语，即互斥锁 (mutex) 和条件变量 (condition variable) 。互斥锁是一种简单的锁原语 (lock primitives) ，用于控制对共享资源的访问。注意到在线程中，整个地址空间是共享的，因此所有一切都可以被认为是共享资源。然而，在大多数情况下，线程单独使用（概念上的）私有局部变量进行工作，这些变量是在 `pthread_create` 调用的初始化函数及其后续函数中被创建的，线程间通过全局变量将它们的工作结合起来。对共享的可写变量的访问必须被控制。*（译者注：线程具有不同的运行时栈地址，而局部变量存放在栈上，因此局部变量不共享，全局变量共享，然而线程间地址空间共享，故一个线程可以通过另一个线程的运行时栈地址访问其局部变量，因此广义上说局部变量也是共享的。对同一对象的写若不是原子的则可能出现指令级的错误，因此需要控制。）*

让我们创建一个读者/写者的应用程序，其包含一个读者和一个写者通过一个共享的缓冲区进行通信，并且使用一个互斥锁来控制访问。
``` c
void reader_function(void);
void writer_function(void);

char buffer;
int buffer_has_item = 0;
pthread_mutex_t mutex;
struct timespec delay;

main() {
    pthread_t reader;
    delay.tv_sec = 2;
    delay.tv_nsec = 0;

    pthread_mutex_init(&mutex, pthread_mutexattr_defalut);
    pthread_create(&reader, pthread_attr_default, (void*)&reader_function, NULL);
    writer_function();
}

void writer_function(void) {
    while(1) {
        pthread_mutex_lock(&mutex);
        if(buffer_has_item == 0) {
            buffer = make_new_item();
            buffer_has_item = 1;
        }
        pthread_mutex_unlock(&mutex);
        pthread_delay_np(&delay);
    }
}

void reader_function(void) {
    while(1) {
        pthread_mutex_lock(&mutex);
        if(buffer_has_item == 1) {
            consume_item(buffer);
            buffer_has_item = 0;
        }
        pthread_mutex_unlock(&mutex);
        pthread_delay_np(&delay);
    }
}
```
在这个简单的程序中，我们假设缓冲区只能容纳一个元素，所以它总是处于两种状态之一，要么有元素，要么为空。写者首先锁住互斥锁，若调用时互斥锁已被锁住，则阻塞直到其解锁。之后检查缓冲区是否为空。如果缓冲区为空，那么创建一个新的元素并设置标记 `buffer_has_item` 来告诉读者缓冲区中有元素。之后写者解锁互斥锁并等待 2 秒使得读者有机会消费掉这个元素。这里的延迟与我们之前使用的延迟的目的并不相同，它只是为了提高程序的效率。如果没有这个延迟，写者会释放锁，并在下一条语句中尝试重新获得锁以创建新的元素。读者很可能没有机会消费掉元素。因此使用延迟是一个好的方式。

读者的工作方式是类似的。它首先获得锁，然后检查是否有元素被创建了，如果有，消费该元素。之后它释放锁，并等待一小段时间来让写者有机会再次创建元素。在这个例子中，读者和写者一直运行，不断地生成和消费元素。然而，如果一个互斥锁在程序中不再需要，应该使用 `pthread_mutex_destroy(&mutex)` 来销毁它。注意到在必须的互斥锁的初始化函数中，我们使用了 `pthread_mutexattr_default` 来作为互斥锁的属性。在 OSF/1 中 `mutex attribute` 没有任何意义，所以我们建议使用默认值。

正确的使用互斥锁可以保证消除竞争情况。然而，互斥锁本身是非常弱的，因为它只有锁定和解锁两种状态。 POSIX 条件变量通过允许线程阻塞并等待其他线程的信号以作为同步机制的补充。当信号被接收时，阻塞的线程将被唤醒并尝试在相关的互斥锁上获取锁。因此信号和互斥锁可以结合起来用来消除我们的读者写者问题中出现的自旋锁 (spin-lock) 问题。我们设计了一个使用 pthread 的互斥锁和条件变量的简单的整数信号量库 (integer semaphores) 。之后，我们基于此讨论同步问题。这些信号量的代码可以在附录 A 中找到，关于条件变量的更详细信息可以在 man pages 中找到。

本节包含的函数有 `pthread_mutex_init()` ， `pthread_mutex_lock()` ， `pthread_mutex_unlock()` ， `pthread_mutex_destroy()` 。

## Coordinating Activities With Semaphores

让我们使用信号量重新审视我们的读者/写者程序。我们将使用更 robust 的整数信号量替代互斥锁并消除自旋锁问题。 信号量操作包括 `semaphore_up`， `semaphore_down` ， `semaphore_init` ， `semaphore_destroy` 和 `semaphore_decrement` 。 `up` 和 `down` 函数符合传统的信号量语义，如果信号量的值小于等于 0 那么 `down` 操作将被阻塞，而 `up` 操作将增加信号量的值。 `init` 函数必须在使用信号量之前被调用，所有的信号量都将被初始化为 `1` 。 当信号量不再需要时， `destroy` 函数用于销毁信号量。所有的函数都只有一个参数，即指向信号量对象的指针。

`decrement` 函数是一个用于减少信号量的值的非阻塞的函数。它允许线程将信号量的值减少到某个负数值作为初始化过程的一部分。我们将在读者写者程序之后给出一个使用 `semaphore_decrement` 的例子。
``` c
void reader_function(void);
void writer_function(void);

char buffer;
Semaphore writers_turn;
Semaphore readers_turn;

main() {
    pthread_t reader;
    semaphore_init(&readers_turn);
    semaphore_init(&writers_turn);

    /* writer must go first */
    semaphore_down(&readers_turn);

    pthread_create(&reader, pthread_attr_default, (void*)&reader_function, NULL);
    writer_function();
}

void writer_function(void) {
    while(1) {
        semaphore_down(&writers_turn);
        buffer = make_new_item();
        semaphore_up(&readers_turn);
    }
}

void reader_function(void) {
    while(1) {
        semaphore_down(&readers_turn);
        consume_item(buffer);
        semaphore_up(&writers_turn);
    }
}
```

这个例子仍然没有完全利用一般的整数信号量的力量。让我们修改一下第二节中的 `Hello World` 程序并使用整数信号量来解决竞争问题。
``` c
void print_message_function(void* ptr);

Semaphore child_counter;
Semaphore worlds_turn;

main () {
    pthread_t thread1, thread2;
    char* message1 = "Hello";
    char* message2 = "World";

    semaphore_init(&child_counter);
    semaphore_init(&worlds_turn);

    semaphore_down(&worlds_turn); /* world goes second */

    semaphore_decrement(&child_counter); /* value now 0 */
    semaphore_decrement(&child_counter); /* value now -1 */
    /* child_counter now must be up-ed 2 times for a thread blocked on it to be released */

    pthread_create(&thread1, pthread_attr_default, (void*)&print_message_function, (void*)message1);
    semaphore_down(&worlds_turn);
    pthread_create(&thread2, pthread_attr_default, (void*)&print_message_function, (void*)message2);
    semaphore_down(&child_counter);
    
    /* not really necessary to destroy since we are exiting anyway */
    semaphore_destroy(&child_counter);
    semaphore_destroy(&worlds_turn);
    exit(0);
}

void print_message_function(void* ptr) {
    char* message;
    message = (char*)ptr;
    printf("%s", message);
    fflush(stdout);
    semaphore_up(&worlds_turn);
    semaphore_up(&child_counter);
    pthread_exit(0);
}
```

读者可以很容易的确信在这个版本的 `Hello World` 程序中没有任何竞争问题并且单词会按照正确的顺序打印。信号量 `child_counter` 是用来强制父线程阻塞直到两个子线程都完成了 `printf` 语句和 `semaphore_up(&child_counter)` 。

本节中包含的函数有 `semaphore_init()` ， `semaphore_up()` ， `semaphore_down()` ， `semaphore_destroy()` ， `semaphore_decrement()` 。

## Pragmatics

为了编译使用了 pthread 的代码，你必须包含头文件 `#include <pthread.h>` 并且必须链接 pthread 库。例如 `cc hello_world.c -o hello_world -lphtread`

*译者注：如果使用 Cmake，你应该使用 target_link_libraries(TARGET pthread)*

为了使用信号量库，你同样应该包含头文件并链接刀目标文件或库。

DEC 的 pthreads 是基于 POSIX IV 标准而不是 POSIX VIII 标准。函数 `pthread_join` 允许一个线程等待另一个线程退出。尽管这可以用在 `Hello World` 程序中用来确定子线程何时完成而不需要使用信号量操作，然而如果遇到线程对象不存在的情况， DEC 的 `pthread` 实现有着不可靠的行为。例如，在以下代码中，如果 `some_thread` 不再存在， `pthread_join` 可能会产生一个错误而不是直接返回。
``` c
pthread_t some_thread;
void *exit_status;
pthread_join(some_thread, &exit_status);
``` 

其他奇怪的错误可能发生在线程例程之外的函数中。虽然这些错误很少，但有些库做了单进程假设。例如，在使用 `fread` 和 `fwrite` 时，我们遇到了间歇性的困难，这只能归咎于竞争问题。关于错误的问题，我们在例子中没有检查函数的返回值以简化它们，但返回值是应该被检查的。几乎所有与 pthread 相关的函数在错误时都会返回 `-1` 。例如：
``` c
pthread_t some_thread;
if(pthread_create(&some_thread, ...) == -1) {
    perror("Thread creation error");
    exit(1);
}
```
信号量库会在遇到错误时输出信息并退出。

另外，一些有用的函数在例子中没有被提及，例如：
``` c
pthread_yield(); 
/* Informs the scheduler that the thread is willing to yield its quantum, requires no arguments. */

pthread_t me;
me = pthread_self(); 
/* Allows a pthread to obtain its own identifier */

pthread_t thread;
pthread_detach(thread); 
/* Informs the library that the threads exit status will not be needed by subsequent pthread_join calls resulting in better threads performance. */
```

更多信息请查阅库或 man pages, 例如 `man -k pthread` 

## Appendix A

见 [原文附录 A](https://github.com/N2Sys-EDU/Lab1-Documentation/releases/download/resources/pthreads.pdf) 。