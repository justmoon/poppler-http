//========================================================================
//
// CachedFile.cc
//
// This file is licensed under the GPLv2 or later
//
// Copyright 2009 Stefan Thomas <thomas@eload24.com>
// Copyright 2010 Hib Eris <hib@hiberis.nl>
// Copyright 2010 Albert Astals Cid <aacid@kde.org>
//
//========================================================================

#include <config.h>
#include "CachedFile.h"

// Size of the header (always load the first n bytes)
#define CachedFileHeaderSize 32768

//------------------------------------------------------------------------
// CachedFile
//------------------------------------------------------------------------

CachedFile::CachedFile(CachedFileLoader *cachedFileLoaderA, GooString *uriA)
{
  uri = uriA;
  loader = cachedFileLoaderA;

  streamPos = 0;
  chunks = new GooVector<Chunk>();
  length = 0;

  length = loader->init(uri, this);
  refCnt = 1;

  chunks->resize(length/CachedFileChunkSize + 1);
}

CachedFile::~CachedFile()
{
  delete uri;
  delete loader;
  delete chunks;
}

void CachedFile::incRefCnt() {
  refCnt++;
}

void CachedFile::decRefCnt() {
  if (--refCnt == 0)
    delete this;
}

long int CachedFile::tell() {
  return streamPos;
}

int CachedFile::seek(long int offset, int origin)
{
  if (origin == SEEK_SET) {
    streamPos = offset;
  } else if (origin == SEEK_CUR) {
    streamPos += offset;
  } else {
    streamPos = length + offset;
  }

  if (streamPos > length) {
    streamPos = 0;
    return 1;
  }

  return 0;
}

int CachedFile::cache(const GooVector<ByteRange> &origRanges)
{
  GooVector<int> loadChunks;
  int numChunks = length/CachedFileChunkSize + 1;
  GooVector<bool> chunkNeeded(numChunks);
  int startChunk, endChunk;
  GooVector<ByteRange> chunk_ranges, all;
  ByteRange range;
  const GooVector<ByteRange> *ranges = &origRanges;

  if (ranges->empty()) {
    range.offset = 0;
    range.length = length;
    all.push_back(range);
    ranges = &all;
  }

  memset(&chunkNeeded[0], 0, sizeof(bool) * numChunks);
  for (size_t i = 0; i < ranges->size(); i++) {

    if ((*ranges)[i].length == 0) continue;
    if ((*ranges)[i].offset >= length) continue;

    size_t start = (*ranges)[i].offset;
    size_t end = start + (*ranges)[i].length - 1;
    if (end >= length) end = length - 1;

    startChunk = start / CachedFileChunkSize;
    endChunk = end / CachedFileChunkSize;
    for (int chunk = startChunk; chunk <= endChunk; chunk++) {
      if ((*chunks)[chunk].state == chunkStateNew) {
           chunkNeeded[chunk] = true;
      }
    }
  }

  int chunk = 0;
  while (chunk < numChunks) {
    while (!chunkNeeded[chunk] && (++chunk != numChunks)) ;
    if (chunk == numChunks) break;
    startChunk = chunk;
    loadChunks.push_back(chunk);

    while ((++chunk != numChunks) && chunkNeeded[chunk]) {
      loadChunks.push_back(chunk);
    }
    endChunk = chunk - 1;

    range.offset = startChunk * CachedFileChunkSize;
    range.length = (endChunk - startChunk + 1) * CachedFileChunkSize;

    chunk_ranges.push_back(range);
  }

  if (chunk_ranges.size() > 0) {
    CachedFileWriter writer =
        CachedFileWriter(this, &loadChunks);
    return loader->load(chunk_ranges, &writer);
  }

  return 0;
}

size_t CachedFile::read(void *ptr, size_t unitsize, size_t count)
{
  size_t bytes = unitsize*count;
  if (length < (streamPos + bytes)) {
    bytes = length - streamPos;
  }

  if (bytes == 0) return 0;
  
  // If the first block is requested, we'll actually cache a bit more than what
  // the user requested. This is a quick fix to make reading the header faster.
  size_t toCopy = bytes;
  if (streamPos == 0 && bytes < CachedFileHeaderSize && CachedFileHeaderSize < length)
    bytes = CachedFileHeaderSize;

  // Load data
  if (cache(streamPos, bytes) != 0) return 0;

  // Copy data to buffer
  while (toCopy) {
    int chunk = streamPos / CachedFileChunkSize;
    int offset = streamPos % CachedFileChunkSize;
    size_t len = CachedFileChunkSize-offset;

    if (len > toCopy)
      len = toCopy;

    memcpy(ptr, (*chunks)[chunk].data + offset, len);
    streamPos += len;
    toCopy -= len;
    ptr = (char*)ptr + len;
  }

  return bytes;
}

int CachedFile::cache(size_t offset, size_t length)
{
  GooVector<ByteRange> r;
  ByteRange range;
  range.offset = offset;
  range.length = length;
  r.push_back(range);
  return cache(r);
}

//------------------------------------------------------------------------
// CachedFileWriter
//------------------------------------------------------------------------

CachedFileWriter::CachedFileWriter(CachedFile *cachedFileA, GooVector<int> *chunksA)
{
   cachedFile = cachedFileA;
   chunks = chunksA;

   if (chunks) {
     offset = 0;
     it = (*chunks).begin();
   }
}

CachedFileWriter::~CachedFileWriter()
{
}

size_t CachedFileWriter::write(const char *ptr, size_t size)
{
  const char *cp = ptr;
  size_t len = size;
  size_t nfree, ncopy;
  size_t written = 0;
  size_t chunk;

  if (!len) return 0;

  while (len) {
    if (chunks) {
      if (offset == CachedFileChunkSize) {
         it++;
         if (it == (*chunks).end()) return written;
         offset = 0;
      }
      chunk = *it;
    } else {
      offset = cachedFile->length % CachedFileChunkSize;
      chunk = cachedFile->length / CachedFileChunkSize;
    }

    if (chunk >= cachedFile->chunks->size()) {
       cachedFile->chunks->resize(chunk + 1);
    }

    nfree = CachedFileChunkSize - offset;
    ncopy = (len >= nfree) ? nfree : len;
    memcpy(&((*cachedFile->chunks)[chunk].data[offset]), cp, ncopy);
    len -= ncopy;
    cp += ncopy;
    offset += ncopy;
    written += ncopy;

    if (!chunks) {
      cachedFile->length += ncopy;
    }

    if (offset == CachedFileChunkSize) {
       (*cachedFile->chunks)[chunk].state = CachedFile::chunkStateLoaded;
    }
  }

  if ((chunk == (cachedFile->length / CachedFileChunkSize)) &&
      (offset == (cachedFile->length % CachedFileChunkSize))) {
     (*cachedFile->chunks)[chunk].state = CachedFile::chunkStateLoaded;
  }

  return written;
}

//------------------------------------------------------------------------

