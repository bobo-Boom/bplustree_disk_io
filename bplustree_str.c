#include"bplustree.h"


/*
内存结构
 ---------------------------------------------------------------------------------------------------
|			|		|		|		|		|		|		|		|		|		|		|		|
|叶子节点	|  node | key	| key 	| key 	| key 	| key 	| data 	| data 	| data 	| data 	| data 	|
|			|		|		|		|		|		|		|		|		|		|		|		|
|---------------------------------------------------------------------------------------------------
|			|		|		|		|		|		|		|		|		|		|		|		|
|非叶子节点	|  node | key 	| key 	| key 	| key 	|  ptr  |  ptr  |  ptr  |  ptr  |  ptr	|  ptr	|
|			|		|		|		|		|		|		|		|		|		|		|		|
 ---------------------------------------------------------------------------------------------------
key和data的个数由_max_entries_arr决定：_max_entries_arr = (_block_size_arr - sizeof(node)) / (sizeof(key_t_arr) + sizeof(long));
一个节点的大小由_block_size_arr决定，容量要包含1个node结构体和3个及以上的key，data
*/

/*16位数据宽度*/
#define ADDR_STR_WIDTH 16

/*B+树节点node末尾的偏移地址，即key的首地址*/
#define offset_ptr_arr(node) ((char *) (node) + sizeof(*node))

/*返回B+树节点末尾地址，强制转换为key_t_arr*，即key的指针*/
#define key_arr(node) ((key_t_arr *)offset_ptr_arr(node))

/*返回B+树节点和key末尾地址，强制转换为long*，即data指针*/
#define data_arr(node) ((long *)(offset_ptr_arr(node) + _max_entries_arr * sizeof(key_t_arr)))

/*返回最后一个key的指针，用于非叶子节点的指向，即第一个ptr*/
#define sub_arr(node) ((off_t *)(offset_ptr_arr(node) + (_max_order_arr - 1) * sizeof(key_t_arr)))

/*
全局静态变量
_block_size_arr--------------------每个节点的大小(容量要包含1个node和3个及以上的key，data)
_max_entries_arr-------------------叶子节点内包含个数最大值
_max_order_arr---------------------非叶子节点内最大关键字个数
*/
static int _block_size_arr;
static int _max_entries_arr;
static int _max_order_arr;

/*
判断是否为叶子节点
*/
static inline int is_leaf_str(struct bplus_node *node) {
    return node->type == BPLUS_TREE_LEAF;
}
/*
键值二分查找
*/
static int key_binary_search_str(struct bplus_node *node, key_t_arr target) {
    key_t_arr *arr = key_arr(node);
    /*叶子节点：len；非叶子节点：len-1;非叶子节点的key少一个，用于放ptr*/
    int len = is_leaf_str(node) ? node->children : node->children - 1;
    int low = -1;
    int high = len;
    while (low + 1 < high) {
        int mid = low + (high - low) / 2;
        if (strcmp(target, arr[mid]) > 0) {
            low = mid;
        } else {
            high = mid;
        }
    }
    if (high >= len || strcmp(target, arr[high]) != 0) {
        return -high - 1;
    } else {
        return high;
    }
}

/*
查找键值在父节点的第几位
*/
static inline int parent_key_index_str(struct bplus_node *parent, key_t_arr key) {
    int index = key_binary_search_str(parent, key);
    return index >= 0 ? index : -index - 2;
}

/*
占用缓存区，与cache_defer_str对应
占用内存，以供使用
缓存不足，assert(0)直接终止程序
*/
static inline struct bplus_node *cache_refer_str(struct bplus_tree *tree) {
    int i;
    for (i = 0; i < MIN_CACHE_NUM; i++) {
        if (!tree->used[i]) {
            tree->used[i] = 1;
            char *buf = tree->caches + _block_size_arr * i;
            return (struct bplus_node *) buf;
        }
    }
    assert(0);
}

/*
释放缓冲区，与cache_refer_str对应
将used重置，能够存放接下来的数据
*/
static inline void cache_defer_str(struct bplus_tree *tree, struct bplus_node *node) {
    char *buf = (char *) node;
    int i = (buf - tree->caches) / _block_size_arr;
    tree->used[i] = 0;
}

/*
创建新的节点
*/
static struct bplus_node *node_new_str(struct bplus_tree *tree) {
    struct bplus_node *node = cache_refer_str(tree);
    node->self = INVALID_OFFSET;
    node->parent = INVALID_OFFSET;
    node->prev = INVALID_OFFSET;
    node->next = INVALID_OFFSET;
    node->children = 0;
    return node;
}

/*
创建新的非叶子节点
*/
static inline struct bplus_node *non_leaf_new_str_str(struct bplus_tree *tree) {
    struct bplus_node *node = node_new_str(tree);
    node->type = BPLUS_TREE_NON_LEAF;
    return node;
}

/*
创建新的叶子节点
*/
static inline struct bplus_node *leaf_new_str(struct bplus_tree *tree) {
    struct bplus_node *node = node_new_str(tree);
    node->type = BPLUS_TREE_LEAF;
    return node;
}

/*
根据偏移量从.index获取节点的全部信息，加载到缓冲区
偏移量非法则返回NULL
*/
static struct bplus_node *node_fetch_str(struct bplus_tree *tree, off_t offset) {
    if (offset == INVALID_OFFSET) {
        return NULL;
    }

    struct bplus_node *node = cache_refer_str(tree);
    int len = pread(tree->fd, node, _block_size_arr, offset);
    assert(len == _block_size_arr);
    return node;
}

/*
通过节点的偏移量从.index中获取节点的全部信息
*/
static struct bplus_node *node_seek_str(struct bplus_tree *tree, off_t offset) {
    /*偏移量不合法*/
    if (offset == INVALID_OFFSET) {
        return NULL;
    }

    /*偏移量合法*/
    int i;
    for (i = 0; i < MIN_CACHE_NUM; i++) {
        if (!tree->used[i]) {
            char *buf = tree->caches + _block_size_arr * i;
            int len = pread(tree->fd, buf, _block_size_arr, offset);
            assert(len == _block_size_arr);
            return (struct bplus_node *) buf;
        }
    }
    assert(0);
}

/*
B+树节点保存
将其保存到index
并将内存内的缓冲区释放
往tree->fd的文件描述符写入
node指向的节点信息和其后面跟随的节点内容
长度为_block_size_arr
偏移量为node->self
*/
static inline void node_flush_str(struct bplus_tree *tree, struct bplus_node *node) {
    if (node != NULL) {
        int len = pwrite(tree->fd, node, _block_size_arr, node->self);
//        if(is_leaf_str(node)){
//            printf("\nnode_flush_str  key: ");
//            for (int i = 0; i < node->children; ++i) {
//                printf("%s ",key_arr(node)[i]);
//            }
//            printf("\n");
//
//            printf("\nnode_flush_str  data: ");
//            for (int i = 0; i < node->children; ++i) {
//                printf("%ld ",data_arr(node)[i]);
//            }
//            printf("\n");
//        }

        assert(len == _block_size_arr);
        cache_defer_str(tree, node);
    }
}

/*
节点加入到树，为新节点分配新的偏移量，即文件大小
判断链表是否为空，判断是否有空闲区块
空闲区块首地址保存在.boot
*/
static off_t new_node_append_str(struct bplus_tree *tree, struct bplus_node *node) {
    /*.index无空闲区块*/
    if (list_empty(&tree->free_blocks)) {
        node->self = tree->file_size;
        tree->file_size += _block_size_arr;
        /*.inedx有空闲区块*/
    } else {
        struct free_block *block;
        block = list_first_entry(&tree->free_blocks, struct free_block, link);
        list_del(&block->link);
        node->self = block->offset;
        free(block);
    }
    return node->self;
}

