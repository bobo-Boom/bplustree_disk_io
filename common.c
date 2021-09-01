#include "bplustree.h"

/*
判断是否为叶子节点
*/
int is_leaf(struct bplus_node *node) {
    return node->type == BPLUS_TREE_LEAF;
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
*/
//where used
off_t offset_load(char *t_ptr, off_t offset) {
    char buf[ADDR_STR_WIDTH];
    char *p = memcpy(buf, t_ptr + offset, sizeof(buf));
    return p != NULL ? str_to_hex(buf, sizeof(buf)) : INVALID_OFFSET;
}

/*
存储B+相关数据
*/
//where used
void offset_store(char *t_ptr, off_t offset) {
    char buf[ADDR_STR_WIDTH];
    hex_to_str(offset, buf, sizeof(buf));
    //return write(fd, buf, sizeof(buf));
    //todo
    memcpy(t_ptr + offset, buf, sizeof(buf));
}

/***
 * 获取tree配置信息
 * @param tree_boot_addr
 * @return off_t
 */

off_t get_boot_size(char *tree_boot_addr) {

    off_t offset = offset_load(tree_boot_addr, BOOT_FILE_SIZE_OFF);
    return offset;
}

off_t get_tree_root(char *tree_boot_addr) {

    off_t offset = offset_load(tree_boot_addr, TREE_ROOT_OFF);
    return offset;
}

off_t get_tree_id(char *tree_boot_addr) {

    off_t offset = offset_load(tree_boot_addr, TREE_ID_OFF);
    return offset;
}

off_t get_tree_type(char *tree_boot_addr) {

    off_t offset = offset_load(tree_boot_addr, TREE_TYPE_OFF);
    return offset;
}

off_t get_tree_block_size(char *tree_boot_addr) {

    off_t offset = offset_load(tree_boot_addr, BLOCK_SIZE_OFF);
    return offset;
}

off_t get_tree_size(char *tree_boot_addr) {

    off_t offset = offset_load(tree_boot_addr, TREE_FILE_SIZE_OFF);
    return offset;
}


int get_file_size(char *filename) {
    struct stat temp;
    stat(filename, &temp);
    return temp.st_size;
}


char *mmap_btree_file(char *file_name) {

    int fd, file_size;
    char *p_map;

    file_size = get_file_size(file_name);
    fd = open(file_name, O_RDWR, 0644);

    p_map = (char *) mmap(NULL, file_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
    close(fd);
    return p_map;

};

void munmap_btree_file(char *m_ptr, off_t file_size) {
    munmap(m_ptr, file_size);
}


struct bplus_tree *get_tree(char *tree_boot_addr) {
    struct bplus_tree *tree = NULL;
    off_t tree_off_t = 0;
    off_t block_size = 0;

    int type = get_tree_type(tree_boot_addr);
    printf("type is %d\n",type);

    switch (type) {
        case INT_TREE_TYPE:
            tree_off_t = get_boot_size(tree_boot_addr);
            printf("tree_off_t is %lld\n",tree_off_t);
            block_size = get_tree_block_size(tree_boot_addr);
            printf("block_size is %lld\n",block_size);
            tree = bplus_tree_load(tree_boot_addr + tree_off_t, tree_boot_addr, block_size);

            break;
        case STRING_TREE_TYPE:
            tree_off_t = get_boot_size(tree_boot_addr);
            printf("tree_off_t is %lld\n",tree_off_t);
            block_size = get_tree_block_size(tree_boot_addr);
            printf("block_size is %lld\n",block_size);
            char* tree_addr=tree_boot_addr+tree_off_t;
            tree = bplus_tree_load_str(tree_boot_addr, tree_boot_addr, block_size);
            break;
        default:
            return NULL;
    }
    return tree;
}