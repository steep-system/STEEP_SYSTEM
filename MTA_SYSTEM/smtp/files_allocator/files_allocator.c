/*
 *    files allocator is a memory pool for mem files
 */
#include "common_types.h"
#include "mem_file.h"
#include "files_allocator.h"

static size_t g_blocks_num;
static LIB_BUFFER *g_allocator;

/*
 *    files allocator's construct function
 *    @param
 *        blocks    indicate the number of 
 */
void files_allocator_init(size_t blocks)
{
    g_blocks_num = blocks;
}

/*
 *    run the files allocator
 *    @return
 *        -1    fail to reserve memory
 *         0    OK
 */
int files_allocator_run()
{
    g_allocator = lib_buffer_init(FILE_ALLOC_SIZE, g_blocks_num, TRUE);
    if (NULL == g_allocator) {
        return -1;
    }
    return 0;
}

/*
 *    stop the files allocator
 *    @return
 *        -1    fail to stop
 *         0    OK
 */
int files_allocator_stop()
{
    if (NULL == g_allocator) {
        return 0;
    }
    lib_buffer_free(g_allocator);
    g_allocator = 0;
    return 0;
}

/*
 *    files allocator's destruct function
 */
void files_allocator_free()
{
    g_blocks_num = 0;
}

/*
 *    get the files allocator's allocator
 */
LIB_BUFFER* files_allocator_get_allocator()
{
    return g_allocator;
}