/*
从.index删除整个节点，多出一块空闲区块，添加到B+树信息结构体
struct bplus_tree *tree-------------------B+树信息结构体
struct bplus_node *node-------------------要被删除的节点
struct bplus_node *left-------------------左孩子
struct bplus_node *right------------------右孩子
*/
static void
node_delete_str(struct bplus_tree *tree, struct bplus_node *node, struct bplus_node *left, struct bplus_node *right) {
    if (left != NULL) {
        /*左右孩子均存在*/
        if (right != NULL) {
            left->next = right->self;
            right->prev = left->self;
            node_flush_str(tree, right);
            /*左孩子存在，右孩子不存在*/
        } else {
            left->next = INVALID_OFFSET;
        }
        node_flush_str(tree, left);
    } else {
        /*没有孩子节点*/
        if (right != NULL) {
            right->prev = INVALID_OFFSET;
            node_flush_str(tree, right);
        }
    }

    assert(node->self != INVALID_OFFSET);
    struct free_block *block = malloc(sizeof(*block));
    assert(block != NULL);
    /*空闲区块指向被删除节点在.index中的偏移量*/
    block->offset = node->self;
    /*添加空闲区块*/
    list_add_tail(&block->link, &tree->free_blocks);
    /*释放缓冲区*/
    cache_defer_str(tree, node);
}

/*
更新非叶子节点的指向
struct bplus_tree *tree----------------B+树信息结构体
struct bplus_node *parent--------------父节点
int index------------------------------插入位置
struct bplus_node *sub_node------------要插入的分支
*/
static inline void sub_node_update_str(struct bplus_tree *tree, struct bplus_node *parent,
                                   int index, struct bplus_node *sub_node) {
    assert(sub_node->self != INVALID_OFFSET);
    sub_arr(parent)[index] = sub_node->self;
    sub_node->parent = parent->self;
    node_flush_str(tree, sub_node);
}

/*
将分裂的非叶子节点的孩子重定向，并写入.index
struct bplus_tree *tree----------------B+树信息结构体
struct bplus_node *parent--------------分裂的新的非叶子节点
off_t sub_offset-----------------------偏移量，即指向子节点的指针
*/
static inline void sub_node_flush_str(struct bplus_tree *tree, struct bplus_node *parent, off_t sub_offset) {
    struct bplus_node *sub_node = node_fetch_str(tree, sub_offset);
    assert(sub_node != NULL);
    sub_node->parent = parent->self;
    node_flush_str(tree, sub_node);
}

/*
B+树查找
*/
static long bplus_tree_search_str(struct bplus_tree *tree, key_t_arr key) {
    int ret = -1;
    /*返回根节点的结构体*/
    struct bplus_node *node = node_seek_str(tree, tree->root);
    while (node != NULL) {
        int i = key_binary_search_str(node, key);
        /*到达叶子节点*/
        if (is_leaf_str(node)) {
            ret = i >= 0 ? data_arr(node)[i] : -1;
            break;
            /*未到达叶子节点，循环递归*/
        } else {
            if (i >= 0) {
                node = node_seek_str(tree, sub_arr(node)[i + 1]);
            } else {
                i = -i - 1;
                node = node_seek_str(tree, sub_arr(node)[i]);
            }
        }
    }

    return ret;
}

/*
左节点添加
设置左右兄弟叶子节点的指向，不存在就设置为非法
struct bplus_tree *tree------------B+树信息结构体
struct bplus_node *node------------B+树要分裂的节点
struct bplus_node *left------------B+树左边的新节点
*/
static void left_node_add_str(struct bplus_tree *tree, struct bplus_node *node, struct bplus_node *left) {
    new_node_append_str(tree, left);

    struct bplus_node *prev = node_fetch_str(tree, node->prev);
    if (prev != NULL) {
        prev->next = left->self;
        left->prev = prev->self;
        node_flush_str(tree, prev);
    } else {
        left->prev = INVALID_OFFSET;
    }
    left->next = node->self;
    node->prev = left->self;
}

/*
右节点添加
设置左右兄弟叶子节点的指向，不存在就设置为非法
struct bplus_tree *tree------------B+树信息结构体
struct bplus_node *node------------B+树要分裂的节点
struct bplus_node *right-----------B+树右边的新节点
*/
static void right_node_add_str(struct bplus_tree *tree, struct bplus_node *node, struct bplus_node *right) {
    new_node_append_str(tree, right);

    struct bplus_node *next = node_fetch_str(tree, node->next);
    if (next != NULL) {
        next->prev = right->self;
        right->next = next->self;
        node_flush_str(tree, next);
    } else {
        right->next = INVALID_OFFSET;
    }
    right->prev = node->self;
    node->next = right->self;
}

/*非叶子节点插入，声明*/
static int
non_leaf_insert_str(struct bplus_tree *tree, struct bplus_node *node, struct bplus_node *l_ch, struct bplus_node *r_ch,
                    key_t_arr key);

/*
下一层节点满后分裂，建立新的父节点，添加键值
struct bplus_tree *tree-------------B+树信息结构体
struct bplus_node *l_ch-------------B+树左孩子节点
struct bplus_node *r_ch-------------B+树右孩子节点
key_t_arr key---------------------------后继节点的键值
*/
static int
parent_node_build_str(struct bplus_tree *tree, struct bplus_node *l_ch, struct bplus_node *r_ch, key_t_arr key) {
    /*左右节点均没有父节点*/
    if (l_ch->parent == INVALID_OFFSET && r_ch->parent == INVALID_OFFSET) {
        /*左右节点均没有父节点，建立新的父节点*/
        struct bplus_node *parent = non_leaf_new_str_str(tree);
        //todo
        //key_arr(parent)[0] = key;
        memcpy(key_arr(parent)[0], key, sizeof(key_t_arr));
        sub_arr(parent)[0] = l_ch->self;
        sub_arr(parent)[1] = r_ch->self;
        parent->children = 2;

        /*写入新的父节点，升级B+树信息结构体内的root根节点*/
        tree->root = new_node_append_str(tree, parent);
        l_ch->parent = parent->self;
        r_ch->parent = parent->self;
        tree->level++;

        /*操作完成，将父节点和子节点记入index*/
        node_flush_str(tree, l_ch);
        node_flush_str(tree, r_ch);
        node_flush_str(tree, parent);

        return 0;
        /*右节点没有父节点*/
    } else if (r_ch->parent == INVALID_OFFSET) {
        /*node_fetch_str(tree, l_ch->parent):从.index文件获取*/
        return non_leaf_insert_str(tree, node_fetch_str(tree, l_ch->parent), l_ch, r_ch, key);
        /*左节点没有父节点*/
    } else {

        /*node_fetch_str(tree, r_ch->parent):从.index文件获取*/
        return non_leaf_insert_str(tree, node_fetch_str(tree, r_ch->parent), l_ch, r_ch, key);
    }
}

