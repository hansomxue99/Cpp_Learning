#include <cstdio>
#include <unistd.h>
#include <thread>
#include <string>
#include <fcntl.h>
#include <system_error>
#include <map>

using namespace std;

struct InStream {
    virtual size_t read(char *__restrict s, size_t len) = 0;
    virtual ~InStream() = default;

    virtual int getchar() {
        char c;
        size_t n = read(&c, 1);
        if (n == 0) {
            return EOF;
        }
        return c;
    }

    virtual size_t readn(char *__restrict s, size_t len) {
        size_t n = read(s, len);
        if (n == 0) return 0;
        while (n != len) {
            size_t m = read(s + n, len - n);
            if (m == 0) break;
            n += m;
        }
        return n;
    }

    std::string readall() {
        std::string ret;
        ret.resize(32);
        char *buf = &ret[0];
        size_t pos = 0;
        while (true) {
            size_t n = read(buf + pos, ret.size() - pos);
            if (n == 0) {
                break;
            }
            pos += n;
            if (pos == ret.size()) {
                ret.resize(ret.size() * 2);
            }
        }
        ret.resize(pos);
        return ret;
    }

    std::string readuntil(char eol) {
        std::string ret;
        while (true) {
            int c = getchar();
            if (c == EOF) {
                break;
            }
            ret.push_back(c);
            if (c == eol) {
                break;
            }
        }
        return ret;
    }

    std::string readuntil(const char *__restrict eol, size_t neol) {
        std::string ret;
        while (true) {
            int c = getchar();
            if (c == EOF) {
                break;
            }
            ret.push_back(c);
            if (ret.size() >= neol) {
                if (memcmp(ret.data() + ret.size() - neol, eol, neol) == 0) {
                    break;
                }
            }
        }
        return ret;
    }

    std::string readuntil(std::string const &eol) {
        return readuntil(eol.data(), eol.size());
    }


    std::string getline(char eol) {
        std::string ret = readuntil(eol);
        if (ret.size() > 0 && ret.back() == eol)
            ret.pop_back();
        return ret;
    }

    std::string getline(const char *__restrict eol, size_t neol) {
        std::string ret = readuntil(eol, neol);
        if (ret.size() >= neol && memcmp(ret.data() + ret.size() - neol, eol, neol) == 0)
            ret.resize(ret.size() - neol);
        return ret;
    }

    std::string getline(std::string const &eol) {
        return getline(eol.data(), eol.size());
    }
};


struct UnixFileInStream : InStream {
private:
    int fd;

public:
    explicit UnixFileInStream(int fd_) : fd(fd_) {

    }

    size_t read(char *__restrict s, size_t len) override {
        if (len == 0)   return 0;
        this_thread::sleep_for(0.1s);
        size_t n = ::read(fd, s, len);
        if (n < 0) {
            throw std::system_error(errno, std::generic_category());
        }
        return n;
    }

    UnixFileInStream(UnixFileInStream &&) = delete;

    ~UnixFileInStream() {
        ::close(fd);
    }
};


struct BufferedInStream : InStream {
private:
    std::unique_ptr<InStream> in;
    char *buf;
    size_t top = 0;
    size_t max = 0;

    [[nodiscard]] bool refill() {
        top = 0;
        max = in->read(buf, BUFSIZ);
        // max <= BUFSIZ
        return max != 0;
    }

public:
    explicit BufferedInStream(std::unique_ptr<InStream> in_)
        : in(std::move(in_))
    {
        buf = (char *)valloc(BUFSIZ);
    }

    int getchar() override {
        if (top == max) {
            if (!refill())
                return EOF;
        }
        return buf[top++];
    }

    size_t read(char *__restrict s, size_t len) override {
        // 如果缓冲区为空，则阻塞，否则尽量不阻塞，返回已缓冲的字符
        char *__restrict p = s;
        while (p != s + len) {
            if (top == max) {
                if (p != s || !refill())
                    break;
            }
            int c = buf[top++];
            *p++ = c;
        }
        return p - s;
    }

    size_t readn(char *__restrict s, size_t len) override {
        // 尽量读满 len 个字符，除非遇到 EOF，才会返回小于 len 的值
        char *__restrict p = s;
        while (p != s + len) {
            if (top == max) {
                if (!refill())
                    break;
            }
            int c = buf[top++];
            *p++ = c;
        }
        return p - s;
    }

    BufferedInStream(BufferedInStream &&) = delete;

    ~BufferedInStream() {
        free(buf);
    }
};


struct OutStream {
    virtual void write(const char *__restrict s, size_t len) = 0;

    virtual ~OutStream() = default;

    void puts(const char *__restrict s) {
        write(s, strlen(s));
    }

    virtual void putchar(char c) {
        write(&c, 1);
    }

    virtual void flush() {

    }
};

