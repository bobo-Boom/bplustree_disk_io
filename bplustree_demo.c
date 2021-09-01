#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<time.h>
#include<math.h>
#include"bplustree.h"


int main(void) {

    /*定义一个B+树信息结构体*/
    struct bplus_tree *tree = NULL;

    //str tree
    //初始化B+树
    char *boot_ptr = mmap_btree_file("./data_str.index.boot");
    char *tree_ptr = mmap_btree_file("./data_str.index");

    tree = bplus_tree_load_str(tree_ptr, boot_ptr, 4096);


    for (int i = 1; i < 1000; i++) {
        key_t_arr key = {0};
        sprintf(key, "%d", i);
        long data = bplus_tree_get_str(tree, key);
        printf("data_str %d is %ld\n", i, data);
    }

    //清空内存
    //bplus_tree_deinit_str(tree,tree_ptr,boot_ptr);

    //int tree
    char *boot_ptr1 = mmap_btree_file("./data.index.boot");
    char *tree_ptr1 = mmap_btree_file("./data.index");

    struct bplus_tree *tree1 = bplus_tree_load(tree_ptr1, boot_ptr1, 4096);

    //获取B+树数据
    for (int i = 1; i < 10000; ++i) {
        long ret = bplus_tree_get(tree1, i);
        printf("ret is  %ld\n", ret);
    }

    //范围查询
    int amount = 0;
    long *rets = bplus_tree_get_range(tree1, 3, 10000, &amount);
    for (int i = 0; i < amount; ++i) {
        printf("%ld\n", rets[i]);
    }

    //小于key值的范围查询
    rets = bplus_tree_less_than(tree1, 8888, &amount);
    printf("======================小于范围查询===================\n");
    for (int i = 0; i < amount; ++i) {
        printf("%ld\n", rets[i]);
    }

    //大于范围查询
    rets = bplus_tree_get_more_than(tree1, 300, &amount);
    printf("=====================大于范围查询===================\n");
    for (int i = 0; i < amount; ++i) {
        printf("%ld\n", rets[i]);
    }

    return 0;
}


