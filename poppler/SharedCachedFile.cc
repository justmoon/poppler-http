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
    error(-1, "Unable to access shared memory");
    exit(1);
  }
  
  // Load the size from the cache file if we have one
  fstat(datafd, &datastat);
  if (datastat.st_size) {
    setLength(datastat.st_size);
  }
  
  if (ftruncate(datafd, getLength())) {
    error(-1, "Unable to resize shared memory");
    exit(1);
  }
  
  data = (char *) mmap(NULL, getLength(), PROT_READ | PROT_WRITE, MAP_SHARED, datafd, 0);
  if (data == NULL) {
    error(-1, "Unable to map shared memory");
    exit(1);
  }
  
  // Open shared meta memory
  filename->append("-meta");
  
  metafd = shm_open(filename->getCString(), O_RDWR | O_CREAT, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
  if (metafd < 0) {
    error(-1, "Unable to access shared memory");
    exit(1);
  }
  
  cacheSize = getLength()/CachedFileChunkSize + 1;
  if (ftruncate(metafd, cacheSize*sizeof(ChunkState))) {
    error(-1, "Unable to resize shared memory");
    exit(1);
  }
  
  meta = (ChunkState *) mmap(NULL, cacheSize*sizeof(ChunkState), PROT_READ | PROT_WRITE, MAP_SHARED, metafd, 0);
  if (meta == NULL) {
    error(-1, "Unable to map shared memory");
    exit(1);
  }
  
  delete filename;
}

SharedCachedFile::~SharedCachedFile()
{
}

void SharedCachedFile::resizeCache(size_t numChunks)
{
  if (ftruncate(datafd, numChunks*CachedFileChunkSize)) {
    error(-1, "Unable to resize shared memory");
    exit(1);
  }
  
  if (ftruncate(metafd, numChunks*sizeof(ChunkState))) {
    error(-1, "Unable to resize shared memory");
    exit(1);
  }
  
  data = (char *) mremap(data, cacheSize*CachedFileChunkSize, numChunks*CachedFileChunkSize, MREMAP_MAYMOVE);
  if (data == MAP_FAILED) {
    error(-1, "mremap failed");
    exit(1);
  }
  
  meta = (ChunkState *) mremap(meta, cacheSize*sizeof(ChunkState), numChunks*sizeof(ChunkState), MREMAP_MAYMOVE);
  if (meta == MAP_FAILED) {
    error(-1, "mremap failed");
    exit(1);
  }
  
  cacheSize = numChunks;
}

//------------------------------------------------------------------------

