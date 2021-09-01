#include"bplustree.h"



/*B+树节点node末尾的偏移地址，即key的首地址*/
#define offset_ptr(node) ((char *) (node) + sizeof(*node))

/*返回B+树节点末尾地址，强制转换为key_t*，即key的指针*/
#define key(node) ((key_t *)offset_ptr(node))

/*返回B+树节点和key末尾地址，强制转换为long*，即data指针*/
#define data(node) ((long *)(offset_ptr(node) + _max_entries * sizeof(key_t)))

/*返回最后一个key的指针，用于非叶子节点的指向，即第一个ptr*/
#define sub(node) ((off_t *)(offset_ptr(node) + (_max_order - 1) * sizeof(key_t)))

/*
全局静态变量
_block_size--------------------每个节点的大小(容量要包含1个node和3个及以上的key，data)
_max_entries-------------------叶子节点内包含个数最大值
_max_order---------------------非叶子节点内最大关键字个数
*/
static int _block_size;
static int _max_entries;
static int _max_order;


/*
键值二分查找
*/
static int key_binary_search(struct bplus_node *node, key_t target) {
    //Key的首地址
    key_t *arr = key(node);
    /*叶子节点：len；非叶子节点：len-1;非叶子节点的key少一个？，用于放ptr*/
    /*len用于定位最后一个key的位置*/
    int len = is_leaf(node) ? node->children : node->children - 1;

    int low = -1;
    int high = len;

    while (low + 1 < high) {
        int mid = low + (high - low) / 2;
        if (target > arr[mid]) {
            low = mid;
        } else {
            high = mid;
        }
    }

    if (high >= len || arr[high] != target) {
        return -high - 1;
    } else {
        return high;
    }
}

/*
查找键值在父节点的第几位
*/
static inline int parent_key_index(struct bplus_node *parent, key_t key) {
    int index = key_binary_search(parent, key);
    return index >= 0 ? index : -index - 2;
}

/*
占用缓存区，与cache_defer对应
占用内存，以供使用
缓存不足，assert(0)直接终止程序
*/
static inline struct bplus_node *cache_refer(struct bplus_tree *tree) {
    int i;
    for (i = 0; i < MIN_CACHE_NUM; i++) {
        if (!tree->used[i]) {
            tree->used[i] = 1;
            char *buf = tree->caches + _block_size * i;
            return (struct bplus_node *) buf;
        }
    }
    assert(0);
}

/*
释放缓冲区，与cache_refer对应
将used重置，能够存放接下来的数据
*/
static inline void cache_defer(struct bplus_tree *tree, struct bplus_node *node) {
    char *buf = (char *) node;
    int i = (buf - tree->caches) / _block_size;
    tree->used[i] = 0;
}

/*
 * 将节点从文件中加载出来
根据偏移量从.index获取节点的全部信息，加载到缓冲区
偏移量非法则返回NULL
*/
static struct bplus_node *node_fetch(struct bplus_tree *tree, off_t offset) {
    if (offset == INVALID_OFFSET) {
        return NULL;
    }

    struct bplus_node *node = cache_refer(tree);
    //int len = pread(tree->fd, node, _block_size, offset);
    //assert(len == _block_size);
    //todo
    memcpy(node, tree->fd + offset, _block_size);
    return node;
}

/*
通过节点的偏移量从.index中获取节点的全部信息
*/
static struct bplus_node *node_seek(struct bplus_tree *tree, off_t offset) {
    /*偏移量不合法*/
    if (offset == INVALID_OFFSET) {
        return NULL;
    }

    /*偏移量合法*/
    int i;
    for (i = 0; i < MIN_CACHE_NUM; i++) {
        if (!tree->used[i]) {
            char *buf = tree->caches + _block_size * i;
            // int len =pread(tree->fd, buf, _block_size, offset);
            //assert(len == _block_size);
            //todo
            memcpy(buf, tree->fd + offset, _block_size);
            return (struct bplus_node *) buf;
        }
    }
    return NULL;
}

/*
B+树查找
*/
static long bplus_tree_search(struct bplus_tree *tree, key_t key) {
    int ret = -1;
    /*返回根节点的结构体*/
    struct bplus_node *node = node_seek(tree, tree->root);
    while (node != NULL) {
        int i = key_binary_search(node, key);
        /*到达叶子节点*/
        if (is_leaf(node)) {
            ret = i >= 0 ? data(node)[i] : -1;
            break;
            /*未到达叶子节点，循环递归*/
        } else {
            if (i >= 0) {
                node = node_seek(tree, sub(node)[i + 1]);
            } else {
                i = -i - 1;
                node = node_seek(tree, sub(node)[i]);
            }
        }
    }

    return ret;
}

