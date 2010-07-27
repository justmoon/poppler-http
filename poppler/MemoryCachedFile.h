//========================================================================
//
// MemoryCachedFile.h
//
// This file is licensed under the GPLv2 or later
//
// Copyright 2010 Hib Eris <hib@hiberis.nl>
// Copyright 2010 Albert Astals Cid <aacid@kde.org>
// Copyright 2010 Stefan Thomas <thomas@txtbear.com>
//
//========================================================================

#ifndef MEMORYCACHEDFILE_H
#define MEMORYCACHEDFILE_H

#include "poppler-config.h"
#include "CachedFile.h"

//------------------------------------------------------------------------

class MemoryCachedFile : public CachedFile {

public:

  MemoryCachedFile(CachedFileLoader *cachedFileLoaderA, GooString *uriA);

  size_t getCacheSize() { return chunks->size(); }
  void resizeCache(size_t numChunks) { chunks->resize(numChunks); }
  ChunkState getChunkState(int chunk) { return (*chunks)[chunk].state; };
  void setChunkState(int chunk, ChunkState value) { (*chunks)[chunk].state = value; }
  const char *getChunkPointer(int chunkId) { return (*chunks)[chunkId].data; };
  char *startChunkUpdate(int chunkId) { return (*chunks)[chunkId].data; };
  void endChunkUpdate(int chunkId) { };

protected:

  ~MemoryCachedFile();

private:

  typedef struct {
    ChunkState state;
    char data[CachedFileChunkSize];
  } Chunk;

  GooVector<Chunk> *chunks;

};

#endif
