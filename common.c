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
    char buf[ADDR_STR_WIDTH] = {0};
    char *p = memcpy(buf, t_ptr + offset, sizeof(buf));
    return p != NULL ? str_to_hex(buf, sizeof(buf)) : INVALID_OFFSET;
}

/*
存储B+相关数据
*/
//where used
void offset_store(char *t_ptr, off_t offset) {
    char buf[ADDR_STR_WIDTH] = {0};
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
    char *tree_addr = NULL;

    off_t tree_off_t = 0;
    off_t block_size = 0;

    int type = get_tree_type(tree_boot_addr);
    printf("type is %d\n", type);
    tree_off_t = get_boot_size(tree_boot_addr);
    tree_addr = tree_boot_addr + tree_off_t;
    printf("boot addr %p\n", tree_boot_addr);
    printf("off_t %lld\n tree_addr %p\n", tree_off_t, tree_addr);

    switch (type) {
        case INT_TREE_TYPE:
            printf("tree_off_t is %lld\n", tree_off_t);
            block_size = get_tree_block_size(tree_boot_addr);
            printf("block_size is %lld\n", block_size);

            tree = bplus_tree_load(tree_addr, tree_boot_addr, block_size);

            break;
        case STRING_TREE_TYPE:
            printf("tree_off_t is %lld\n", tree_off_t);
            block_size = get_tree_block_size(tree_boot_addr);
            printf("block_size is %lld\n", block_size);
            tree = bplus_tree_load_str(tree_addr, tree_boot_addr, block_size);
            break;
        default:
            return NULL;
    }
    return tree;
}

char *ltoa(long num, char *str, int radix) {
    char index[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";//索引表
    unsigned unum;//存放要转换的整数的绝对值,转换的整数可能是负数
    int i = 0, j, k;//i用来指示设置字符串相应位，转换之后i其实就是字符串的长度；转换后顺序是逆序的，有正负的情况，k用来指示调整顺序的开始位置;j用来指示调整顺序时的交换。

    //获取要转换的整数的绝对值
    if (radix == 10 && num < 0)//要转换成十进制数并且是负数
    {
        unum = (unsigned) -num;//将num的绝对值赋给unum
        str[i++] = '-';//在字符串最前面设置为'-'号，并且索引加1
    } else unum = (unsigned) num;//若是num为正，直接赋值给unum

    //转换部分，注意转换后是逆序的
    do {
        str[i++] = index[unum % (unsigned) radix];//取unum的最后一位，并设置为str对应位，指示索引加1
        unum /= radix;//unum去掉最后一位

    } while (unum);//直至unum为0退出循环

    str[i] = '\0';//在字符串最后添加'\0'字符，c语言字符串以'\0'结束。

    //将顺序调整过来
    if (str[0] == '-') k = 1;//如果是负数，符号不用调整，从符号后面开始调整
    else k = 0;//不是负数，全部都要调整

    char temp;//临时变量，交换两个值时用到
    for (j = k; j <= (i - 1) / 2; j++)//头尾一一对称交换，i其实就是字符串的长度，索引最大值比长度少1
    {
        temp = str[j];//头部赋值给临时变量
        str[j] = str[i - 1 + k - j];//尾部赋值给头部
        str[i - 1 + k - j] = temp;//将临时变量的值(其实就是之前的头部值)赋给尾部
    }

    return str;//返回转换后的字符串

}

char *itoa(int num, char *str, int radix) {
    char index[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";//索引表
    unsigned unum;//存放要转换的整数的绝对值,转换的整数可能是负数
    int i = 0, j, k;//i用来指示设置字符串相应位，转换之后i其实就是字符串的长度；转换后顺序是逆序的，有正负的情况，k用来指示调整顺序的开始位置;j用来指示调整顺序时的交换。

    //获取要转换的整数的绝对值
    if (radix == 10 && num < 0)//要转换成十进制数并且是负数
    {
        unum = (unsigned) -num;//将num的绝对值赋给unum
        str[i++] = '-';//在字符串最前面设置为'-'号，并且索引加1
    } else unum = (unsigned) num;//若是num为正，直接赋值给unum

    //转换部分，注意转换后是逆序的
    do {
        str[i++] = index[unum % (unsigned) radix];//取unum的最后一位，并设置为str对应位，指示索引加1
        unum /= radix;//unum去掉最后一位

    } while (unum);//直至unum为0退出循环

    str[i] = '\0';//在字符串最后添加'\0'字符，c语言字符串以'\0'结束。

    //将顺序调整过来
    if (str[0] == '-') k = 1;//如果是负数，符号不用调整，从符号后面开始调整
    else k = 0;//不是负数，全部都要调整

    char temp;//临时变量，交换两个值时用到
    for (j = k; j <= (i - 1) / 2; j++)//头尾一一对称交换，i其实就是字符串的长度，索引最大值比长度少1
    {
        temp = str[j];//头部赋值给临时变量
        str[j] = str[i - 1 + k - j];//尾部赋值给头部
        str[i - 1 + k - j] = temp;//将临时变量的值(其实就是之前的头部值)赋给尾部
    }

    return str;//返回转换后的字符串

}

char *get_key(int tree_id, long page_index, char *key, int *len) {
    itoa(tree_id, key, 10);
    //get length
    int i = 0;
    while (key[i] != '\0') {
        i++;
    }
    //put '_' in key
    key[i++] = '_';

    //cat page_index to key
    int k = 0;
    char temp[16] = {0};
    ltoa(page_index, temp, 10);
    while (temp[k] != '\0') {
        key[i] = temp[k];
        i++;
        k++;
    }
    *len=i;
    return key;
}
