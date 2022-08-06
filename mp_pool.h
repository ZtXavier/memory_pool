#ifndef _MP_POOL_H_
#define _MP_POOL_H_
#include<unistd.h>
#include<stdlib.h>

#define PAGE_SIZE 4096
#define MP_ALIGNMENT 16

// 每4k创建一个block节点
struct mp_node_s{
    unsigned char *end; // 块的结尾
    unsigned char *last; // 最新的节点
    struct mp_node_s *next;
    int quote; // 引用计数
    int failed; // 失效次数
};

struct mp_large_s{
    struct mp_large_s *next;
    int size; //alloc 的大小
    void *alloc; // 大块内存的起始地址
};


// 定义curr的原因是如果一个block剩余空间小于size超过一定要次数后,将current指向下一块
// block,加快内存分配效率,减少遍历次数
struct mp_pool_s{
    struct mp_large_s *large;
    struct mp_node_s *head;
    struct mp_node_s *curr;
};

// 创建一个内存池,通过mp_pool_s这个结构体来申请4k内存,将各个指针指向上文初始状态的图
struct mp_pool_s *mp_create_pool(size_t size);
// 销毁内存池
void mp_destroy_pool(struct mp_pool_s *pool);
// 申请内存的api
void *mp_malloc(struct mp_pool_s *pool,size_t size);

void *mp_malloc_block(struct mp_pool_s *pool,size_t size);

void *mp_malloc_large(struct mp_pool_s *pool,size_t size);

void *mp_calloc(struct mp_pool_s *pool,size_t size);
// 释放mp_malloc返回的内存
void mp_free(struct mp_pool_s *pool,void *p);
// 将block的last置为初始状态,销毁所有的大内存
void mp_reset_pool(struct mp_pool_s *pool);
// 监控内存池的状态
void monitor_mp_pool(struct mp_pool_s *pool,char *tk);


//我们在分配内存时需要内存对齐机制来提高访问速度
// 某些arm平台不支持未对其的内存访问,会出错
#define mp_align(n,alignment) (((n) + (alignment-1))& ~(alignment-1))
#define mp_align_ptr(p,alignment) (void*)((((size_t)p) + (alignment-1)) & ~(alignment-1))



















#endif