/*
非叶子节点的分裂插入
insert在spilit左边,insert<spilit
struct bplus_tree *tree------------------B+树信息结构体
struct bplus_node *node------------------原节点
struct bplus_node *left------------------新分裂的节点
struct bplus_node *l_ch------------------左孩子
struct bplus_node *r_ch------------------右孩子
key_t_arr key--------------------------------键值
int insert-------------------------------插入位置
*/
//todo
static char *
non_leaf_split_left_str(struct bplus_tree *tree, struct bplus_node *node, struct bplus_node *left,
                        struct bplus_node *l_ch,
                        struct bplus_node *r_ch, key_t_arr key, int insert) {
    int i;
    //todo
    static key_t_arr split_key = {0};

    /*分裂边界spilit=(len+1)/2*/
    int split = (_max_order_arr + 1) / 2;

    /*左节点添加到树*/
    left_node_add_str(tree, node, left);

    /*重新计算左右兄弟节点的孩子*/
    int pivot = insert;
    left->children = split;
    node->children = _max_order_arr - split + 1;

    /*将原来的insert~spilit的key和data复制到分裂的左兄弟*/
    memmove(key_arr(left)[0], key_arr(node)[0], pivot * sizeof(key_t_arr));
    memmove(&sub_arr(left)[0], &sub_arr(node)[0], pivot * sizeof(off_t));

    /*将原来的insert+1~end的key和data后移1位，方便插入*/
    memmove(key_arr(left)[pivot + 1], key_arr(node)[pivot], (split - pivot - 1) * sizeof(key_t_arr));
    memmove(&sub_arr(left)[pivot + 1], &sub_arr(node)[pivot], (split - pivot - 1) * sizeof(off_t));

    /*将分裂的左节点的孩子重定向，写入.index*/
    for (i = 0; i < left->children; i++) {
        if (i != pivot && i != pivot + 1) {
            sub_node_flush_str(tree, left, sub_arr(left)[i]);
        }
    }

    /*插入新键和子节点，并找到拆分键*/
    //key_arr(left)[pivot] = key;
    //todo
    memcpy(key_arr(left)[pivot], key, sizeof(key_t_arr));
    /*
    插入的非叶子节点有左右两孩子
    判断他们在分裂边界的那一边
    pivot == split - 1：孩子节点在分裂边界的两边
    else：孩子节点均在分裂节点左边
    */
    if (pivot == split - 1) {
        /*
        孩子节点在分裂边界的两边
        更新索引，l_ch放到新分裂的非叶子节点
        r_ch放到原非叶子节点
        */
        sub_node_update_str(tree, left, pivot, l_ch);
        sub_node_update_str(tree, node, 0, r_ch);
        //todo
        //split_key = key;
        memcpy(split_key, key, sizeof(key_t_arr));
    } else {
        /*
        两个新的子节点在分裂左节点
        更新索引，l_ch和r_ch均放到新分裂的非叶子节点
        */
        sub_node_update_str(tree, left, pivot, l_ch);
        sub_node_update_str(tree, left, pivot + 1, r_ch);
        sub_arr(node)[0] = sub_arr(node)[split - 1];
        //todo
        //split_key = key_arr(node)[split - 2];
        memcpy(split_key, key_arr(node)[split - 2], sizeof(key_t_arr));

    }

    /*将原节点分裂边界右边的key和ptr左移*/
    memmove(key_arr(node)[0], key_arr(node)[split - 1], (node->children - 1) * sizeof(key_t_arr));
    memmove(&sub_arr(node)[1], &sub_arr(node)[split], (node->children - 1) * sizeof(off_t));
    //todo
    /*返回前继节点，作为上一层键值*/
    return (char *) split_key;
}

/*
非叶子节点的分裂插入
insert与spilit重叠，insert==spilit
直接分裂，移动操作减少
struct bplus_tree *tree------------------B+树信息结构体
struct bplus_node *node------------------原节点
struct bplus_node *right-----------------新分裂的节点
struct bplus_node *l_ch------------------左孩子
struct bplus_node *r_ch------------------右孩子
key_t_arr key--------------------------------键值
int insert-------------------------------插入位置
*/
//todo
static char *non_leaf_split_right_str1(struct bplus_tree *tree, struct bplus_node *node, struct bplus_node *right,
                                       struct bplus_node *l_ch, struct bplus_node *r_ch, key_t_arr key, int insert) {
    int i;

    /*分裂边界spilit=(len+1)/2*/
    int split = (_max_order_arr + 1) / 2;

    /*新分裂的节点添加到树*/
    right_node_add_str(tree, node, right);

    /*上一层的键值*/
    //todo
    //key_t_arr split_key = key_arr(node)[split - 1];
    static key_t_arr split_key = {0};
    memcpy(split_key, key_arr(node)[split - 1], sizeof(key_t_arr));

    /*重新计算孩子个数*/
    int pivot = 0;
    node->children = split;
    right->children = _max_order_arr - split + 1;

    /*插入key和ptr*/
    // key_arr(right)[0] = key;
    memcpy(key_arr(right)[0], key, sizeof(key_t_arr));
    sub_node_update_str(tree, right, pivot, l_ch);
    sub_node_update_str(tree, right, pivot + 1, r_ch);

    /*复制数据到新的分裂节点*/
    memmove(key_arr(right)[pivot + 1], key_arr(node)[split], (right->children - 2) * sizeof(key_t_arr));
    memmove(&sub_arr(right)[pivot + 2], &sub_arr(node)[split + 1], (right->children - 2) * sizeof(off_t));

    /*重定向父子结点，写入.index*/
    for (i = pivot + 2; i < right->children; i++) {
        sub_node_flush_str(tree, right, sub_arr(right)[i]);
    }

    /*返回上一层键值*/
    return (char *) split_key;
}

/*
非叶子节点的分裂插入
insert在spilit右边,insert>spilit
struct bplus_tree *tree------------------B+树信息结构体
struct bplus_node *node------------------原节点
struct bplus_node *right-----------------新分裂的节点
struct bplus_node *l_ch------------------左孩子
struct bplus_node *r_ch------------------右孩子
key_t_arr key--------------------------------键值
int insert-------------------------------插入位置
*/
//todo
static char *non_leaf_split_right_str2(struct bplus_tree *tree, struct bplus_node *node, struct bplus_node *right,
                                       struct bplus_node *l_ch, struct bplus_node *r_ch, key_t_arr key, int insert) {
    int i;

    /*分裂边界spilit=(len+1)/2*/
    int split = (_max_order_arr + 1) / 2;

    /*右节点添加到树*/
    right_node_add_str(tree, node, right);
    //todo
    /*上一层的键值*/
    //key_t_arr split_key = key_arr(node)[split];
    static key_t_arr split_key = {0};
    memcpy(split_key, key_arr(node)[split], sizeof(key_t_arr));

    /*重新计算孩子个数*/
    int pivot = insert - split - 1;
    node->children = split + 1;
    right->children = _max_order_arr - split;

    /*复制数据到新的分裂节点*/
    memmove(key_arr(right)[0], key_arr(node)[split + 1], pivot * sizeof(key_t_arr));
    memmove(&sub_arr(right)[0], &sub_arr(node)[split + 1], pivot * sizeof(off_t));

    /*插入key和ptr，更新索引*/
//    key_arr(right)[pivot] = key;
    //todo
    memcpy(key_arr(right)[pivot], key, sizeof(key_t_arr));

    sub_node_update_str(tree, right, pivot, l_ch);
    sub_node_update_str(tree, right, pivot + 1, r_ch);

    /*将原节点insert+1~end的数据移动到新分裂的非叶子节点*/
    memmove(key_arr(right)[pivot + 1], key_arr(node)[insert], (_max_order_arr - insert - 1) * sizeof(key_t_arr));
    memmove(&sub_arr(right)[pivot + 2], &sub_arr(node)[insert + 1], (_max_order_arr - insert - 1) * sizeof(off_t));

    /*重定向父子结点，写入.index*/
    for (i = 0; i < right->children; i++) {
        if (i != pivot && i != pivot + 1) {
            sub_node_flush_str(tree, right, sub_arr(right)[i]);
        }
    }

    /*返回上一层键值*/
    //todo
    return (char *) split_key;
}

