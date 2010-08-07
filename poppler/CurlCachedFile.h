//========================================================================
//
// CurlCachedFile.h
//
// This file is licensed under the GPLv2 or later
//
// Copyright 2010 Hib Eris <hib@hiberis.nl>
// Copyright 2010 Albert Astals Cid <aacid@kde.org>
//
//========================================================================

#ifndef CURLCACHELOADER_H
#define CURLCACHELOADER_H

#include "poppler-config.h"
#include "CachedFile.h"

#include <curl/curl.h>

//------------------------------------------------------------------------

class CurlCachedFileLoader;

//------------------------------------------------------------------------

enum CurlCachedFileResponseState{
	curlResponseStatePreamble,
	curlResponseStateMeta,
	curlResponseStateData,
	curlResponseStateEpilogue
};

//------------------------------------------------------------------------

class CurlCachedFileResponseHandler {

public:

  CurlCachedFileResponseHandler(CurlCachedFileLoader *loaderA, CachedFileWriter *writerA);
  ~CurlCachedFileResponseHandler();
  size_t handleHeader(const char *ptr, size_t len);
  size_t handleBody(const char *ptr, size_t len);

private:

  CurlCachedFileLoader *loader;
  CachedFileWriter *writer;
  GooString *boundary;
  CurlCachedFileResponseState state;
  char *buffer;
  size_t bufferPos;
  size_t bufferLen;
  size_t dataLen;
  GBool isNonPartial;
  
  static void trim(GooString *subject);
  static GooString *getHeaderValue(const char *headerString, size_t len);
  static char *getNextHeader(char *bufferA);
  void enableMultipart(const char *boundaryA);
  void parseContentRange(const GooString *value);

};

//------------------------------------------------------------------------

class CurlCachedFileLoader : public CachedFileLoader {

friend class CurlCachedFileResponseHandler;

public:

  CurlCachedFileLoader();
  ~CurlCachedFileLoader();
  
  void setUrl(GooString *urlA);
  
  int load(const GooVector<ByteRange> &ranges, CachedFileWriter *writer);
  
  long getLatestHttpStatus();

private:

  GooString *url;
  CURL *curl;

};

#endif

