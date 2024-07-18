// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.

#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

struct
{
  struct spinlock lock[NBUCKET];
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct buf head[NBUCKET];
} bcache;

void binit(void)
{
  struct buf *b;

  for (int i = 0; i < NBUCKET; i++)
  {
    initlock(&bcache.lock[i], "bcache");
  }
  // Create linked list of buffers
  // bcache.head[0].prev = &bcache.head[0];
  bcache.head[0].next = &bcache.buf[0];
  for (b = bcache.buf; b < bcache.buf + NBUF - 1; b++)
  {
    b->next = b + 1;
    initsleeplock(&b->lock, "buffer");
  }
  initsleeplock(&b->lock, "buffer");
}

void write_cache(struct buf *take_buf, uint dev, uint blockno)
{
  take_buf->dev = dev;
  take_buf->blockno = blockno;
  take_buf->valid = 0;
  take_buf->refcnt = 1;
  take_buf->time = ticks;
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf *
bget(uint dev, uint blockno)
{
  struct buf *b, *last;
  struct buf *take_buf = 0;
  int id = HASH(blockno);
  acquire(&(bcache.lock[id]));

  b = bcache.head[id].next;
  last = &(bcache.head[id]);
  for (; b; b = b->next, last = last->next)
  {

    if (b->dev == dev && b->blockno == blockno)
    {
      b->time = ticks;
      b->refcnt++;
      release(&(bcache.lock[id]));
      acquiresleep(&b->lock);
      return b;
    }
    if (b->refcnt == 0)
    {
      take_buf = b;
    }
  }

  if (take_buf)
  {
    write_cache(take_buf, dev, blockno);
    release(&(bcache.lock[id]));
    acquiresleep(&(take_buf->lock));
    return take_buf;
  }

  int lock_num = -1;

  uint64 time = __UINT64_MAX__;
  struct buf *tmp;
  struct buf *last_take = 0;
  for (int i = 0; i < NBUCKET; ++i)
  {

    if (i == id)
      continue;
    acquire(&(bcache.lock[i]));

    for (b = bcache.head[i].next, tmp = &(bcache.head[i]); b; b = b->next, tmp = tmp->next)
    {
      if (b->refcnt == 0)
      {
        if (b->time < time)
        {

          time = b->time;
          last_take = tmp;
          take_buf = b;
          if (lock_num != -1 && lock_num != i && holding(&(bcache.lock[lock_num])))
            release(&(bcache.lock[lock_num]));
          lock_num = i;
        }
      }
    }

    if (lock_num != i)
      release(&(bcache.lock[i]));
  }

  if (!take_buf)
    panic("bget: no buffers");

  last_take->next = take_buf->next;
  take_buf->next = 0;
  release(&(bcache.lock[lock_num]));

  b = last;
  b->next = take_buf;
  write_cache(take_buf, dev, blockno);

  release(&(bcache.lock[id]));
  acquiresleep(&(take_buf->lock));

  return take_buf;
}

// Return a locked buf with the contents of the indicated block.
struct buf *
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if (!b->valid)
  {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void bwrite(struct buf *b)
{
  if (!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void brelse(struct buf *b)
{
  if (!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  int h = HASH(b->blockno);
  acquire(&bcache.lock[h]);
  b->refcnt--;
  release(&bcache.lock[h]);
}

void bpin(struct buf *b)
{
  acquire(&bcache.lock[HASH(b->blockno)]);
  b->refcnt++;
  release(&bcache.lock[HASH(b->blockno)]);
}

void bunpin(struct buf *b)
{
  acquire(&bcache.lock[HASH(b->blockno)]);
  b->refcnt--;
  release(&bcache.lock[HASH(b->blockno)]);
}
