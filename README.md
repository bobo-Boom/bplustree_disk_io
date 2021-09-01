# 基于B+树的内存索引

支持加载、查询、范围操作


两个存储文件：index存储数据，boot存储B+树的信息


编译

```
make
make clean
```

运行

```
./bplustree_demo.out
```

库文件

```
lib
|----bplustree.h
|----bplustree.c
```

demo

```
bplustree_demo.c
```

迭代说明

```
更改 boot file 配置信息结构。
对应分支 ：  v1.3.1_hard_dev
BootFileSize|TreeRoot|TreeId|KyeType|BlockSize|TreeFileSize
```