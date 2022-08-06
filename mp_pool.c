#include<stdio.h>
#include<memory.h>
#include"mp_pool.h"


struct mp_pool_s *mp_create_pool(size_t size)
{
    struct mp_pool_s *pool;
    if(size < PAGE_SIZE || size % PAGE_SIZE != 0)
    {
        size = PAGE_SIZE;
    }
    int ret = posix_memalign((void**)&pool,MP_ALIGNMENT,size);
    if(ret)
    {
        return NULL;
    }
    pool->large = NULL;
    pool->curr = pool->head = (unsigned char *)pool + sizeof(struct mp_pool_s);
    pool->curr->last = (unsigned char*)pool + sizeof(struct mp_pool_s) + sizeof(struct mp_node_s);
    pool->head->end = (unsigned char*)pool + PAGE_SIZE;
    pool->head->failed = 0;
    return pool;
}


void mp_destroy_pool(struct mp_pool_s *pool)
{
    struct mp_large_s *large;

    for(large = pool->large;large;large = large->next)
    {
        if(large->alloc)
        {
            free(large->alloc);
        }
    }

    struct mp_node_s *cur,*next;
    cur = pool->head->next;

    while(cur)
    {
        next = cur->next;
        free(cur);
        cur= next;
    }

    free(pool);
}

void *mp_malloc(struct mp_pool_s *pool,size_t size)
{
    if(size <= 0)
    {
        return NULL;
    }
    if(size > PAGE_SIZE - sizeof(struct mp_node_s))
    {
        return mp_malloc_large(pool,size);
    }
    else
    {
        // small
        unsigned char *mem_addr = NULL;
        struct mp_node_s *cur = NULL;
        cur = pool->curr;
        while(cur)
        {
            mem_addr = mp_align_ptr(cur->last,MP_ALIGNMENT);
            if(cur->end-mem_addr >= size)
            {
                cur->quote++;
                cur->last = mem_addr + size;
                return mem_addr;
            }
            else
            {
                cur = cur->next;
            }
        }
        return mp_malloc_block(pool,size);   
    }
}

void *mp_calloc(struct mp_pool_s *pool,size_t size)
{
    void *mem_addr = mp_malloc(pool,size);
    if(mem_addr)
    {
        memset(mem_addr,0,size);
    }
    return mem_addr;
}

void *mp_malloc_block(struct mp_pool_s *pool,size_t size)
{
    unsigned char *block;
    int ret = posix_memalign((void**)&block,MP_ALIGNMENT,PAGE_SIZE);
    if(ret)
    {
        return NULL;
    }
    struct mp_node_s *new_node = (struct mp_node_s *)block;
    new_node->end = block + PAGE_SIZE;
    new_node->next = NULL;

    unsigned char *ret_addr = mp_align_ptr(block + sizeof(struct mp_node_s),MP_ALIGNMENT);

    new_node->last = ret_addr + size;
    new_node->quote++;

    struct mp_node_s *curr = pool->curr;
    struct mp_node_s *cur = NULL;

    for(cur = curr;cur->next;cur = cur->next)
    {
        if(cur->failed++ >4)
        {
            curr = cur->next;
        }
    }

    cur->next = new_node;
    pool->curr = curr;
    return ret_addr;
}

// 分配大内存
void *mp_malloc_large(struct mp_pool_s *pool,size_t size)
{
    unsigned char *big_addr;
    int ret = posix_memalign((void**)&big_addr,MP_ALIGNMENT,PAGE_SIZE);
    if(ret)
    {
        return NULL;
    }
    struct mp_large_s *large;
    int n = 0;
    for(large = pool->large;large;large = large->next)
    {
        if(large->alloc == NULL)
        {
            large->size = size;
            large->alloc = big_addr;
            return big_addr;
        }
        if(n++ >3)
        {
            break;
        }
    }
    large = mp_malloc(pool,sizeof(struct mp_large_s));
    if(large == NULL){
        free(big_addr);
        return NULL;
    }
    large->size = size;
    large->alloc = big_addr;
    large->next = pool->large;
    pool->large = large;
    return big_addr;
}


