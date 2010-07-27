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

  size_t getCacheSize() { return cacheSize; }
  void resizeCache(size_t numChunks);
  ChunkState getChunkState(int chunk) { return meta[chunk]; };
  void setChunkState(int chunk, ChunkState value) { meta[chunk] = value; }
  const char *getChunkPointer(int chunkId) { return data + chunkId*CachedFileChunkSize; };
  char *startChunkUpdate(int chunkId) { return data + chunkId*CachedFileChunkSize; };
  void endChunkUpdate(int chunkId) { };

protected:

  ~SharedCachedFile();

private:

  char *data;
  ChunkState *meta;
  int datafd;
  int metafd;
  
  size_t cacheSize;

};

#endif