struct UnixFileOutStream : OutStream {
private:
    int fd;

public:
    explicit UnixFileOutStream(int fd_) : fd(fd_) {

    }

    void write(const char *__restrict s, size_t len) override {
        if (len == 0)   return;
        this_thread::sleep_for(0.1s);
        size_t written = ::write(fd, s, len);
        if (written < 0) {
            throw std::system_error(errno, std::generic_category());
        }

        if (written == 0) {
            throw std::system_error(EPIPE, std::generic_category());
        }

        while ((size_t)written != len) {
            size_t new_written = ::write(fd, s + written, len - written);
            if (new_written < 0) {
                throw std::system_error(errno, std::generic_category());
            }
            written += new_written;
        }
    }

    UnixFileOutStream(UnixFileOutStream &&) = delete;

    ~UnixFileOutStream() {
        ::close(fd);
    }
};

struct BufferedOutStream : OutStream{
    enum BufferMode {
        FullBuf,
        LineBuf,
        NoBuf,
    };

private:
    std::unique_ptr<OutStream> out;
    size_t top = 0;
    BufferMode mode;
    char *buf;

public:
    explicit BufferedOutStream(std::unique_ptr<OutStream> out_, BufferMode mode_ = FullBuf, char *buf_ = nullptr) 
            : out(std::move(out_))
            , mode(mode_)
            , buf(buf_) 
    {
        if (buf == nullptr && mode != _IONBF) {
            buf = (char *)valloc(4096);
        }
    }

    void flush() override {
        out->write(buf, top);
        top = 0;
    }

    void putchar(char c) override {
        if (mode == _IONBF) {
            out->write(&c, 1);
            return;
        }
        if (top == BUFSIZ) {
            flush();
        }
        buf[top++] = c;
        if (mode == _IOLBF && c == '\n') {
            flush();
        }
    }

    void write(const char *__restrict s, size_t len) override {
        if (mode == _IONBF) {
            out->write(s, len);
            return;
        }
        for (const char *__restrict p = s; p != s + len; ++p) {
            if (top == BUFSIZ) {
                flush();
            }
            char c = *p;
            buf[top++] = c;
            if (mode == _IOLBF && c == '\n') {
                flush();
            }
        }
    }

    BufferedOutStream(BufferedOutStream &&) = delete;   // 有析构需要去除移动函数，删除这一个即可删除其他三个

    ~BufferedOutStream() {
        flush();
        free(buf);
    }
};

BufferedInStream myin(std::make_unique<UnixFileInStream>(STDIN_FILENO));
BufferedOutStream mout(std::make_unique<UnixFileOutStream>(STDOUT_FILENO), BufferedOutStream::LineBuf);
BufferedOutStream merr(std::make_unique<UnixFileOutStream>(STDERR_FILENO), BufferedOutStream::NoBuf);

void mperror(const char *msg) {
    merr.puts(msg);
    merr.puts(": ");
    merr.puts(strerror(errno));
    merr.puts("\n");
}

enum OpenFlag {
    Read,
    Write,
    Append,
    ReadWrite,
};

std::map<OpenFlag, int> openFlagToUnixFlag = {
    {OpenFlag::Read, O_RDONLY},
    {OpenFlag::Write, O_WRONLY | O_TRUNC | O_CREAT},
    {OpenFlag::Append, O_WRONLY | O_APPEND | O_CREAT},
    {OpenFlag::ReadWrite, O_RDWR | O_CREAT},
};

std::unique_ptr<OutStream> out_file_open(const char *path, OpenFlag flag) {
    int oflag = openFlagToUnixFlag.at(flag);
    // oflag |= O_DIRECT;
    int fd = open(path, oflag);
    if (fd < 0) {
        throw std::system_error(errno, std::generic_category());
        // mperror(path);
        return nullptr;
    }
    auto file = std::make_unique<UnixFileOutStream>(fd);
    return std::make_unique<BufferedOutStream>(std::move(file));
}

std::unique_ptr<InStream> in_file_open(const char *path, OpenFlag flag) {
    int oflag = openFlagToUnixFlag.at(flag);
    // oflag |= O_DIRECT;
    int fd = open(path, oflag);
    if (fd < 0) {
        throw std::system_error(errno, std::generic_category());
        // mperror(path);
        return nullptr;
    }
    auto file = std::make_unique<UnixFileInStream>(fd);
    return file;
}


int main() {
    {
        auto p = out_file_open("/tmp/a.txt", OpenFlag::Write);
        p->puts("Hello!\nWorld!\n");
    }
    {
        auto p = in_file_open("/tmp/a.txt", OpenFlag::Read);
        auto s = p->getline('\n');
        printf("%s\n", s.c_str());
        s = p->getline('\n');
        printf("%s\n", s.c_str());
        s = p->getline('\n');
        printf("%s\n", s.c_str());
    }
}