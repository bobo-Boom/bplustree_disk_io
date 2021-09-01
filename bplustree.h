#ifndef _BPLUS_TREE_H
#define _BPLUS_TREE_H

#include<stdio.h>
#include<stdlib.h>
#include<assert.h>
#include<string.h>
#include<fcntl.h>
#include<ctype.h>
#include<unistd.h>
#include<sys/stat.h>
#include<sys/mman.h>
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

/*
关键字key类型
*/
enum {
    INT_TREE_TYPE,
    STRING_TREE_TYPE = 1,
};


/*16位数据宽度*/
#define ADDR_STR_WIDTH 16

/*tree boot 配置文件偏移量*/
#define BOOT_FILE_SIZE_OFF 0*ADDR_STR_WIDTH
#define TREE_ROOT_OFF 1*ADDR_STR_WIDTH
#define TREE_ID_OFF 2*ADDR_STR_WIDTH
#define TREE_TYPE_OFF  3*ADDR_STR_WIDTH
#define BLOCK_SIZE_OFF 4*ADDR_STR_WIDTH
#define TREE_FILE_SIZE_OFF 5*ADDR_STR_WIDTH


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
    char *fd;
    int level;
    off_t tree_id;
    off_t key_type;
    off_t root;
    off_t file_size;
    struct list_head free_blocks;
} bplus_tree;

/*
关键字key为字符数组的操作方法
*/
long bplus_tree_get_str(struct bplus_tree *tree, key_t_arr key);

long bplus_tree_get_range_str(struct bplus_tree *tree, key_t_arr key1, key_t_arr key2);

struct bplus_tree *bplus_tree_init_str(char *filename, int block_size);

void bplus_tree_deinit_str(struct bplus_tree *tree, char *tree_addr, char *tree_boot_addr);

struct bplus_tree *bplus_tree_load_str(char *tree_addr, char *tree_boot_addr, int block_size);


/*
关键字key为int的操作方法
*/
long bplus_tree_get(struct bplus_tree *tree, key_t key);

long *bplus_tree_get_range(struct bplus_tree *tree, key_t key1, key_t key2, int *amount);

long *bplus_tree_get_more_than(struct bplus_tree *tree, key_t key, int *amount);

long *bplus_tree_less_than(struct bplus_tree *tree, key_t key, int *amount);

void bplus_tree_deinit(struct bplus_tree *tree, char *tree_addr, char *tree_boot_addr);

struct bplus_tree *bplus_tree_load(char *tree_addr, char *tree_boot_addr, int block_size);

/*
 common
 */

off_t str_to_hex(char *c, int len);

void hex_to_str(off_t offset, char *buf, int len);


off_t offset_load(char *t_ptr, off_t offset);

void offset_store(char *fd, off_t offset);


int get_file_size(char *filename);

char *mmap_btree_file(char *file_name);

void munmap_btree_file(char *m_ptr, off_t file_size);


struct bplus_tree *get_tree(char *tree_boot_addr);
/**/
int is_leaf(struct bplus_node *node);

/*
 获取tree配置信息
 BootFileSize|TreeRoot|TreeId|KyeType|BlockSize|TreeFileSize
 */
off_t get_boot_size(char *tree_boot_addr);

off_t get_tree_root(char *tree_boot_addr);

off_t get_tree_id(char *tree_boot_addr);

off_t get_tree_type(char *tree_boot_addr);

off_t get_tree_block_size(char *tree_boot_addr);

off_t get_tree_size(char *tree_boot_addr);

/*_BPLUS_TREE_H*/
#endif  