/*
查找结点的入口
*/
long bplus_tree_get(struct bplus_tree *tree, key_t key) {
    return bplus_tree_search(tree, key);
}


int get_range_amount(struct bplus_tree *tree, key_t key1, key_t key2) {
    long start = -1;
    key_t min = key1 <= key2 ? key1 : key2;
    key_t max = min == key1 ? key2 : key1;


    int count = 0;

    struct bplus_node *node = node_seek(tree, tree->root);
    while (node != NULL) {
        int i = key_binary_search(node, min);
        if (is_leaf(node)) {
            if (i < 0) {
                i = -i - 1;
                if (i >= node->children) {
                    node = node_seek(tree, node->next);
                }
            }
            while (node != NULL && key(node)[i] < max) {
                count++;
                if (++i >= node->children) {
                    node = node_seek(tree, node->next);
                    i = 0;
                }
            }
            break;
        } else {
            if (i >= 0) {
                node = node_seek(tree, sub(node)[i + 1]);
            } else {
                i = -i - 1;
                node = node_seek(tree, sub(node)[i]);
            }
        }
    }
    return count;
}

long *bplus_tree_get_range(struct bplus_tree *tree, key_t key1, key_t key2, int *amount) {
    long start = -1;
    key_t min = key1 <= key2 ? key1 : key2;
    key_t max = min == key1 ? key2 : key1;
    int count = 0;

    count = get_range_amount(tree, key1, key2);
    *amount = count;
    if (count == 0) {
        return NULL;
    }

    long *results = (long *) malloc(count * sizeof(long));
    count = 0;
    struct bplus_node *node = node_seek(tree, tree->root);
    while (node != NULL) {
        int i = key_binary_search(node, min);
        if (is_leaf(node)) {
            if (i < 0) {
                i = -i - 1;
                if (i >= node->children) {
                    node = node_seek(tree, node->next);
                }
            }
            while (node != NULL && key(node)[i] < max) {
                start = data(node)[i];
                results[count] = start;
                count++;
                if (++i >= node->children) {
                    node = node_seek(tree, node->next);
                    i = 0;
                }
            }
            break;
        } else {
            if (i >= 0) {
                node = node_seek(tree, sub(node)[i + 1]);
            } else {
                i = -i - 1;
                node = node_seek(tree, sub(node)[i]);
            }
        }
    }
    return results;
}

int get_greater_amount(struct bplus_tree *tree, key_t key) {
    long start = -1;
    key_t min = key;
    int count = 0;

    struct bplus_node *node = node_seek(tree, tree->root);
    while (node != NULL) {
        int i = key_binary_search(node, min);
        if (is_leaf(node)) {
            if (i < 0) {
                i = -i - 1;
                if (i >= node->children) {
                    node = node_seek(tree, node->next);
                }
            }
            //计数
            while (node != NULL) {
                count++;
                if (++i >= node->children) {
                    node = node_seek(tree, node->next);
                    i = 0;
                }
            }
            break;
        } else {
            if (i >= 0) {
                node = node_seek(tree, sub(node)[i + 1]);
            } else {
                i = -i - 1;
                node = node_seek(tree, sub(node)[i]);
            }
        }
    }
    return count;
}


long *bplus_tree_get_more_than(struct bplus_tree *tree, key_t key, int *amount) {
    long start = -1;
    key_t min = key;
    long *results = NULL;
    int count = 0;
    struct bplus_node *temp = NULL;


    count = get_greater_amount(tree, key);
    *amount = count;
    if (count == 0) {
        return NULL;
    }
    results = malloc(count * sizeof(long));
    count = 0;

    struct bplus_node *node = node_seek(tree, tree->root);
    while (node != NULL) {
        int i = key_binary_search(node, min);
        if (is_leaf(node)) {
            if (i < 0) {
                i = -i - 1;
                if (i >= node->children) {
                    node = node_seek(tree, node->next);
                }
            }
            //计数
            while (node != NULL) {
                start = data(node)[i];
                results[count] = start;
                count++;
                if (++i >= node->children) {
                    node = node_seek(tree, node->next);
                    i = 0;
                }
            }

            break;
        } else {
            if (i >= 0) {
                node = node_seek(tree, sub(node)[i + 1]);
            } else {
                i = -i - 1;
                node = node_seek(tree, sub(node)[i]);
            }
        }
    }
    return results;
}

int get_less_amount(struct bplus_tree *tree, key_t key) {
    key_t max = key;
    int count = 0;

    struct bplus_node *node = node_seek(tree, tree->root);
    while (node != NULL) {
        int i = key_binary_search(node, max);
        if (is_leaf(node)) {
            if (i < 0) {
                i = -i - 1;
                if (i >= node->children) {
                    node = node_seek(tree, node->next);
                }
            }
            while (node != NULL) {
                count++;
                if (--i < 0) {
                    node = node_seek(tree, node->prev);
                    if (node == NULL) {
                        break;
                    }
                    i = node->children - 1;
                }
            }
            break;
        } else {
            if (i >= 0) {
                node = node_seek(tree, sub(node)[i + 1]);
            } else {
                i = -i - 1;
                node = node_seek(tree, sub(node)[i]);
            }
        }
    }
    return count;
}

