#include <cstdio>
#include <thread>

using namespace std;

// Linux
// stdout : _IOLBF 行缓冲   IO 大概是 60ms
// stderr : _IONBF 无缓冲
// fopen : _IOFBF 完全缓冲  BUFSIZ : 8192

// stdout : _IOLBF 行缓冲 1
// stderr : _IONBF 无缓冲 2
// stdlog : _IOLBF 行缓冲 1
// fstream : _IOFBF 完全缓冲

// MSVC
// stdout : _IONBF 无缓冲
// stderr : _IONBF 无缓冲
// fopen : _IOFBF 完全缓冲  BUFSZ : 512

// stdout : _IONBF 无缓冲
// stderr : _IONBF 无缓冲
// stdlog : _IONBF 无缓冲
// cfstream : _IOFBF 完全缓冲

// C语言标准 stdout可以是行缓冲，stderr必须是无缓冲

static char buf[BUFSIZ];

// _IOLBF : c == '\n' || sz >= BUFSIZ

int main () {
    setvbuf(stdout, buf, _IONBF, sizeof buf);

    // printf("Hello, ");
    fprintf(stdout, "Hello, ");
    // fprintf(stderr, "Hello, ");
    fflush(stdout);

    this_thread::sleep_for(1s);

    printf("World\n");

    this_thread::sleep_for(1s);

    printf("Exiting\n");

    this_thread::sleep_for(1s);
    
    return 0;
}