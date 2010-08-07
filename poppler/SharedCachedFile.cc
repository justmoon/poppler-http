//========================================================================
//
// SharedCachedFile.cc
//
// This file is licensed under the GPLv2 or later
//
// Copyright 2010 Hib Eris <hib@hiberis.nl>
// Copyright 2010 Albert Astals Cid <aacid@kde.org>
// Copyright 2009-2010 Stefan Thomas <thomas@txtbear.com>
//
//========================================================================

#include <config.h>
#include "SharedCachedFile.h"

#include "goo/sha1.h"

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

//------------------------------------------------------------------------
// SharedCachedFile
//------------------------------------------------------------------------

SharedCachedFile::SharedCachedFile(CachedFileLoader *cachedFileLoaderA, GooString *uriA) :
  CachedFile(cachedFileLoaderA, uriA)
{
  unsigned char hash[20];
  char hexstring[41];
  GooString *filename;
  struct stat datastat;
  
  sha1::calc(uriA->getCString(), uriA->getLength(), hash);
  sha1::toHexString(hash, hexstring);
  
  // Open shared data memory
  filename = new GooString("poppler-");
  filename->append(hexstring);
  
  datafd = shm_open(filename->getCString(), O_RDWR | O_CREAT, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
  if (datafd < 0) {
    error(-1, "Unable to access shared memory data");
    exit(1);
  }
  
  // Open shared meta memory
  filename->append("-meta");
  
  metafd = shm_open(filename->getCString(), O_RDWR | O_CREAT, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
  if (metafd < 0) {
    error(-1, "Unable to access shared memory metadata");
    exit(1);
  }
  
  delete filename;
  
  // Load the size from the cache file if we have one
  fstat(datafd, &datastat);
  if (datastat.st_size) {
    setLength(datastat.st_size);
  } else {
  	loadHeader();
  }
}

SharedCachedFile::~SharedCachedFile()
{
}

void SharedCachedFile::setLength(Guint lengthA)
{
  size_t numChunks = lengthA/CachedFileChunkSize + 1;
  
  if (length == lengthA) return;
  
  CachedFile::setLength(lengthA);
  
  if (!data) {
    data = (char *) mapFile(datafd, length);
  } else {
    data = (char *) remapFile(datafd, (void *)data, numChunks*CachedFileChunkSize, length);
  }
  
  if (!meta) {
    meta = (ChunkState *) mapFile(metafd, numChunks*sizeof(ChunkState));
  } else {
    meta = (ChunkState *) remapFile(metafd, (void *)meta, cacheSize*sizeof(ChunkState), numChunks*sizeof(ChunkState));
  }
  
  cacheSize = numChunks;
}

void SharedCachedFile::reserveCacheSpace(size_t len)
{
  size_t numChunks = len/CachedFileChunkSize + 1;
  
  if (lengthKnown) return;
  
  if (!data) {
    data = (char *) mapFile(datafd, numChunks*CachedFileChunkSize);
  } else {
    data = (char *) remapFile(datafd, (void *)data, cacheSize*CachedFileChunkSize, numChunks*CachedFileChunkSize);
  }
  
  if (!meta) {
    meta = (ChunkState *) mapFile(metafd, numChunks*sizeof(ChunkState));
  } else {
    meta = (ChunkState *) remapFile(metafd, (void *)meta, cacheSize*sizeof(ChunkState), numChunks*sizeof(ChunkState));
  }
  
  cacheSize = numChunks;
}

void *SharedCachedFile::mapFile(int fd, size_t len)
{
  void *ptr;

  printf("Map %d\n", len);

  if (ftruncate(fd, len)) {
    error(-1, "Unable to resize shared memory data");
    exit(1);
  }
  
  ptr = (void *) mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (ptr == NULL) {
    error(-1, "Unable to map shared memory");
    exit(1);
  }
  
  return ptr;
}

void *SharedCachedFile::remapFile(int fd, void *ptr, size_t oldLen, size_t newLen)
{
  printf("Remap %d->%d\n", oldLen, newLen);
  
  if (ftruncate(fd, newLen)) {
    error(-1, "Unable to resize shared memory data");
    exit(1);
  }
  
  ptr = (void *) mremap(ptr, oldLen, newLen, MREMAP_MAYMOVE);
  if (ptr == MAP_FAILED) {
    error(-1, "mremap failed");
    exit(1);
  }
  
  return ptr;
}

//------------------------------------------------------------------------

