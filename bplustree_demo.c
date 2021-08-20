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

int get_file_size(char* filename){
    struct stat temp;
    stat(filename,&temp);
    return temp.st_size;
}


char *mmap_btree_file(char *file_name) {

    int fd, i,file_size;
    char *p_map, *p_index;

    file_size= get_file_size(file_name);
    fd = open(file_name, O_RDWR, 0644);

    p_map = (char *) mmap(NULL, file_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
    p_index = p_map;

    for (i = 0; i < file_size; i++) {
        printf("%c", *p_index);
        p_index++;
    }
    printf("\n");
    close(fd);

    return p_map;

};

void munmap_btree_file(char *m_ptr, off_t file_size) {

    munmap(m_ptr, file_size);

};
int get_file_size(char* filename){
    struct stat temp;
    stat(filename,&temp);
    return temp.st_size;
}


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
//    struct bplus_tree *tree1 = NULL;
//    tree1 = bplus_tree_init("./data_int.index", 4096);
//    int amount = 0;
//
//    //插入数据
//    for (int i = 0; i < 10000; i++) {
//        bplus_tree_put(tree1, i, i);
//    }
////    //获取数据
////    long pageIndex = bplus_tree_get(tree1, 10000);
////    printf("pageIndex is :%ld\n", pageIndex);
//
//
//    long *rets = bplus_tree_get_range(tree1, 3000, 4009, &amount);
//    for (int i = 0; i < amount; i++) {
//        printf("range results : %ld\n", rets[i]);
//    }
//    free(rets);
//
//    bplus_tree_deinit(tree1);
    int file_size;
    char *boot_ptr = mmap_btree_file("./data_int.index.boot");
    char *boot_ptr = mmap_btree_file("./data_int.boot");

    file_size = get_file_size("./data_int.index.boot");



    munmap_btree_file(m_ptr, 5);
    return 0;
}


