#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<time.h>
#include<math.h>

#include"bplustree.h"

/*
B+树设置结构体
char filename[1024]----文件名字
int block_size---------文件大小？ 应该是节点缓冲大小
*/
struct bplus_tree_config {
    char filename[1024];
    int block_size;
};

/*计时器结构体，定义在<time.h>*/
struct timespec t1, t2;

/*
刷新stdin缓冲区
*/
static void stdin_flush(void) {
    int c;
    while ((c = getchar()) != '\n' && c != EOF) {
        continue;
    }
}


int main(void) {

    //B+树初始化。
    struct bplus_tree *tree = NULL;
    tree = bplus_tree_init("./data.index", 4096);
    int amount = 0;

    //插入数据
    for (int i = 0; i < 10000; i++) {
        bplus_tree_put(tree, i, i);
    }

//
//    //获取数据
//    long pageIndex = bplus_tree_get(tree, 10000);
//    printf("pageIndex is :%ld\n", pageIndex);
//


    long *rets = bplus_tree_get_range(tree, 3000, 4009, &amount);
    for (int i = 0; i < amount; i++) {
        printf("range results : %ld\n", rets[i]);
    }
    free(rets);


//     printf("大于3000\n");
//     long * results=bplus_tree_get_more_than(tree, 9800,&amount);
//     printf("amoount is %d\n",amount);
//     for (int i = 0; i < amount; i++) {
//        printf("data is %ld\n",results[i]);
//     }
//     free(results);

//    printf("小于9999");
//    long *results = bplus_tree_less_than(tree, 9999, &amount);
//    printf("amount  is  ================= %d\n", amount);
//    for (int i = 0; i < amount; i++) {
//        printf("data is %ld\n", results[i]);
//    }
//    free(results);

    //关闭B+树
    bplus_tree_deinit(tree);

    return 0;
}
