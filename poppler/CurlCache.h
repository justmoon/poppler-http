//========================================================================
//
// CurlCache.h
//
// Caching wrapper around curl.
//
// This file is licensed under the GPLv2 or later
//
// Copyright 2009 Stefan Thomas <thomas@eload24.com>
//
//========================================================================

#ifndef CURLCACHE_H
#define CURLCACHE_H

#include <config.h>

#ifdef ENABLE_LIBCURL

#ifdef USE_GCC_PRAGMAS
#pragma interface
#endif

#include "poppler-config.h"
#include "goo/gtypes.h"
#include "goo/GooString.h"

#include <curl/curl.h>

#include <map>

//------------------------------------------------------------------------

#define curlCacheChunkSize 8192

enum CurlCacheChunkState {
  cccStateNew,
// If we want this to be thread-safe, concurrent, whatever, we need another state:
// cccStateLoading,
  cccStateLoaded
};

typedef struct {
  CurlCacheChunkState state;
  char data[curlCacheChunkSize];
} CurlCacheChunk;

class CurlCache {
public:

  friend class CurlCacheJob;

  CurlCache(GooString *urlA);
  ~CurlCache();
  
  GooString *getFileName();
  
  long int tell();
  int seek(long int offset, int origin);
  size_t read(void * ptr, size_t unitsize, size_t count);
  void preload(long int start, long int end);
  
  void loadChunks(int startBlock, int endBlock);

private:

  CURL *curl;
  GooString *url;
  long int size;
  long int streamPos;
  
  std::map<unsigned, CurlCacheChunk> chunks;
  
  static size_t noop(void *ptr, size_t size, size_t nmemb, void *ptr2);

};

class CurlCacheJob {
public:

  CurlCacheJob(CurlCache *ccA, int startBlockA, int endBlockA);
  
  void run();

private:

  CurlCache *cc;
  int startBlock;
  int endBlock;
  int currentByte;
  
  static size_t write(void *ptr, size_t size, size_t nmemb, CurlCacheJob *ccj);

};

#endif

#endif
