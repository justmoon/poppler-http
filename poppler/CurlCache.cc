//========================================================================
//
// CurlCache.cc
//
// This file is licensed under the GPLv2 or later
//
// Copyright 2009 Stefan Thomas <thomas@eload24.com>
//
//========================================================================

#include "CurlCache.h"

#ifdef ENABLE_LIBCURL

#ifdef USE_GCC_PRAGMAS
#pragma implementation
#endif

#include <stdio.h>
#include <string.h>
#include "Error.h"
#include <curl/curl.h>

//------------------------------------------------------------------------

CurlCache::CurlCache(GooString *urlA) {
  url = urlA;

  long code = NULL;
  double contentLength = -1;

  curl = curl_easy_init();
  curl_easy_setopt(curl, CURLOPT_URL, url->getCString());
  curl_easy_setopt(curl, CURLOPT_HEADER, 1);
  curl_easy_setopt(curl, CURLOPT_NOBODY, 1);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &CurlCache::noop);
  curl_easy_perform(curl);
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
  curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &contentLength);
  curl_easy_reset(curl);
  
  size = contentLength;
  
  streamPos = 0;
}

CurlCache::~CurlCache() {
  curl_easy_cleanup(curl);
}

GooString *CurlCache::getFileName() {
  int i, sl = 0, qm = 0;
  for (i = 6; i < url->getLength(); i++) {
    // note position after last slash
    if (url->getChar(i) == '/') sl = i+1;
	
    // note position of first question mark
    if (url->getChar(i) == '?' && !qm) qm = i;
  }
  // find document filename
  return new GooString(url, sl, (qm) ? qm : (url->getLength()-sl));
}

long int CurlCache::tell() {
  return streamPos;
}

int CurlCache::seek(long int offset, int origin) {
  if (origin == SEEK_SET) {
    streamPos = offset;
  } else if (origin == SEEK_CUR) {
    streamPos += offset;
  } else {
    streamPos = size + offset;
  }
  
  if (streamPos > size) {
    streamPos = 0;
    return 1;
  }
  
  return 0;
}

size_t CurlCache::read(void *ptr, size_t unitsize, size_t count) {
  size_t bytes = unitsize*count;
  size_t endPos = streamPos + bytes;
  //printf("Reading %li - %li\n", streamPos, streamPos + unitsize*count);
  
  if (endPos > size) {
    endPos = size;
    bytes = size - streamPos;
  }
  
  if (bytes == 0) return 0;
  
  preload(streamPos, endPos);
  
  // Write data to buffer
  size_t toCopy = bytes;
  
  while (toCopy) {
    int chunk = streamPos / curlCacheChunkSize;
    int offset = streamPos % curlCacheChunkSize;
  
    int len = curlCacheChunkSize-offset;
  
    if (len > toCopy)
      len = toCopy;

    //printf("Reading Chunk %i, offset %i, len %i\n", chunk, offset, len);
    memcpy(ptr, chunks[chunk].data + offset, len);
    streamPos += len;
    toCopy -= len;
    ptr = (char*)ptr + len;
    
    /*
    // Dump a chunk
    if (chunk == 28 || chunk == 29) {
      for (int i = 0; i < len; ++i) {
        printf("%02X ", (unsigned char) chunks[chunk].data[offset + i]);
      }
      printf("\n");
    }
    */
  }
  
  return bytes;
}

void CurlCache::preload(size_t start, size_t end) {
  if (end == 0 || end > size) end = size;
  if (start > end) start = end - curlCacheChunkSize;

  int startBlock = start / curlCacheChunkSize;
  int startSkip = start % curlCacheChunkSize;
  
  int endBlock = (end-1) / curlCacheChunkSize;
  int endSkip = curlCacheChunkSize-1 - ((end-1) % curlCacheChunkSize);
  
  //printf("Get block %i to %i, skipping %i at start and %i at end.\n", startBlock, endBlock, startSkip, endSkip);
  
  // Make sure data is in cache
  loadChunks(startBlock, endBlock);
}

void CurlCache::loadChunks(int startBlock, int endBlock) {
  int startSequence;
  int i = startBlock;
  
  while (i <= endBlock) {
    if (chunks[i].state == cccStateNew) {
      startSequence = i;
      while (i < endBlock) {
        i++;
        if (chunks[i].state != cccStateNew) {
          i--;
          break;
        }
      }
      
      CurlCacheJob *ccj = new CurlCacheJob(this, startSequence, i);
      ccj->run();
    }
    i++;
  }
}

size_t CurlCache::noop(void *ptr, size_t size, size_t nmemb, void *ptr2) {
  return size*nmemb;
}

//------------------------------------------------------------------------

CurlCacheJob::CurlCacheJob(CurlCache *ccA, int startBlockA, int endBlockA) {
  //printf("Getting blocks %i to %i\n", startBlockA, endBlockA);
  cc = ccA;
  startBlock = startBlockA;
  endBlock = endBlockA;
}

void CurlCacheJob::run() {
  size_t fromByte = startBlock * curlCacheChunkSize;
  size_t toByte = ((endBlock+1) * curlCacheChunkSize)-1;
  
  if (toByte >= cc->size-1) {
    toByte = cc->size-1;
  }
  
  GooString *range = GooString::format("{0:ud}-{1:ud}", fromByte, toByte);
  //printf("Range: %s\n", range->getCString());

  currentByte = fromByte;

  curl_easy_setopt(cc->curl, CURLOPT_URL, cc->url->getCString());
  curl_easy_setopt(cc->curl, CURLOPT_WRITEFUNCTION, &CurlCacheJob::write);
  curl_easy_setopt(cc->curl, CURLOPT_WRITEDATA, this);
  curl_easy_setopt(cc->curl, CURLOPT_RANGE, range->getCString());
  curl_easy_perform(cc->curl);
  curl_easy_reset(cc->curl);
}

size_t CurlCacheJob::write(void *ptr, size_t size, size_t nmemb, CurlCacheJob *ccj) {
  //printf("%u bytes received\n", size*nmemb);
  size_t toCopy = size*nmemb;
  
  while (toCopy) {
    int chunk = ccj->currentByte / curlCacheChunkSize;
    int offset = ccj->currentByte % curlCacheChunkSize;
  
    size_t len = curlCacheChunkSize-offset;
  
    if (len > toCopy)
      len = toCopy;

    //printf("Writing Chunk %i, offset %i, len %i\n", chunk, offset, len);
    memcpy(&ccj->cc->chunks[chunk].data[offset], ptr, len);
    ccj->currentByte += len;
    toCopy -= len;
    ptr = (char*)ptr + len;
    
    ccj->cc->chunks[chunk].state = cccStateLoaded;
  }
  
  return size*nmemb;
}

#endif
