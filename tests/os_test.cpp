#define OS_IMPL
#include "../os.h"
#include <thread>

#include <gtest/gtest.h>
class FileTest : public ::testing::Test {
   protected:
    virtual void SetUp() {
        tempFile = make_tempfile();
        file     = file_open(tempFile, "rw+");
    }

    virtual void TearDown() {
        free(tempFile);
        file_close(file);
        free(file);
    }

    char* tempFile;
    File* file;
};

TEST_F(FileTest, FileOpen) {
    ASSERT_TRUE(file != nullptr);
    ASSERT_TRUE(tempFile != nullptr);
    ASSERT_TRUE(file_size(file) == 0U);
}

TEST_F(FileTest, FileWrite) {
    const char* data = "Hello, World!";
    size_t written   = file_write(file, data, 1, strlen(data));
    ASSERT_EQ(written, strlen(data));
}

TEST_F(FileTest, FileRead) {
    const char* data = "Hello, World!";
    file_write(file, data, strlen(data), 1);
    rewind(file->file);

    char buffer[256];
    size_t read  = file_read(file, buffer, 1, sizeof(buffer) - 1);
    buffer[read] = '\0';
    ASSERT_EQ(read, strlen(data));
    ASSERT_STREQ(buffer, data);
}

TEST_F(FileTest, FileSeek) {
    const char* data = "Hello, World!";
    file_write(file, data, strlen(data), 1);
    rewind(file->file);

    char buffer[256];
    file_seek(file, 7, SEEK_SET);
    size_t read  = file_read(file, buffer, 1, sizeof(buffer) - 1);
    buffer[read] = '\0';
    ASSERT_EQ(read, strlen(data) - 7);
    ASSERT_STREQ(buffer, "World!");
}

TEST_F(FileTest, FileTell) {
    const char* data = "Hello, World!";
    file_write(file, data, strlen(data), 1);
    rewind(file->file);

    file_seek(file, 7, SEEK_SET);
    ASSERT_EQ(ftell(file->file), 7);
}

TEST_F(FileTest, FileSize) {
    const char* data = "Hello, World!";
    file_write(file, data, strlen(data), 1);
    file_close(file);
    file = file_open(tempFile, "r");
    ASSERT_EQ(file_size(file), strlen(data));
}

// file read all
TEST_F(FileTest, FileReadAll) {
    const char* data = "Hello, World!";
    file_write(file, data, strlen(data), 1);
    rewind(file->file);

    ssize_t read = -1;
    char* buffer = (char*)file_readall(file, &read);
    ASSERT_STREQ(buffer, data);
    ASSERT_EQ(read, strlen(data));
    free(buffer);
}

// async read(aread)
TEST_F(FileTest, AsyncRead) {
    file_awrite(file, "Hello World!", 12, 0);
    char buffer2[16];
    size_t size  = 16;
    off_t offset = 0;
    long read    = 0;
    read         = file_aread(file, buffer2, size, offset);
    ASSERT_EQ(read, 12);
    buffer2[12] = '\0';
    ASSERT_STREQ(buffer2, "Hello World!");
}

// is_file
TEST_F(FileTest, IsFile) {
    ASSERT_TRUE(is_file(tempFile));
    ASSERT_FALSE(is_file("nonexistent"));
}

// file_lock
TEST_F(FileTest, FileLockUnlock) {
    ASSERT_TRUE(file_lock(file));
    ASSERT_TRUE(file->is_locked);
    ASSERT_TRUE(file_unlock(file));
    ASSERT_FALSE(file->is_locked);
}

// file copy
TEST_F(FileTest, FileCopy) {
    const char* data = "Hello, World!";
    file_write(file, data, strlen(data), 1);
    file_close(file);

    file            = file_open(tempFile, "r");
    char* tempFile2 = make_tempfile();
    File* file2     = file_open(tempFile2, "rw+");

    ASSERT_TRUE(file_copy(file, file2) == 0);
    ASSERT_EQ(file_size(file), file_size(file2));
    file_close(file2);
}

// file_mmap & unmap
TEST_F(FileTest, FileMmapUnmap) {
    const char* data = "Hello, World!";
    file_write(file, data, strlen(data), 1);

    // mmap the file
    size_t len = strlen(data);
    char* mem  = (char*)file_mmap(file, len);
    ASSERT_TRUE(mem != nullptr);
    ASSERT_STREQ(mem, data);

    // unmap the memory
    int ret = file_munmap(mem, len);
    ASSERT_EQ(ret, 0);
}

class PipeTest : public ::testing::Test {
   protected:
    virtual void SetUp() { pipe_open(&p); }
    virtual void TearDown() { pipe_close(&p, PIPE_END_BOTH); }

    PIPE p;
};

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

