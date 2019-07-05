#ifndef __RTXALLOCATORPOOL__
#define __RTXALLOCATORPOOL__

#include <vector>

template<class ElementType, int BucketSize = 1024>
class Pool
{
private:
  struct Bucket
  {
    ElementType array[BucketSize];
    int inUse;
    int wasUsed;
  };
  vector<Bucket*> bucketList;
  unsigned int idx;
  Bucket* cur;
  inline void getNextBucket()
  {
    idx++;
    if(idx == bucketList.size())
    {
      cur = new Bucket;
      cur->inUse = 0;
      cur->wasUsed = 0;
      bucketList.push_back(cur);
    }
    else
    {
      cur = bucketList[idx];
      cur->inUse = 0;
    }
  }
public:
  Pool()
  {
    idx = 0;
    Bucket* b = new Bucket;
    b->inUse = 0;
    b->wasUsed = 0;
    bucketList.push_back(b);
    cur = b;
  }
  ~Pool()
  {
    while(bucketList.size() > 0)
    {
      Bucket* cur = bucketList.back();
      bucketList.pop_back();
      delete cur;
    }
  }
  void reset()
  {
    idx = 0;
    bucketList[0]->inUse = 0;
    cur = bucketList[idx];
  }
  ElementType* next()
  {
    if(cur->inUse == BucketSize)
    {
      getNextBucket();
    }
    ElementType* ch = &cur->array[cur->inUse++];
    if(cur->inUse > cur->wasUsed)
    {
      cur->wasUsed++;
    }
    else
    {
      ch->~ElementType();
    }
    ch = new(ch) ElementType();
    return ch;
  }
};

#endif
