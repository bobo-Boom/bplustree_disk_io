#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<time.h>
#include<math.h>
#include<sys/mman.h>
#include"bplustree.h"

/*
B+树设置结构体
char filename[1024]----文件名字
int block_size---------文件大小
*/
struct bplus_tree_config {
    char filename[1024];
    int block_size;
};

int get_file_size(char *filename) {
    struct stat temp;
    stat(filename, &temp);
    return temp.st_size;
}


char *mmap_btree_file(char *file_name) {

    int fd, i, file_size;
    char *p_map, *p_index;

    file_size = get_file_size(file_name);
    fd = open(file_name, O_RDWR, 0644);

    p_map = (char *) mmap(NULL, file_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
//    p_index = p_map;
//
//    int addr=0;
//    for (i = 0; i < file_size; i++) {
//        printf("%c", *p_index);
//        p_index++;
//    }
//    printf("\n");
    close(fd);

    return p_map;

};

void munmap_btree_file(char *m_ptr, off_t file_size) {

    munmap(m_ptr, file_size);

};

int main(void) {

//    struct bplus_tree_config config;
//    config.block_size = 1024;
//    strcpy(config.filename, "./data_str.index");
//    /*定义一个B+树信息结构体*/
//    struct bplus_tree *tree = NULL;
//
//    //str tree
//    //初始化B+树
//    tree = bplus_tree_init_str(config.filename, config.block_size);
//
//    for (int i = 1; i < 1000; i++) {
//        key_t_arr key = {0};
//        sprintf(key, "%d", i);
//        printf(" insert %d\n", i);
//        bplus_tree_put_str(tree, key, i);
//
//    }
//    for (int i = 1; i < 1000; i++) {
//        key_t_arr key = {0};
//        sprintf(key, "%d", i);
//        long data = bplus_tree_get_str(tree, key);
//        printf("data%d is %ld\n", i, data);
//    }
//
//    bplus_tree_deinit_str(tree);

    //int tree
    char *boot_ptr = mmap_btree_file("./data_int.index.boot");
    char *tree_ptr = mmap_btree_file("./data_int.index");

    struct bplus_tree *tree2 = bplus_tree_load(tree_ptr, boot_ptr, 4096);
    printf("treee2 %p\n", tree2);

    //获取B+树数据
    for (int i = 0; i < 10000; ++i) {
        long ret = bplus_tree_get(tree2, i);
        printf("ret is  %ld\n", ret);
    }

    //范围查询
    int amount = 0;
    long *rets = bplus_tree_get_range(tree2, 3, 10000, &amount);
    for (int i = 0; i < amount; ++i) {
        printf("%ld\n", rets[i]);
    }

    //小于key值的范围查询
    rets = bplus_tree_less_than(tree2, 8888, &amount);
    printf("======================小于范围查询===================\n");
    for (int i = 0; i < amount; ++i) {
        printf("%ld\n", rets[i]);
    }

    //大于范围查询
    rets = bplus_tree_get_more_than(tree2, 300, &amount);
    printf("=====================大于范围查询===================\n");
    for (int i = 0; i < amount; ++i) {
        printf("%ld\n", rets[i]);
    }

    return 0;
}


