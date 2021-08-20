#ifndef _BPLUS_TREE_H
#define _BPLUS_TREE_H

#include<stdio.h>
#include<stdlib.h>
#include<assert.h>
#include<string.h>
#include<fcntl.h>
#include<ctype.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/stat.h>
/*
最少缓冲数目，我们最多需要5个节点的缓冲足矣
节点自身，左兄弟节点，右兄弟节点，兄弟的兄弟节点，父节点

*/
#define MIN_CACHE_NUM 5

/*
偏移量的枚举
INVALID_OFFSET非法偏移量
*/
enum {
    INVALID_OFFSET = 0xdeadbeef,
};

/*
是否为叶子节点的枚举
叶子节点
非叶子节点
*/
enum {
    BPLUS_TREE_LEAF,
    BPLUS_TREE_NON_LEAF = 1,
};

/*
兄弟节点的枚举
左兄弟
右兄弟
*/
enum {
    LEFT_SIBLING,
    RIGHT_SIBLING = 1,
};

/*16位数据宽度*/
#define ADDR_STR_WIDTH 16



/*得到struct bplus_tree内free_blocks的偏移量*/
#define list_entry(ptr, type, member) \
        ((type *)((char *)(ptr) - (size_t)(&((type *)0)->member)))

/*得到struct bplus_tree内free_blocks->next的偏移量*/
#define list_first_entry(ptr, type, member) \
    list_entry((ptr)->next, type, member)

/*得到struct bplus_tree内free_blocks->prev的偏移量*/
#define list_last_entry(ptr, type, member) \
    list_entry((ptr)->prev, type, member)

#define list_for_each(pos, head) \
        for (pos = (head)->next; pos != (head); pos = pos->next)

#define list_for_each_safe(pos, n, head) \
        for (pos = (head)->next, n = pos->next; pos != (head); \
                pos = n, n = pos->next)

typedef int key_t;

typedef char key_t_arr[128];

/*
链表头部
记录前一个节点和后一个节点
*/
struct list_head {
    struct list_head *prev, *next;
};

/*
链表头部初始化
前一个节点和后一个节点均指向自己
*/
static inline void list_init(struct list_head *link) {
    link->prev = link;
    link->next = link;
}

/*
添加一个节点
*/
static inline void __list_add(struct list_head *link, struct list_head *prev, struct list_head *next) {
    link->next = next;
    link->prev = prev;
    next->prev = link;
    prev->next = link;
}


/*
删除一个节点
*/
static inline void __list_del(struct list_head *prev, struct list_head *next) {
    prev->next = next;
    next->prev = prev;
}

/*
添加一个节点
*/
static inline void list_add(struct list_head *link, struct list_head *prev) {
    __list_add(link, prev, prev->next);
}

/*
添加头节点
*/
static inline void list_add_tail(struct list_head *link, struct list_head *head) {
    __list_add(link, head->prev, head);
}

/*
从链表中删除一个节点
并初始化被删除的节点
*/
static inline void list_del(struct list_head *link) {
    __list_del(link->prev, link->next);
    list_init(link);
}

/*
判断是否为空列表
*/
static inline int list_empty(const struct list_head *head) {
    return head->next == head;
}

/*
B+树节点结构体
偏移量指在.index文件中的偏移量，用文件的大小来设置
B+树叶子节点后面会跟随key和data
B+树非叶子节点后面会更随key和ptr
off_t-----------------------------------32位long int类型
off_t self------------------------------记录自身节点偏移量
off_t parent----------------------------记录父亲节点偏移量
off_t prev------------------------------记录上一个节点偏移量
off_t next------------------------------记录下一个节点偏移量
int type--------------------------------记录节点类型：叶子节点或者非叶子节点
int children----------------------------如果是叶子节点记录节点内键值个数，不是就记录分支数量(即指针ptr的数量)
*/
typedef struct bplus_node {
    off_t self;
    off_t parent;
    off_t prev;
    off_t next;
    int type;
    /* If leaf node, it specifies  count of entries,
     * if non-leaf node, it specifies count of children(branches) */
    int children;
} bplus_node;


/*
struct list_head link---------链表头部，指向上一个节点和下一个节点
off_t offset------------------记录偏移地址
*/
//空洞节点列表
//空洞是指上次删除节点的标记，可用于下次新插入节点的位置，优先于追加
typedef struct free_block {
    struct list_head link;
    off_t offset;
} free_block;

/*
定义B+树信息结构体
char *caches------------------------节点缓存，存放B+树节点的内存缓冲，最少5个，包括：自身节点，父节点，左兄弟节点，右兄弟节点，兄弟的兄弟节点
int used[MIN_CACHE_NUM]-------------可用缓存个数
char filename[1024];----------------文件名字
int fd------------------------------文件描述符指向index
int level---------------------------文件等级
off_t root--------------------------B+树根节点
off_t file_size---------------------文件大小
struct list_head free_blocks--------链表指针
*/
typedef struct bplus_tree {
    char *caches;
    int used[MIN_CACHE_NUM];
    char filename[1024];
    int fd;
    int level;
    off_t root;
    off_t file_size;
    struct list_head free_blocks;
} bplus_tree;

//*
//关键字key为字符数组的操作方法
//*/
//long bplus_tree_get_str(struct bplus_tree *tree, key_t_arr key);
//
//int bplus_tree_put_str(struct bplus_tree *tree, key_t_arr key, long data);
//
//long bplus_tree_get_range_str(struct bplus_tree *tree, key_t_arr key1, key_t_arr key2);
//
//struct bplus_tree *bplus_tree_init_str(char *filename, int block_size);
//
//void bplus_tree_deinit_str(struct bplus_tree *tree);

/*
关键字key为int的操作方法
*/
long bplus_tree_get(struct bplus_tree *tree, key_t key);

int bplus_tree_put(struct bplus_tree *tree, key_t key, long data);

long *bplus_tree_get_range(struct bplus_tree *tree, key_t key1, key_t key2, int *amount);

long *bplus_tree_get_more_than(struct bplus_tree *tree, key_t key, int *amount);

long *bplus_tree_less_than(struct bplus_tree *tree, key_t key, int *amount);

struct bplus_tree *bplus_tree_init(char *filename, int block_size);

void bplus_tree_deinit(struct bplus_tree *tree);

/*
 common
 */
int bplus_open(char *filename);

void bplus_close(int fd);

off_t str_to_hex(char *c, int len);

void hex_to_str(off_t offset, char *buf, int len);

off_t offset_load(int fd);

ssize_t offset_store(int fd, off_t offset);

/*_BPLUS_TREE_H*/
#endif  