/*
父节点未满时，非叶子节点的简单插入
struct bplus_tree *tree----------------B+树信息结构体
struct bplus_node *node----------------父节点
struct bplus_node *l_ch----------------左孩子
struct bplus_node *r_ch----------------右孩子
key_t_arr key------------------------------键值
int insert-----------------------------插入位置
*/
static void non_leaf_simple_insert_str(struct bplus_tree *tree, struct bplus_node *node, struct bplus_node *l_ch,
                                       struct bplus_node *r_ch, key_t_arr key, int insert) {
    /*将insert处原来的值后移*/
    memmove(key_arr(node)[insert + 1], key_arr(node)[insert], (node->children - 1 - insert) * sizeof(key_t_arr));
    memmove(&sub_arr(node)[insert + 2], &sub_arr(node)[insert + 1], (node->children - 1 - insert) * sizeof(off_t));

    /*在insert处插入键值，并更新索引*/
    //key_arr(node)[insert] = key;
    //todo
    memcpy(key_arr(node)[insert], key, sizeof(key_t_arr));
    sub_node_update_str(tree, node, insert, l_ch);
    sub_node_update_str(tree, node, insert + 1, r_ch);
    node->children++;
}

/*
非叶子节点插入，定义
即生成新的父节点
struct bplus_tree *tree-------------B+树信息结构体
struct bplus_node *node-------------要接入的新的B+树兄弟叶子节点
struct bplus_node *l_ch-------------B+树左孩子节点
struct bplus_node *r_ch-------------B+树右孩子节点
key_t_arr key
*/
static int
non_leaf_insert_str(struct bplus_tree *tree, struct bplus_node *node, struct bplus_node *l_ch, struct bplus_node *r_ch,
                    key_t_arr key) {
    /*键值二分查找*/

    int insert = key_binary_search_str(node, key);
    assert(insert < 0);
    insert = -insert - 1;

    /*父节点满，进行分裂*/
    if (node->children == _max_order_arr) {
        //todo
        //key_t_arr split_key;
        char *split_key = NULL;
        /*分裂边界spilit=(len+1)/2*/
        int split = (node->children + 1) / 2;
        /*生成一个新的分裂的非叶子节点*/
        struct bplus_node *sibling = non_leaf_new_str_str(tree);
        if (insert < split) {
            split_key = non_leaf_split_left_str(tree, node, sibling, l_ch, r_ch, key, insert);
        } else if (insert == split) {
            split_key = non_leaf_split_right_str1(tree, node, sibling, l_ch, r_ch, key, insert);
        } else {
            split_key = non_leaf_split_right_str2(tree, node, sibling, l_ch, r_ch, key, insert);
        }

        //todo
        key_t_arr k1 = {0};
        /*再次建立新的父节点*/
        if (insert < split) {
            memcpy(k1, split_key, sizeof(key_t_arr));
            // return parent_node_build_str(tree, sibling, node, split_key);
            return parent_node_build_str(tree, sibling, node, k1);
        } else {
            memcpy(k1, split_key, sizeof(key_t_arr));
            //return parent_node_build_str(tree, node, sibling, split_key);
            return parent_node_build_str(tree, node, sibling, k1);
        }
        /*父节点未满，进行简单的非叶子节点插入，并保存*/
    } else {
        non_leaf_simple_insert_str(tree, node, l_ch, r_ch, key, insert);
        node_flush_str(tree, node);
    }
    return 0;
}

/*
节点分裂插入，插入位置在分裂位置的左边
与leaf_split_right_str类似
struct bplus_tree *tree-----------B+树信息结构体
struct bplus_node *leaf-----------B+树叶子节点
struct bplus_node *left-----------新的叶子节点
key_t_arr key-------------------------键值
long data-------------------------数据
int insert------------------------插入位置
*/
//todo
static char *
leaf_split_left_str(struct bplus_tree *tree, struct bplus_node *leaf, struct bplus_node *left, key_t_arr key, long data,
                    int insert) {
    /*分裂边界split=(len+1)/2*/
    int split = (leaf->children + 1) / 2;

    /*节点分裂，设置左右兄弟叶子节点的指向*/
    left_node_add_str(tree, leaf, left);

    /*重新设置children的数值*/
    int pivot = insert;
    left->children = split;
    leaf->children = _max_entries_arr - split + 1;

    /*
    将原叶子节点key[0]-key[insert]的数值复制到左边分裂出的新的叶子节点
    将原叶子节点data[0]-data[insert]的数值复制到左边分裂出的新的叶子节点
    */
    memmove(key_arr(left)[0], key_arr(leaf)[0], pivot * sizeof(key_t_arr));
    memmove(&data_arr(left)[0], &data_arr(leaf)[0], pivot * sizeof(long));

    /*在insert处插入新的key和data*/
    //todo
    //key_arr(left)[pivot] = key;
    memcpy(key_arr(left)[pivot], key, sizeof(key_t_arr));
    data_arr(left)[pivot] = data;

    /*从原叶子节点将insert到split的值放到新的叶子节点insert+1处*/
    memmove(key_arr(left)[pivot + 1], key_arr(leaf)[pivot], (split - pivot - 1) * sizeof(key_t_arr));
    memmove(&data_arr(left)[pivot + 1], &data_arr(leaf)[pivot], (split - pivot - 1) * sizeof(long));

    /*将原叶子节点insert+1~end的key和data复制到原叶子节点key[0]*/
    memmove(key_arr(leaf)[0], key_arr(leaf)[split - 1], leaf->children * sizeof(key_t_arr));
    memmove(&data_arr(leaf)[0], &data_arr(leaf)[split - 1], leaf->children * sizeof(long));

    /*返回后继节点的key，即原叶子节点现在的key[0]*/

    //此处其实是返回key的值即可，目前是返回的节点内存地址，有存在被修改的风险。
    static key_t_arr ret = {0};
    memcpy(ret, key_arr(leaf)[0], sizeof(key_t_arr));
    return (char *) ret;
}

/*
节点分裂插入，插入位置在分裂位置的右边
与leaf_split_left_str类似
struct bplus_tree *tree-----------B+树信息结构体
struct bplus_node *leaf-----------B+树叶子节点
struct bplus_node *right----------新的叶子节点
key_t_arr key-------------------------键值
long data-------------------------数据
int insert------------------------插入位置
*/
//todo
static char *
leaf_split_right_str(struct bplus_tree *tree, struct bplus_node *leaf, struct bplus_node *right, key_t_arr key,
                     long data,
                     int insert) {
    /*分裂边界split=(len+1)/2*/
    int split = (leaf->children + 1) / 2;

    /*节点分裂，设置左右兄弟叶子节点的指向*/
    right_node_add_str(tree, leaf, right);

    /*重新设置children的数值*/
    int pivot = insert - split;
    leaf->children = split;
    right->children = _max_entries_arr - split + 1;

    /*将原叶子节点spilt~insert的key和data复制到右边分裂出的新的叶子节点*/
    memmove(key_arr(right)[0], key_arr(leaf)[split], pivot * sizeof(key_t_arr));
    memmove(&data_arr(right)[0], &data_arr(leaf)[split], pivot * sizeof(long));

    /*在insert处插入新的key和data*/
    //key_arr(right)[pivot] = key;
    //todo
    memcpy(key_arr(right)[pivot], key, sizeof(key_t_arr));
    data_arr(right)[pivot] = data;

    /*移动剩余的数据*/
    memmove(key_arr(right)[pivot + 1], key_arr(leaf)[insert], (_max_entries_arr - insert) * sizeof(key_t_arr));
    memmove(&data_arr(right)[pivot + 1], &data_arr(leaf)[insert], (_max_entries_arr - insert) * sizeof(long));

    /*返回后继节点的key，即分裂的叶子节点的key[0]*/
    //此处其实是返回key的值即可，目前是返回的节点内存地址，有存在被修改的风险。
    static key_t_arr ret = {0};
    memcpy(ret, key_arr(right)[0], sizeof(key_t_arr));
    return (char *) ret;
}

