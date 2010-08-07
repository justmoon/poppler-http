//========================================================================
//
// SharedCachedFile.h
//
// This file is licensed under the GPLv2 or later
//
// Copyright 2010 Stefan Thomas <thomas@txtbear.com>
//
//========================================================================

#ifndef SHAREDCACHEDFILE_H
#define SHAREDCACHEDFILE_H

#include "poppler-config.h"
#include "CachedFile.h"

//------------------------------------------------------------------------

class SharedCachedFile : public CachedFile {

public:

  SharedCachedFile(CachedFileLoader *cachedFileLoaderA, GooString *uriA);
  
  void setLength(Guint lengthA);

  size_t getCacheSize() { return cacheSize; }
  void reserveCacheSpace(size_t len);
  ChunkState getChunkState(Guint chunk) {
    if (chunk < cacheSize) {
      return meta[chunk];
    } else {
      return chunkStateNew;
    }
  };
  void setChunkState(Guint chunk, ChunkState value) { meta[chunk] = value; }
  const char *getChunkPointer(Guint chunkId) { return data + chunkId*CachedFileChunkSize; };
  char *startChunkUpdate(Guint chunkId) { return data + chunkId*CachedFileChunkSize; };
  void endChunkUpdate(Guint chunkId) { };

protected:

  ~SharedCachedFile();

private:

  void *mapFile(int fd, size_t len);
  void *remapFile(int fd, void *ptr, size_t oldLen, size_t newLen);

  char *data;
  ChunkState *meta;
  int datafd;
  int metafd;
  
  size_t cacheSize;

};

#endif