TEST_F(PipeTest, PipeReadAndWrite) {
    Thread t            = 0;
    std::string message = "Hello, World!";
    PipeArgs* args      = (PipeArgs*)malloc(sizeof(struct PipeArgs));
    ASSERT_TRUE(args != nullptr);
    args->pipe    = &p;
    args->message = message.data();

    // Write to pipe in a seperate thread as may block
    ThreadData data = (ThreadData){.arg = args};
    thread_create(&t, handle_pipe_write, &data);

    // Read from pipe
    char buf[16];
    ssize_t read2 = pipe_read(&p, buf, sizeof(buf));
    buf[read2]    = '\0';

    ASSERT_EQ(read2, message.size());
    ASSERT_STREQ(buf, message.data());

    thread_join(t, NULL);

    free(args);
}

// test threadpool functions
void handle_threadpool(void* ptr) {
    ThreadData* data = (ThreadData*)ptr;

    // Double number in  retval
    int* retval = (int*)data->retval;
    *retval     = *retval * 2;
    free(data);

    sleep_ms(250);
}

TEST(ThreadPoolTest, ThreadPool) {
    ThreadPool* pool;
    const std::string message = "Hello, World!";

    pool = threadpool_create(std::thread::hardware_concurrency());
    ASSERT_TRUE(pool != nullptr);

    int retvals[10] = {0};
    for (int i = 0; i < 10; i++) {
        retvals[i]       = i;
        ThreadData* data = (ThreadData*)malloc(sizeof(ThreadData));
        if (data == nullptr) {
            printf("Failed to allocate memory\n");
            return;
        }

        data->arg    = (void*)message.data();
        data->retval = &retvals[i];
        threadpool_add_task(pool, handle_threadpool, data);
    }

    threadpool_wait(pool);

    ASSERT_EQ(pool->num_working_threads, 0);

    // Free the retval
    for (int i = 0; i < 10; i++) {
        ASSERT_EQ(retvals[i], i * 2);
    }
    threadpool_destroy(pool);
}

// test process functions
TEST(ProcessTest, Process) {
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
    ASSERT_EQ(ret, 0);
    ASSERT_TRUE(proc.pid > 0);
    int status = -1;
    process_wait(&proc, &status);
    ASSERT_TRUE(status == 0);
}

// File path functions
// =============================

TEST(FilepathTest, FilepathBasename) {
    char basename[256];
    filepath_basename("/home/user/file.txt", basename, sizeof(basename));
    ASSERT_STREQ(basename, "file.txt");
}

TEST(FilepathTest, FilepathDirname) {
    char dirname[256];
    filepath_dirname("/home/user/file.txt", dirname, sizeof(dirname));
    ASSERT_STREQ(dirname, "/home/user");
}

TEST(FilepathTest, FilepathExtension) {
    char ext[256];
    filepath_extension("/home/user/file.txt", ext, sizeof(ext));
    ASSERT_STREQ(ext, ".txt");
}

TEST(FilepathTest, FilepathNameonly) {
    char name[256];
    filepath_nameonly("/home/user/file.txt", name, sizeof(name));
    ASSERT_STREQ(name, "file");
}

TEST(FilepathTest, FilepathAbsolute) {
    char* tmpDir     = make_tempdir();
    char* joinedPath = filepath_join(tmpDir, "file.txt");
    File* f          = file_open(joinedPath, "w");
    ASSERT_TRUE(f != nullptr);
    file_close(f);

    ASSERT_TRUE(joinedPath != nullptr);

    char* abs = filepath_absolute(joinedPath);
    ASSERT_TRUE(abs != nullptr);
    ASSERT_TRUE(abs[0] == '/');
    free(abs);
    free(joinedPath);
}

TEST(FilepathTest, FilepathRemove) {
    char* tempFile = make_tempfile();
    ASSERT_TRUE(filepath_remove(tempFile) == 0);
    free(tempFile);
}

TEST(FilepathTest, FilepathRename) {
    char* tempFile  = make_tempfile();
    char* tempFile2 = make_tempfile();
    ASSERT_TRUE(filepath_rename(tempFile, tempFile2) == 0);
    free(tempFile);
    free(tempFile2);
}

TEST(FilepathTest, FilepathExpanduser) {
    char* home = getenv("HOME");
    char* abs  = filepath_expanduser("~/");
    ASSERT_TRUE(abs != nullptr);
    ASSERT_STREQ(abs, home);
    free(abs);

    abs = filepath_expanduser("~/user/Dowloads");
    ASSERT_TRUE(abs != nullptr);

    ASSERT_TRUE(strstr(abs, home) != nullptr);
}

TEST(FilepathTest, FilepathJoin) {
    char* path = filepath_join("/home/user", "file.txt");
    ASSERT_TRUE(path != nullptr);
    ASSERT_STREQ(path, "/home/user/file.txt");
    free(path);
}

TEST(FilepathTest, FilepathSplit) {
    char dir[256];
    char name[256];
    filepath_split("/home/user/file.txt", dir, name, sizeof(dir), sizeof(name));
    ASSERT_STREQ(dir, "/home/user");
    ASSERT_STREQ(name, "file.txt");
}

int main(int argc, char* argv[]) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}