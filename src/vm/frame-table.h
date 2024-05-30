#ifndef VM_FRAME_TABLE_H
#define VM_FRAME_TABLE_H

#include "../lib/kernel/list.h"
#include "../lib/kernel/hash.h"
#include "../threads/thread.h"

typedef struct Frame {
    struct thread *owner; /* The (first) owner of this frame. */
    void *user_page;      /* Corresponding user page. */
    bool r;               /* For clock algorithm. */
} Frame;

void frame_table_init(void *user_pool_base, uint32_t user_pool_page_count);

void *allocate_user_page(void *user_address, bool writable, bool zeroed);
void free_all_user_pages(struct thread *thread);

#endif /* vm/frame-table.h */
