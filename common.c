#include "bplustree.h"
/*
打开B+树
返回fd
*/
int bplus_open(char *filename) {
    return open(filename, O_CREAT | O_RDWR, 0644);
}

/*
关闭B+树
*/
void bplus_close(int fd) {
    close(fd);
}

/*
字符串转16进制
*/
off_t str_to_hex(char *c, int len) {
    off_t offset = 0;
    while (len-- > 0) {
        if (isdigit(*c)) {
            offset = offset * 16 + *c - '0';
        } else if (isxdigit(*c)) {
            if (islower(*c)) {
                offset = offset * 16 + *c - 'a' + 10;
            } else {
                offset = offset * 16 + *c - 'A' + 10;
            }
        }
        c++;
    }
    return offset;
}

/*
16进制转字符串
*/
void hex_to_str(off_t offset, char *buf, int len) {
    const static char *hex = "0123456789ABCDEF";
    while (len-- > 0) {
        buf[len] = hex[offset & 0xf];
        offset >>= 4;
    }
}

/*
加载文件数据，每16位记录一个信息
如果读取到数据，即len>0，返回数据
如果没有读到数据，即len<=0，返回INVALID_OFFSET
*/
off_t offset_load(int fd) {
    char buf[ADDR_STR_WIDTH];
    ssize_t len = read(fd, buf, sizeof(buf));
    return len > 0 ? str_to_hex(buf, sizeof(buf)) : INVALID_OFFSET;
}

/*
存储B+相关数据
*/
ssize_t offset_store(int fd, off_t offset) {
    char buf[ADDR_STR_WIDTH];
    hex_to_str(offset, buf, sizeof(buf));
    return write(fd, buf, sizeof(buf));
}