/*
叶子节点在未满时的简单插入
struct bplus_tree *tree--------------------B+树信息结构体
struct bplus_node *leaf--------------------B+树节点结构体，要插入的位置的上一个节点
key_t_arr key----------------------------------键值
long data----------------------------------数据
int intsert--------------------------------要插入的节点位序
两个memmove是将在insert之前的数据往后存放，使得数据能够插入
*/
static void
leaf_simple_insert_str(struct bplus_tree *tree, struct bplus_node *leaf, key_t_arr key, long data, int insert) {
    memmove(key_arr(leaf)[insert + 1], key_arr(leaf)[insert], (leaf->children - insert) * sizeof(key_t_arr));
    memmove(&data_arr(leaf)[insert + 1], &data_arr(leaf)[insert], (leaf->children - insert) * sizeof(long));
    //key_arr(leaf)[insert] = key;
    //todo
    memcpy(key_arr(leaf)[insert], key, sizeof(key_t_arr));
    data_arr(leaf)[insert] = data;
    leaf->children++;
}

/*
插入叶子节点
struct bplus_tree *tree--------------------B+树信息结构体
struct bplus_node *leaf--------------------B+树节点结构体，要插入的位置的上一个节点
key_t_arr key----------------------------------键值
long data----------------------------------数据
*/
static int leaf_insert_str(struct bplus_tree *tree, struct bplus_node *leaf, key_t_arr key, long data) {
    /*键值二分查找*/
    int insert = key_binary_search_str(leaf, key);
    /*已存在键值*/
    if (insert >= 0) {
        return -1;
    }
    insert = -insert - 1;

    /*从空闲节点缓存中获取*/
    int i = ((char *) leaf - tree->caches) / _block_size_arr;
    tree->used[i] = 1;

    /*叶子节点满*/
    if (leaf->children == _max_entries_arr) {
        //key_t_arr split_key;
        //todo
        char *split_key = NULL;
        /*节点分裂边界split=(len+1)/2*/
        int split = (_max_entries_arr + 1) / 2;
        struct bplus_node *sibling = leaf_new_str(tree);

        /*
        由插入位置决定的兄弟叶复制
        insert < split：插入位置在分裂位置的左边
        insert >= split：插入位置在分裂位置的右边
        返回后继节点，以放入父节点作为键值
        */
        if (insert < split) {
            split_key = leaf_split_left_str(tree, leaf, sibling, key, data, insert);
        } else {
            split_key = leaf_split_right_str(tree, leaf, sibling, key, data, insert);
        }

        /*建立新的父节点*/
        if (insert < split) {
            return parent_node_build_str(tree, sibling, leaf, split_key);
        } else {
            return parent_node_build_str(tree, leaf, sibling, split_key);
        }
        /*叶子节点未满*/
    } else {
        leaf_simple_insert_str(tree, leaf, key, data, insert);
        node_flush_str(tree, leaf);
    }

    return 0;
}

/*
插入节点
*/
static int bplus_tree_insert_str(struct bplus_tree *tree, key_t_arr key, long data) {
    struct bplus_node *node = node_seek_str(tree, tree->root);
    while (node != NULL) {
        /*到达叶子节点*/
        if (is_leaf_str(node)) {
            return leaf_insert_str(tree, node, key, data);
            /*还未到达叶子节点，继续循环递归查找*/
        } else {
            int i = key_binary_search_str(node, key);
            if (i >= 0) {
                node = node_seek_str(tree, sub_arr(node)[i + 1]);
            } else {
                i = -i - 1;
                node = node_seek_str(tree, sub_arr(node)[i]);
            }
        }
    }

    /*
    创建新的叶子节点
    在B+树后面跟随赋值key和data
    添加key：key_arr(root)[0] = key;
    添加data：data_arr(root)[0] = data;
    插入树：tree->root = new_node_append_str(tree, root);
    刷新缓冲区：node_flush_str(tree, root);
    */
    struct bplus_node *root = leaf_new_str(tree);
//    key_arr(root)[0] = key;
    //todo
    memcpy(key_arr(root)[0], key, sizeof(key_t_arr));
    data_arr(root)[0] = data;
    root->children = 1;
    tree->root = new_node_append_str(tree, root);
    tree->level = 1;
    node_flush_str(tree, root);
    return 0;
}

/*
struct bplus_node *l_sib------------------左兄弟
struct bplus_node *r_sib------------------右兄弟
struct bplus_node *parent-----------------父节点
int i-------------------------------------键值在父节点中的位置
*/
static inline int sibling_select_str(struct bplus_node *l_sib, struct bplus_node *r_sib, struct bplus_node *parent, int i) {
    if (i == -1) {
        /*没有左兄弟，选择右兄弟合并*/
        return RIGHT_SIBLING;
    } else if (i == parent->children - 2) {
        /*没有右兄弟，选择左兄弟*/
        return LEFT_SIBLING;
    } else {
        /*有左右兄弟，选择孩子更多的节点*/
        return l_sib->children >= r_sib->children ? LEFT_SIBLING : RIGHT_SIBLING;
    }
}

/*
非叶子节点从左兄弟拿一个值
*/
static void non_leaf_shift_from_left_str(struct bplus_tree *tree, struct bplus_node *node, struct bplus_node *left,
                                     struct bplus_node *parent, int parent_key_index, int remove) {
    memmove(key_arr(node)[1], key_arr(node)[0], remove * sizeof(key_t_arr));
    memmove(&sub_arr(node)[1], &sub_arr(node)[0], (remove + 1) * sizeof(off_t));

    //key_arr(node)[0] = key_arr(parent)[parent_key_index];
    //todo
    memcpy(key_arr(node)[0], key_arr(parent)[parent_key_index], sizeof(key_t_arr));
    //key_arr(parent)[parent_key_index] = key_arr(left)[left->children - 2];
    //todo
    memcpy(key_arr(parent)[parent_key_index], key_arr(left)[left->children - 2], sizeof(key_t_arr));

    sub_arr(node)[0] = sub_arr(left)[left->children - 1];
    sub_node_flush_str(tree, node, sub_arr(node)[0]);

    left->children--;
}

