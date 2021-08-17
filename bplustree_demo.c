#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<time.h>
#include<math.h>

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


int main(void) {


    struct bplus_tree_config config;
    config.block_size = 1024;
    strcpy(config.filename, "./data_str.index");
    /*定义一个B+树信息结构体*/
    struct bplus_tree *tree = NULL;

    //str tree
    //初始化B+树
    tree = bplus_tree_init_str(config.filename, config.block_size);

    for (int i = 1; i < 1000; i++) {
        key_t_arr key = {0};
        sprintf(key, "%d", i);
        printf(" insert %d\n", i);
        bplus_tree_put_str(tree, key, i);

    }
    for (int i = 1; i < 1000; i++) {
        key_t_arr key = {0};
        sprintf(key, "%d", i);
        long data = bplus_tree_get_str(tree, key);
        printf("data%d is %ld\n", i, data);
    }

    bplus_tree_deinit(tree);


    //int tree
    struct bplus_tree *tree1 = NULL;
    tree1 = bplus_tree_init("./data_int.index", 4096);
    int amount = 0;

    //插入数据
    for (int i = 0; i < 10000; i++) {
        bplus_tree_put(tree1, i, i);
    }


    //获取数据
    long pageIndex = bplus_tree_get(tree, 10000);
    printf("pageIndex is :%ld\n", pageIndex);


    long *rets = bplus_tree_get_range(tree1, 3000, 4009, &amount);
    for (int i = 0; i < amount; i++) {
        printf("range results : %ld\n", rets[i]);
    }
    free(rets);

    bplus_tree_deinit(tree1);

    return 0;
}