void mp_reset_pool(struct mp_pool_s *pool)
{
    struct mp_node_s *cur = NULL;
    struct mp_large_s *large = NULL;
    for(large = pool->large;large;large = large->next)
    {
        if(large->alloc)
        {
            free(large->alloc);
        }
    }
    pool->large = NULL;
    pool->curr = pool->head;
    for(cur = pool->head;cur;cur = cur->next)
    {
        cur->last = (unsigned char*)cur + sizeof(struct mp_node_s);
        cur->failed = 0;
        cur->quote = 0;
    }
}

void mp_free(struct mp_pool_s *pool,void *p)
{
    struct mp_large_s *large = NULL;
    for(large = pool->large;large;large = large->next)
    {
        if(p == large->alloc)
        {
            free(large->alloc);
            large->size = 0;
            large->alloc = NULL;
            return;
        }
    }
    struct mp_node_s *cur = NULL;
    for(cur = pool->head;cur;cur = cur->next)
    {
        if((unsigned char*) cur <= (unsigned char *)p && (unsigned char *) p <= (unsigned char *)cur->end)
        {
            cur->quote--;
            if(cur->quote == 0)
            {
                if(cur == pool->head)
                {
                    pool->head->last = (unsigned char *)pool + sizeof(struct mp_pool_s) + sizeof(struct mp_node_s);
                }
                else
                {
                    cur->last = (unsigned char *)cur + sizeof(struct mp_node_s);
                }
                cur->failed = 0;
                pool->curr = pool->head;
            }
            return;
        }
    }
}

void monitor_mp_pool(struct mp_pool_s *pool,char *tk)
{
    printf("\r\n\r----------------start monitor pool------------------%s\r\n\r",tk);
    struct mp_node_s *head = NULL;
    int i = 0;
    for(head = pool->head;head;head = head->next)
    {
        i++;
        if(pool->curr == head)
        {
            printf("current==>第%d块\n",i);
        }
        if(i == 1)
        {
            printf("第%02d块small block 已使用%4ld 剩余空间:%4ld 引用:%4d  failed%4d\n",i,(unsigned char*)head->last-(unsigned char *)pool,head->end-head->last,head->quote,head->failed);
        }
        else
        {
            printf("第%02d块small block 已使用%4ld 剩余空间:%4ld 引用:%4d  failed%4d\n",i,(unsigned char*)head->last-(unsigned char *)head,head->end-head->last,head->quote,head->failed);
        }
    }
    struct mp_large_s *large;
    i = 0;
    for(large = pool->large;large;large = large->next)
    {
        i++;
        if(large->alloc != NULL)
        {
            printf("第%d块large block size=%d\n",i,large->size);
        }
    }
    printf("\r\n\r\n---------------------------stop monitor pool------------------------------\r\n\r\n");
}

int main()
{
    struct mp_pool_s *pool = mp_create_pool(PAGE_SIZE);
    monitor_mp_pool(pool,"create memory pool");

    void *mp[30];

    for(int i = 0;i < 30;i++)
    {
        mp[i] = mp_malloc(pool,512);
    }
    monitor_mp_pool(pool,"申请512字节30个");

    for(int i = 0;i < 30;i++)
    {
        mp_free(pool,mp[i]);
    }
    monitor_mp_pool(pool,"销毁512字节30个");

    for(int i = 0;i < 50;i++)
    {
        char *pp = mp_calloc(pool,32);
        for(int j = 0;j < 32;j++)
        {
            if(pp[j])
            {
                printf("calloc wrong\n");
                exit(-1);
            }
        }
    }
    monitor_mp_pool(pool,"申请内存32字节50个");

    for(int i = 0;i < 50;i++)
    {
        char *p2 = mp_malloc(pool,3);
    }
    monitor_mp_pool(pool,"申请内存3字节50个");



    void *pptr[10];
    for(int i = 0;i < 10;i++)
    {
        pptr[i] = mp_malloc(pool,5120);
    }
    monitor_mp_pool(pool,"申请5120大内存10个");

    for(int i = 0;i < 10;i++)
    {
        mp_free(pool,pptr[i]);
    }
    monitor_mp_pool(pool,"销毁大内存10个");

    mp_reset_pool(pool);
    monitor_mp_pool(pool,"reset pool");

    for(int i = 0;i < 100;i++)
    {
        void *s = mp_malloc(pool,256);
    }
    monitor_mp_pool(pool,"申请256字节内存100个");
    mp_destroy_pool(pool);
    return 0;
}