/*
非叶子节点合并到左兄弟
*/
static void non_leaf_merge_into_left_str(struct bplus_tree *tree, struct bplus_node *node, struct bplus_node *left,
                                     struct bplus_node *parent, int parent_key_index, int remove) {
    /*键值下移*/
    //key_arr(left)[left->children - 1] = key_arr(parent)[parent_key_index];
    //todo
    memcpy(key_arr(left)[left->children - 1], key_arr(parent)[parent_key_index], sizeof(key_t_arr));

    memmove(key_arr(left)[left->children], key_arr(node)[0], remove * sizeof(key_t_arr));
    memmove(&sub_arr(left)[left->children], &sub_arr(node)[0], (remove + 1) * sizeof(off_t));

    memmove(key_arr(left)[left->children + remove], key_arr(node)[remove + 1],
            (node->children - remove - 2) * sizeof(key_t_arr));
    memmove(&sub_arr(left)[left->children + remove + 1], &sub_arr(node)[remove + 2],
            (node->children - remove - 2) * sizeof(off_t));

    int i, j;
    for (i = left->children, j = 0; j < node->children - 1; i++, j++) {
        sub_node_flush_str(tree, left, sub_arr(left)[i]);
    }

    left->children += node->children - 1;
}

/*
非叶子节点从右兄弟拿一个值
*/
static void non_leaf_shift_from_right_str(struct bplus_tree *tree, struct bplus_node *node, struct bplus_node *right,
                                      struct bplus_node *parent, int parent_key_index) {
    //key_arr(node)[node->children - 1] = key_arr(parent)[parent_key_index];
    //todo
    memcpy(key_arr(node)[node->children - 1], key_arr(parent)[parent_key_index], sizeof(key_t_arr));
    //key_arr(parent)[parent_key_index] = key_arr(right)[0];
    //todo
    memcpy(key_arr(parent)[parent_key_index], key_arr(right)[0], sizeof(key_t_arr));

    sub_arr(node)[node->children] = sub_arr(right)[0];
    sub_node_flush_str(tree, node, sub_arr(node)[node->children]);
    node->children++;

    memmove(key_arr(right)[0], key_arr(right)[1], (right->children - 2) * sizeof(key_t_arr));
    memmove(&sub_arr(right)[0], &sub_arr(right)[1], (right->children - 1) * sizeof(off_t));

    right->children--;
}

/*
非叶子节点合并到右兄弟
*/
static void non_leaf_merge_from_right_str(struct bplus_tree *tree, struct bplus_node *node, struct bplus_node *right,
                                      struct bplus_node *parent, int parent_key_index) {
    //key_arr(node)[node->children - 1] = key_arr(parent)[parent_key_index];
    //todo
    memcpy(key_arr(node)[node->children - 1], key_arr(parent)[parent_key_index], sizeof(key_t_arr));
    node->children++;

    memmove(key_arr(node)[node->children - 1], key_arr(right)[0], (right->children - 1) * sizeof(key_t_arr));
    memmove(&sub_arr(node)[node->children - 1], &sub_arr(right)[0], right->children * sizeof(off_t));

    int i, j;
    for (i = node->children - 1, j = 0; j < right->children; i++, j++) {
        sub_node_flush_str(tree, node, sub_arr(node)[i]);
    }

    node->children += right->children - 1;
}

/*
非叶子节点的简单删除
*/
static inline void non_leaf_simple_remove_str(struct bplus_tree *tree, struct bplus_node *node, int remove) {
    assert(node->children >= 2);
    memmove(key_arr(node)[remove], key_arr(node)[remove + 1], (node->children - remove - 2) * sizeof(key_t_arr));
    memmove(&sub_arr(node)[remove + 1], &sub_arr(node)[remove + 2], (node->children - remove - 2) * sizeof(off_t));
    node->children--;
}

/*
非叶子节点的删除操作
叶子节点删除操作后，更新非叶子节点，非叶子节点的键值也可能被删除，非叶子节点也可能合并
struct bplus_tree *tree---------------------B+树信息结构体
struct bplus_node *node---------------------要执行删除操作的节点
int remove----------------------------------要删除的键值位置
*/
static void non_leaf_remove_str(struct bplus_tree *tree, struct bplus_node *node, int remove) {
    /*不存在父节点，要执行删除操作的节点是根节点*/
    if (node->parent == INVALID_OFFSET) {
        /*只有两个键值*/
        if (node->children == 2) {
            /*用第一个子节点替换旧根节点*/
            struct bplus_node *root = node_fetch_str(tree, sub_arr(node)[0]);
            root->parent = INVALID_OFFSET;
            tree->root = root->self;
            tree->level--;
            node_delete_str(tree, node, NULL, NULL);
            node_flush_str(tree, root);
            /*键值大于2，将remove后的数据前移*/
        } else {
            non_leaf_simple_remove_str(tree, node, remove);
            node_flush_str(tree, node);
        }
        /*存在父节点，且非叶子节点内含数据小于一半，也要进行合并操作*/
    } else if (node->children <= (_max_order_arr + 1) / 2) {
        struct bplus_node *l_sib = node_fetch_str(tree, node->prev);
        struct bplus_node *r_sib = node_fetch_str(tree, node->next);
        struct bplus_node *parent = node_fetch_str(tree, node->parent);

        int i = parent_key_index_str(parent, key_arr(node)[0]);

        /*选择左兄弟合并*/
        if (sibling_select_str(l_sib, r_sib, parent, i) == LEFT_SIBLING) {
            /*左兄弟节点内数据过半，无法合并，就拿一个数据过来*/
            if (l_sib->children > (_max_order_arr + 1) / 2) {
                /*左兄弟数据未过半，两两合并*/
                non_leaf_shift_from_left_str(tree, node, l_sib, parent, i, remove);
                node_flush_str(tree, node);
                node_flush_str(tree, l_sib);
                node_flush_str(tree, r_sib);
                node_flush_str(tree, parent);
                /*左兄弟数据未过半，两两合并*/
            } else {
                non_leaf_merge_into_left_str(tree, node, l_sib, parent, i, remove);
                node_delete_str(tree, node, l_sib, r_sib);
                non_leaf_remove_str(tree, parent, i);
            }
            /*选择右兄弟合并*/
        } else {
            /*在与兄弟节点合并时首先删除，以防溢出*/
            non_leaf_simple_remove_str(tree, node, remove);

            /*右兄弟节点内数据过半，无法合并，就拿一个数据过来*/
            if (r_sib->children > (_max_order_arr + 1) / 2) {
                non_leaf_shift_from_right_str(tree, node, r_sib, parent, i + 1);
                node_flush_str(tree, node);
                node_flush_str(tree, l_sib);
                node_flush_str(tree, r_sib);
                node_flush_str(tree, parent);
                /*右兄弟数据未过半，两两合并*/
            } else {
                non_leaf_merge_from_right_str(tree, node, r_sib, parent, i + 1);
                struct bplus_node *rr_sib = node_fetch_str(tree, r_sib->next);
                node_delete_str(tree, r_sib, node, rr_sib);
                node_flush_str(tree, l_sib);
                non_leaf_remove_str(tree, parent, i + 1);
            }
        }
        /*存在父节点，且非叶子节点内含数据大于一半，不需要进行合并操作*/
    } else {
        non_leaf_simple_remove_str(tree, node, remove);
        node_flush_str(tree, node);
    }
}