long *bplus_tree_less_than(struct bplus_tree *tree, key_t key, int *amount) {
    long start = -1;
    key_t max = key;
    long *results = NULL;
    int count = 0;

    count = get_less_amount(tree, key);
    *amount = count;
    if (count == 0) {
        return NULL;
    }
    results = malloc(count * sizeof(long));
    count = 0;

    struct bplus_node *node = node_seek(tree, tree->root);
    while (node != NULL) {
        int i = key_binary_search(node, max);
        if (is_leaf(node)) {
            if (i < 0) {
                i = -i - 1;
                if (i >= node->children) {
                    node = node_seek(tree, node->next);
                }
            }
            while (node != NULL) {
                start = data(node)[i];
                results[count] = start;
                count++;
                if (--i < 0) {
                    node = node_seek(tree, node->prev);
                    if (node == NULL) {
                        break;
                    }
                    i = node->children - 1;
                }
            }
            break;
        } else {
            if (i >= 0) {
                node = node_seek(tree, sub(node)[i + 1]);
            } else {
                i = -i - 1;
                node = node_seek(tree, sub(node)[i]);
            }
        }
    }

    return results;
}

/*
B+ load
*/
struct bplus_tree *bplus_tree_load(char *tree_addr, char *tree_boot_addr, int block_size) {
    int i;
    off_t boot_file_off_t = 0;
    off_t boot_file_size = 0;
    struct bplus_node node;

    /*节点大小不是2的平方*/
    if ((block_size & (block_size - 1)) != 0) {
        //printf todo
        fprintf(stderr, "Block size must be pow of 2!\n");
        return NULL;
    }
    /*节点size太小*/
    if (block_size < (int) sizeof(node)) {
        //printf todo
        fprintf(stderr, "block size is too small for one node!\n");
        return NULL;
    }

    _block_size = block_size;
    _max_order = (block_size - sizeof(node)) / (sizeof(key_t) + sizeof(off_t));
    _max_entries = (block_size - sizeof(node)) / (sizeof(key_t) + sizeof(long));

    /*文件容量太小*/
    if (_max_order <= 2) {
        //  printf todo
        fprintf(stderr, "block size is too small for one node!\n");
        return NULL;
    }

    /*为B+树信息节点分配内存*/
    struct bplus_tree *tree = calloc(1, sizeof(*tree));
    assert(tree != NULL);
    list_init(&tree->free_blocks);
   // strcpy(tree->filename, tree_addr);
    printf("11111111\n");
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
    _block_size = get_tree_block_size(tree_boot_addr);
    //tree size
    tree->file_size = get_tree_size(tree_boot_addr);
    printf("file size %lld\n",tree->file_size);
    tree->fd = tree_addr;

    boot_file_off_t = 6 * ADDR_STR_WIDTH;
    while (boot_file_off_t < boot_file_size) {
        printf("========================\n");
        i = offset_load(tree_boot_addr, boot_file_off_t);
        struct free_block *block = malloc(sizeof(*block));
        assert(block != NULL);
        block->offset = i;
        list_add(&block->link, &tree->free_blocks);
        boot_file_off_t += ADDR_STR_WIDTH;
    }
    /*设置节点内关键字和数据最大个数,如果是256都是18*/
    _max_order = (_block_size - sizeof(node)) / (sizeof(key_t) + sizeof(off_t));
    _max_entries = (_block_size - sizeof(node)) / (sizeof(key_t) + sizeof(long));
    // printf todo
    printf("config node order:%d and leaf entries:%d and _block_size:%d ,sizeof key_t:%lu\n", _max_order, _max_entries,
           _block_size, sizeof(key_t));

    /*申请和初始化节点缓存*/
    tree->caches = malloc(_block_size * MIN_CACHE_NUM);

    return tree;
}

/*
B+树的关闭操作
清空内存
*/
void bplus_tree_deinit(struct bplus_tree *tree, char *tree_addr, char *tree_boot_addr) {

    int len = get_tree_size(tree_addr);
    bzero(tree->fd, len);
    off_t file_size = get_boot_size(tree_boot_addr);
    bzero(tree_boot_addr, file_size);
    free(tree->caches);
    free(tree);
}

/*
返回节点的children个数
*/
static inline int children(struct bplus_node *node) {
    assert(!is_leaf(node));
    return node->children;
}

void bt_free(void *ptr) {
    free(ptr);
    ptr = NULL;
}
