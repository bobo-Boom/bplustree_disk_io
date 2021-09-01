#include"bplustree.h"




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
键值二分查找
*/
static int key_binary_search_str(struct bplus_node *node, key_t_arr target) {
    key_t_arr *arr = key_arr(node);
    /*叶子节点：len；非叶子节点：len-1;非叶子节点的key少一个，用于放ptr*/
    int len = is_leaf(node) ? node->children : node->children - 1;
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
根据偏移量从.index获取节点的全部信息，加载到缓冲区
偏移量非法则返回NULL
*/
static struct bplus_node *node_fetch_str(struct bplus_tree *tree, off_t offset) {
    if (offset == INVALID_OFFSET) {
        return NULL;
    }

    struct bplus_node *node = cache_refer_str(tree);
    memcpy(node, tree->fd + offset, _block_size_arr);
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
            memcpy(buf, tree->fd + offset, _block_size_arr);
            return (struct bplus_node *) buf;
        }
    }
    return NULL;
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
        if (is_leaf(node)) {
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
查找结点的入口
*/
long bplus_tree_get_str(struct bplus_tree *tree, key_t_arr key) {
    return bplus_tree_search_str(tree, key);
}


/*
获取范围
*/
long bplus_tree_get_range_str(struct bplus_tree *tree, key_t_arr key1, key_t_arr key2) {
    long start = -1;

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
        if (is_leaf(node)) {
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
B+ load
*/
struct bplus_tree *bplus_tree_load_str(char *tree_addr, char *tree_boot_addr, int block_size) {
    int i;
    off_t boot_file_off_t = 0;
    off_t boot_file_size = 0;
    struct bplus_node node;
    /*节点大小不是2的平方*/
    if ((block_size & (block_size - 1)) != 0) {
        fprintf(stderr, "Block size must be pow of 2!\n");
        return NULL;
    }
    /*节点size太小*/
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
    strcpy(tree->filename, tree_addr);

    /*
    加载boot文件，可读可写
    */
    if (tree_boot_addr == NULL) {
        return NULL;
    }
    //boot file size
    boot_file_size = get_boot_size(tree_boot_addr);
    //tree root
    tree->root = get_tree_root(tree_boot_addr);
    //tree id
    off_t tree_id = get_tree_id(tree_boot_addr);
    tree->tree_id = tree_id;
    //key type
    off_t tree_type = get_tree_type(tree_boot_addr);
    tree_type = tree_type;
    //block size
    _block_size_arr = get_tree_block_size(tree_boot_addr);
    //tree size
    tree->file_size = get_tree_size(tree_boot_addr);

    tree->fd = tree_addr;


    /*加载freeblocks空闲数据块*/
    boot_file_off_t = 6 * ADDR_STR_WIDTH;
    while (boot_file_off_t < boot_file_size) {
        i = offset_load(tree_boot_addr, boot_file_off_t);
        struct free_block *block = malloc(sizeof(*block));
        assert(block != NULL);
        block->offset = i;
        list_add(&block->link, &tree->free_blocks);
        boot_file_off_t += ADDR_STR_WIDTH;
    }
    /*设置节点内关键字和数据最大个数,如果是256都是18*/
    _max_order_arr = (_block_size_arr - sizeof(node)) / (sizeof(key_t_arr) + sizeof(off_t));
    _max_entries_arr = (_block_size_arr - sizeof(node)) / (sizeof(key_t_arr) + sizeof(long));
    // printf todo
    printf("config node order:%d and leaf entries:%d and _block_size:%d ,sizeof key_t:%lu\n", _max_order_arr,
           _max_entries_arr,
           _block_size_arr, sizeof(key_t_arr));

    /*申请和初始化节点缓存*/
    tree->caches = malloc(_block_size_arr * MIN_CACHE_NUM);

    return tree;
}

/*
B+树的关闭操作
清空内存
*/
void bplus_tree_deinit_str(struct bplus_tree *tree, char *tree_addr, char *tree_boot_addr) {

    int len = get_tree_size(tree_boot_addr);
    bzero(tree->fd, len);
    off_t file_size = get_boot_size(tree_boot_addr);
    bzero(tree_boot_addr, file_size);
    free(tree->caches);
    free(tree);
}