/*
从左兄弟拿一个数据，来保持平衡
struct bplus_tree *tree------------------B+树信息结构体
struct bplus_node *leaf------------------要执行删除操作和合并操作的叶子节点
struct bplus_node *left------------------左兄弟
struct bplus_node *parent----------------父节点
int parent_key_index---------------------leaf在父节点的位置
int remove-------------------------------删除的数据在leaf的位置
*/
static void leaf_shift_from_left_str(struct bplus_tree *tree, struct bplus_node *leaf, struct bplus_node *left,
                                 struct bplus_node *parent, int parent_key_index, int remove) {
    /*腾出第一个位置*/
    memmove(key_arr(leaf)[1], key_arr(leaf)[0], remove * sizeof(key_t_arr));
    memmove(&data_arr(leaf)[1], &data_arr(leaf)[0], remove * sizeof(off_t));

    /*从左兄弟拿一个数据*/
    //key_arr(leaf)[0] = key_arr(left)[left->children - 1];
    //todo
    memcpy(key_arr(leaf)[0], key_arr(left)[left->children - 1], sizeof(key_t_arr));
    data_arr(leaf)[0] = data_arr(left)[left->children - 1];
    left->children--;

    /*更新父节点的键值*/
    //key_arr(parent)[parent_key_index] = key_arr(leaf)[0];
    //todo
    memcpy(key_arr(parent)[parent_key_index], key_arr(leaf)[0], sizeof(key_t_arr));
}

/*
左兄弟数据未过半，两两合并
*/
static void
leaf_merge_into_left_str(struct bplus_tree *tree, struct bplus_node *leaf, struct bplus_node *left, int parent_key_index,
                     int remove) {
    /*将key和data从leaf复制到left，不包括被删除的数据*/
    memmove(key_arr(left)[left->children], key_arr(leaf)[0], remove * sizeof(key_t_arr));
    memmove(&data_arr(left)[left->children], &data_arr(leaf)[0], remove * sizeof(off_t));
    memmove(key_arr(left)[left->children + remove], key_arr(leaf)[remove + 1],
            (leaf->children - remove - 1) * sizeof(key_t_arr));
    memmove(&data_arr(left)[left->children + remove], &data_arr(leaf)[remove + 1],
            (leaf->children - remove - 1) * sizeof(off_t));
    left->children += leaf->children - 1;
}

/*
从左兄弟拿一个数据，来保持平衡
struct bplus_tree *tree------------------B+树信息结构体
struct bplus_node *leaf------------------要执行删除操作和合并操作的叶子节点
struct bplus_node *right-----------------右兄弟
struct bplus_node *parent----------------父节点
int parent_key_index---------------------leaf在父节点的位置
int remove-------------------------------删除的数据在leaf的位置
*/
static void leaf_shift_from_right_str(struct bplus_tree *tree, struct bplus_node *leaf, struct bplus_node *right,
                                  struct bplus_node *parent, int parent_key_index) {
    /*leaf最后一个位置放right第一个数据*/
    //key_arr(leaf)[leaf->children] = key_arr(right)[0];
    //todo
    memcpy(key_arr(leaf)[leaf->children], key_arr(right)[0], sizeof(key_t_arr));
    data_arr(leaf)[leaf->children] = data_arr(right)[0];
    leaf->children++;

    /*right左移*/
    memmove(key_arr(right)[0], key_arr(right)[1], (right->children - 1) * sizeof(key_t_arr));
    memmove(&data_arr(right)[0], &data_arr(right)[1], (right->children - 1) * sizeof(off_t));
    right->children--;

    /*更新父节点的键值*/
    //key_arr(parent)[parent_key_index] = key_arr(right)[0];
    //todo
    memcpy(key_arr(parent)[parent_key_index], key_arr(right)[0], sizeof(key_t_arr));
}

/*
左兄弟数据未过半，两两合并
*/
static inline void leaf_merge_from_right_str(struct bplus_tree *tree, struct bplus_node *leaf, struct bplus_node *right) {
    memmove(key_arr(leaf)[leaf->children], key_arr(right)[0], right->children * sizeof(key_t_arr));
    memmove(&data_arr(leaf)[leaf->children], &data_arr(right)[0], right->children * sizeof(off_t));
    leaf->children += right->children;
}

/*
叶子节点的简单删除操作
*/
static inline void leaf_simple_remove_str(struct bplus_tree *tree, struct bplus_node *leaf, int remove) {
    /*key和data左移覆盖被删除的key和data*/
    memmove(key_arr(leaf)[remove], key_arr(leaf)[remove + 1], (leaf->children - remove - 1) * sizeof(key_t_arr));
    memmove(&data_arr(leaf)[remove], &data_arr(leaf)[remove + 1], (leaf->children - remove - 1) * sizeof(off_t));
    leaf->children--;
}

/*
叶子节点的删除操作
struct bplus_tree *tree-----------------B+树信息结构体
struct bplus_node *leaf-----------------要执行删除操作的叶子节点
key_t_arr key-------------------------------要删除的键值
*/
static int leaf_remove_str(struct bplus_tree *tree, struct bplus_node *leaf, key_t_arr key) {
    int remove = key_binary_search_str(leaf, key);
    /*要删除的键值不存在*/
    if (remove < 0) {
        return -1;
    }

    /*节点所在的缓存位置*/
    int i = ((char *) leaf - tree->caches) / _block_size_arr;
    tree->used[i] = 1;

    /*父节点非法，即不存在父节点，要进行删除操作的叶子节点是根节点*/
    if (leaf->parent == INVALID_OFFSET) {
        /*节点内只有1个数据*/
        if (leaf->children == 1) {
            /* delete the only last node */
            assert(key == key_arr(leaf)[0]);
            tree->root = INVALID_OFFSET;
            tree->level = 0;
            /*删除节点*/
            node_delete_str(tree, leaf, NULL, NULL);
            /*节点内有多个数据*/
        } else {
            leaf_simple_remove_str(tree, leaf, remove);
            node_flush_str(tree, leaf);
        }
        /*有父节点，删除后节点内数据过少，要进行合并操作*/
    } else if (leaf->children <= (_max_entries_arr + 1) / 2) {
        struct bplus_node *l_sib = node_fetch_str(tree, leaf->prev);
        struct bplus_node *r_sib = node_fetch_str(tree, leaf->next);
        struct bplus_node *parent = node_fetch_str(tree, leaf->parent);

        i = parent_key_index_str(parent, key_arr(leaf)[0]);

        /*选择左兄弟合并*/
        if (sibling_select_str(l_sib, r_sib, parent, i) == LEFT_SIBLING) {
            /*左兄弟节点内数据过半，无法合并，就拿一个数据过来*/
            if (l_sib->children > (_max_entries_arr + 1) / 2) {
                leaf_shift_from_left_str(tree, leaf, l_sib, parent, i, remove);
                node_flush_str(tree, leaf);
                node_flush_str(tree, l_sib);
                node_flush_str(tree, r_sib);
                node_flush_str(tree, parent);
                /*左兄弟数据未过半，合并*/
            } else {
                leaf_merge_into_left_str(tree, leaf, l_sib, i, remove);
                /*删除无意义的leaf*/
                node_delete_str(tree, leaf, l_sib, r_sib);
                /*更新父节点*/
                non_leaf_remove_str(tree, parent, i);
            }
            /*选择右兄弟合并*/
        } else {
            leaf_simple_remove_str(tree, leaf, remove);

            /*右兄弟节点内数据过半，无法合并，就拿一个数据过来*/
            if (r_sib->children > (_max_entries_arr + 1) / 2) {
                leaf_shift_from_right_str(tree, leaf, r_sib, parent, i + 1);
                /* flush leaves */
                node_flush_str(tree, leaf);
                node_flush_str(tree, l_sib);
                node_flush_str(tree, r_sib);
                node_flush_str(tree, parent);
                /*右兄弟数据未过半，合并*/
            } else {
                leaf_merge_from_right_str(tree, leaf, r_sib);
                /*删除无意义的leaf*/
                struct bplus_node *rr_sib = node_fetch_str(tree, r_sib->next);
                node_delete_str(tree, r_sib, leaf, rr_sib);
                node_flush_str(tree, l_sib);
                /*更新父节点*/
                non_leaf_remove_str(tree, parent, i + 1);
            }
        }
        /*有父节点，但删除后，节点内数据大于一半，不进行合并*/
    } else {
        leaf_simple_remove_str(tree, leaf, remove);
        node_flush_str(tree, leaf);
    }

    return 0;
}

