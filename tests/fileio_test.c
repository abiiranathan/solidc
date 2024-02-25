#include "os.h"

void* logMessage(void* ptr) {
    ThreadData* data = (ThreadData*)ptr;
    char* message;
    message = (char*)data->arg;
    printf("%s\n", message);

    return NULL;
}

void printNumAndSleep(void* ptr);

struct PipeArgs {
    char* message;
    PIPE* pipe;
};

void* handle_pipe_write(void* ptr) {
    ThreadData* data = (ThreadData*)ptr;
    struct PipeArgs* args;
    args      = (struct PipeArgs*)data->arg;
    ssize_t w = pipe_write(args->pipe, args->message, strlen(args->message));
    printf("Wrote to pipe : %zd bytes\n", w);
    return NULL;
}

int main(void) {
    File* file = file_open("test.txt", "w");

    const char* buffer = "Hello, World!\n";

    file_write_string(file, buffer);
    file_close(file);

    makedirs("testdir\\testdir2\\testdir3\\windows");

    // asyncronus file read.
    File* f2 = file_open("test.txt", "r");

    char buffer2[1024];
    size_t size  = 1024;
    off_t offset = 0;
    long read    = 0;
    read         = file_aread(f2, buffer2, size, offset);
    printf("Read: %s\n", buffer2);
    printf("Read: %ld\n", read);
    file_close(f2);

    // asyncronus file write.
    File* f3 = file_open("test.txt", "a");
    const char* buffer3 =
        "This is a long string to write to the file. Lorem ipsum dolor sit amet, consectetur "
        "adipiscing elit. Nulla nec odio nec nunc tincidunt tincidunt\n";
    size_t size3    = strlen(buffer3);
    off_t offset3   = 13;
    ssize_t written = 0;
    written         = file_awrite(f3, buffer3, size3, offset3);
    file_close(f3);
    printf("Written: %zd\n", written);

    // asyncronus file read.
    File* f4 = file_open("/home/nabiizy/Documents/ConceptNote.docx", "rb");

    char buffer4[BUFSIZ * 4];
    size_t count      = 1;
    size_t read_bytes = 0;
    read_bytes        = file_read(f4, (void*)buffer4, sizeof(buffer4), count);
    printf("Read: %zu bytes\n", read_bytes);

    // asyncronus file write.
    File* f5 = file_open("/home/nabiizy/Documents/ConceptNote2.docx", "wb");
    file_write(f5, (void*)buffer4, sizeof(buffer4), count);
    file_close(f4);
    file_close(f5);

    File *f6, *f7;
    f6 = file_open("test.txt", "r");
    f7 = file_open("test2.txt", "w");
    file_copy(f6, f7);
    file_close(f6);
    file_close(f7);

    // Create new thread
    int num_threads = 4;
    Thread threads[num_threads];
    char message[] = "Hello, World!";

    ThreadData* td = &(ThreadData){.arg = message, .retval = NULL};
    for (int i = 0; i < num_threads; i++) {
        thread_create(&threads[i], logMessage, td);
    }

    for (int i = 0; i < num_threads; i++) {
        thread_join(threads[i], NULL);
    }

    // Test pipes
    PIPE p;
    int fd = pipe_open(&p);
    printf("pipe fd: %d\n", fd);

    // Write to pipe. This will block until the pipe is read on windows.
    // Run in a separate thread to avoid blocking.
    Thread t         = 0;
    char message2[]  = "Hello, World!";
    ThreadData* data = &(ThreadData){.arg = &(struct PipeArgs){.message = message2, .pipe = &p}};
    thread_create(&t, handle_pipe_write, data);

    // Read from pipe
    char buffer5[1024];
    ssize_t read2 = pipe_read(&p, buffer5, sizeof(buffer5));
    printf("Read from pipe : %s\n", buffer5);
    printf("Bytes read: %zu\n", read2);

    // Close pipe
    thread_join(t, NULL);
    pipe_close(&p, PIPE_END_BOTH);

    // test processes
#ifdef _WIN32
    char cmd[]                = "ping -n 1 localhost";
    const char* const argv2[] = {NULL};
#else
    char cmd[]                = "/bin/ping";
    const char* const argv2[] = {"/bin/ping", "-c", "1", "localhost", NULL};
#endif
    const char* const envp[] = {"PATH=/bin", NULL};
    Process proc;
    int ret = process_create(&proc, cmd, argv2, envp);
    if (ret == -1) {
        printf("Error creating process\n");
    }

    printf("Process created with pid: %ld\n", proc.pid);
    int status = -1;
    process_wait(&proc, &status);
    printf("Process exited with status: %d\n", status);

// seed random number generator
#ifdef _WIN32
    srand(GetTickCount());
#else
    srand(time(NULL));
#endif

    // test thread pool
    ThreadPool* pool = threadpool_create(10);
    for (int i = 0; i < 20; i++) {
        int* num = (int*)malloc(sizeof(int));
        if (num == NULL) {
            printf("Memory allocation failed\n");
            return 1;
        }

        *num           = i + 1;
        ThreadData* td = (ThreadData*)malloc(sizeof(ThreadData));
        if (td == NULL) {
            printf("Memory allocation failed\n");
            free(num);
            return 1;
        }
        td->arg    = num;
        td->retval = NULL;
        threadpool_add_task(pool, printNumAndSleep, td);
    }

    // wait for all tasks to complete
    threadpool_wait(pool);

    // destroy thread pool
    // Sometime I get a random warning on windows
    // RtlLeaveCriticalSection section 00007FFFFE8A0CD0 (null)
    // is not acquired, this happens randomly
    threadpool_destroy(pool);
    return 0;
}

void printNumAndSleep(void* ptr) {
    ThreadData* data = (ThreadData*)ptr;
    int num          = *(int*)data->arg;
    printf("Thread %d: %d\n", thread_self(), num);

    sleep_ms(1000);
}

// Test windows features with mingw compiler
// Run: x86_64-w64-mingw32-gcc OS_test.c -o OS_test.exe