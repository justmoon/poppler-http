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
  void reserveCacheSpace(size_t len) { chunks->resize(len/CachedFileChunkSize + 1); }
  ChunkState getChunkState(Guint chunk) {
  	if (chunk < chunks->size()) {
      return (*chunks)[chunk].state;
    } else {
      return chunkStateNew;
    }
  };
  void setChunkState(Guint chunk, ChunkState value) { (*chunks)[chunk].state = value; }
  const char *getChunkPointer(Guint chunkId) { return (*chunks)[chunkId].data; };
  char *startChunkUpdate(Guint chunkId) { return (*chunks)[chunkId].data; };
  void endChunkUpdate(Guint chunkId) { };

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