/*
删除节点
*/
static int bplus_tree_delete_str(struct bplus_tree *tree, key_t_arr key) {
    struct bplus_node *node = node_seek_str(tree, tree->root);
    while (node != NULL) {
        /*叶子节点，直接进行删除操作*/
        if (is_leaf_str(node)) {
            return leaf_remove_str(tree, node, key);
            /*非叶子节点，继续循环递归查找*/
        } else {
            int i = key_binary_search_str(node, key);
            if (i >= 0) {
                node = node_seek_str(tree, sub_arr(node)[i + 1]);
            } else {
                i = -i - 1;
                node = node_seek_str(tree, sub_arr(node)[i]);
            }
        }
    }
    return -1;
}

/*
查找结点的入口
*/
long bplus_tree_get_str(struct bplus_tree *tree, key_t_arr key) {
    return bplus_tree_search_str(tree, key);
}

/*
处理节点入口
插入节点
删除节点
*/
int bplus_tree_put_str(struct bplus_tree *tree, key_t_arr key, long data) {
    if (data) {
        return bplus_tree_insert_str(tree, key, data);
    } else {
        return bplus_tree_delete_str(tree, key);
    }
}

//int bplus_tree_put(struct bplus_tree *tree, key_t key, long data) {
//    if (data) {
//        return bplus_tree_insert_str(tree, key, data);
//    } else {
//        return bplus_tree_delete_str(tree, key);
//    }
//}

/*
获取范围
*/
long bplus_tree_get_range_str(struct bplus_tree *tree, key_t_arr key1, key_t_arr key2) {
    long start = -1;
//    key_t_arr min = key1 <= key2 ? key1 : key2;
//    key_t_arr max = min == key1 ? key2 : key1;
//todo
    key_t_arr min = {0};
    key_t_arr max = {0};
    if (strcmp(key1, key2) <= 0) {
        memcpy(min, key1, sizeof(key_t_arr));
        memcpy(max, key2, sizeof(key_t_arr));
    } else {
        memcpy(min, key2, sizeof(key_t_arr));
        memcpy(max, key1, sizeof(key_t_arr));
    }

    struct bplus_node *node = node_seek_str(tree, tree->root);
    while (node != NULL) {
        int i = key_binary_search_str(node, min);
        if (is_leaf_str(node)) {
            if (i < 0) {
                i = -i - 1;
                if (i >= node->children) {
                    node = node_seek_str(tree, node->next);
                }
            }
            while (node != NULL && key_arr(node)[i] <= max) {
                start = data_arr(node)[i];
                if (++i >= node->children) {
                    node = node_seek_str(tree, node->next);
                    i = 0;
                }
            }
            break;
        } else {
            if (i >= 0) {
                node = node_seek_str(tree, sub_arr(node)[i + 1]);
            } else {
                i = -i - 1;
                node = node_seek_str(tree, sub_arr(node)[i]);
            }
        }
    }

    return start;
}


/*
B+树初始化
char *filename----------文件名
int block_size----------文件大小
返回--------------------B+树头节点结构体指针
*/
struct bplus_tree *bplus_tree_init_str(char *filename, int block_size) {
    int i;
    struct bplus_node node;

    /*文件名过长*/
    if (strlen(filename) >= 1024) {
        fprintf(stderr, "Index file name too long!\n");
        return NULL;
    }

    /*文件大小不是2的平方*/
    if ((block_size & (block_size - 1)) != 0) {
        fprintf(stderr, "Block size must be pow of 2!\n");
        return NULL;
    }

    /*文件容量太小*/
    if (block_size < (int) sizeof(node)) {
        fprintf(stderr, "block size is too small for one node!\n");
        return NULL;
    }

    _block_size_arr = block_size;
    _max_order_arr = (block_size - sizeof(node)) / (sizeof(key_t_arr) + sizeof(off_t));
    _max_entries_arr = (block_size - sizeof(node)) / (sizeof(key_t_arr) + sizeof(long));

    /*文件容量太小*/
    if (_max_order_arr <= 2) {
        fprintf(stderr, "block size is too small for one node!\n");
        return NULL;
    }

    /*为B+树信息节点分配内存*/
    struct bplus_tree *tree = calloc(1, sizeof(*tree));
    assert(tree != NULL);
    list_init(&tree->free_blocks);
    strcpy(tree->filename, filename);

    /*
    加载boot文件，可读可写
    tree->filename变为.boot
    首次运行不存在
    得到信息节点的信息，每16位记录一个信息
    root----------------B+树根节点在.index中的偏移量
    block_size----------分配的空间大小
    file_size-----------实际空间大小
    */
    int fd = open(strcat(tree->filename, ".boot"), O_RDWR, 0644);
    if (fd >= 0) {
        tree->root = offset_load(fd);
        _block_size_arr = offset_load(fd);
        tree->file_size = offset_load(fd);

        /*加载freeblocks空闲数据块*/
        while ((i = offset_load(fd)) != INVALID_OFFSET) {
            struct free_block *block = malloc(sizeof(*block));
            assert(block != NULL);
            block->offset = i;
            list_add(&block->link, &tree->free_blocks);
        }
        close(fd);
    } else {
        tree->root = INVALID_OFFSET;
        _block_size_arr = block_size;
        tree->file_size = 0;
    }

    /*设置节点内关键字和数据最大个数*/
    _max_order_arr = (_block_size_arr - sizeof(node)) / (sizeof(key_t_arr) + sizeof(off_t));
    _max_entries_arr = (_block_size_arr - sizeof(node)) / (sizeof(key_t_arr) + sizeof(long));
    printf("config node order:%d and leaf entries:%d and _block_size_arr:%d raw node size is %lu node size is %lu\n",
           _max_order_arr, _max_entries_arr, _block_size_arr, sizeof(node), (sizeof(key_t_arr) + sizeof(long)));

    /*申请和初始化节点缓存*/
    tree->caches = malloc(_block_size_arr * MIN_CACHE_NUM);

    /*打开index文件，首次运行不存在，创建index文件=*/
    tree->fd = bplus_open(filename);
    assert(tree->fd >= 0);
    return tree;
}

/*
B+树的关闭操作
打开.boot文件
*/
//todo
void bplus_tree_deinit_str(struct bplus_tree *tree) {
    /*向.boot写入B+树的3个配置数据*/
    int fd = open(tree->filename, O_CREAT | O_RDWR, 0644);
    assert(fd >= 0);
    assert(offset_store(fd, tree->root) == ADDR_STR_WIDTH);
    assert(offset_store(fd, _block_size_arr) == ADDR_STR_WIDTH);
    assert(offset_store(fd, tree->file_size) == ADDR_STR_WIDTH);

    /*将空闲块存储在文件中以备将来重用*/
    struct list_head *pos, *n;
    list_for_each_safe(pos, n, &tree->free_blocks) {
        list_del(pos);
        struct free_block *block = list_entry(pos, struct free_block, link);
        assert(offset_store(fd, block->offset) == ADDR_STR_WIDTH);
        free(block);
    }

    bplus_close(tree->fd);
    free(tree->caches);
    free(tree);
}

