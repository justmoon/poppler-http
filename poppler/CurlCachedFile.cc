//========================================================================
//
// CurlCachedFile.cc
//
// This file is licensed under the GPLv2 or later
//
// Copyright 2009 Stefan Thomas <thomas@eload24.com>
// Copyright 2010 Hib Eris <hib@hiberis.nl>
// Copyright 2010 Albert Astals Cid <aacid@kde.org>
//
//========================================================================

#include <config.h>

#include <cctype>

#include "CurlCachedFile.h"

#include "goo/GooString.h"
#include "goo/GooVector.h"

//------------------------------------------------------------------------

// Initial length of the response buffer handler
#define RESPONSE_HANDLER_BUFFER_LEN 32768

const char httpInitial[] = "HTTP";
const char httpContentLength[] = "content-length";
const char httpContentType[] = "content-type";
const char httpContentRange[] = "content-range";
const char httpHeaderSeparator = ':';
const char httpRangeUnit[] = "bytes";

const char mimeMultipartByteranges[] = "multipart/byteranges";
const char mimeSeparator = ';';
const char mimeMultipartBoundary[] = "boundary";
const char mimeValueSeparator = '=';

//------------------------------------------------------------------------
// CurlCachedFileResponseHandler
//------------------------------------------------------------------------

CurlCachedFileResponseHandler::CurlCachedFileResponseHandler(CurlCachedFileLoader *loaderA,
    CachedFileWriter *writerA)
{
	loader = loaderA;
	writer = writerA;
	boundary = NULL;
	state = curlResponseStateData;
	
	buffer = (char*)malloc(RESPONSE_HANDLER_BUFFER_LEN);
	bufferLen = RESPONSE_HANDLER_BUFFER_LEN;
	bufferPos = 0;
	
	isNonPartial = gFalse;
}

CurlCachedFileResponseHandler::~CurlCachedFileResponseHandler()
{
  if (boundary) delete boundary;
  
  free(buffer);
}

void CurlCachedFileResponseHandler::enableMultipart(const char *boundaryA)
{
  if (boundary) delete boundary;
  
  boundary = new GooString("\r\n--");
  boundary->append(boundaryA);
  
  state = curlResponseStatePreamble;
}

inline GBool is_space(char c)
{
  return (c == '\r' || c == '\n' || c == ' ' || c == '\t');
}

void CurlCachedFileResponseHandler::trim(GooString *subject)
{
  int i = 0;
  int len = subject->getLength();
  
  // Delete leading whitespace
  while (i < len && is_space(subject->getChar(i)))
    i++;
  
  subject->del(0, i);
  
  // Delete trailing whitespace
  for (i = subject->getLength()-1; i >= 0; i--) {
    if (!is_space(subject->getChar(i))) {
      i++;
      break;
    }
  }
  
  subject->del(i, len-i);
}

GooString *CurlCachedFileResponseHandler::getHeaderValue(const char *headerString, size_t len)
{
  const char *headerContent;
  
  // Skip to separator
  headerContent = strchr(headerString, httpHeaderSeparator);
  
  // Skip past separator
  headerContent++;
  
  GooString *result = new GooString(headerContent, len - (headerContent-headerString));
  
  // Remove whitespace
  trim(result);
  
  return result;
}

char *CurlCachedFileResponseHandler::getNextHeader(char *bufferA)
{
  static char *buffer = NULL;
  
  int endPos;
  char *result;
  
  if (bufferA != NULL) {
    buffer = bufferA;
  }
  
  assert(buffer != NULL);
  
  if (buffer[0] == '\0' || buffer[1] == '\0' || buffer[2] == '\0') return NULL;
  
  endPos = 2;
  while (buffer[endPos] != '\0') {
    if (buffer[endPos-2] == '\r' &&
        buffer[endPos-1] == '\n' &&
       (buffer[endPos] == '\r' || !is_space(buffer[endPos]))) {
      result = (char*)malloc(endPos+1);
      memcpy(result, buffer, endPos);
      result[endPos] = '\0';
      
      buffer += endPos;
      return result;
    }
    endPos++;
  }
  
  return NULL;
}

size_t CurlCachedFileResponseHandler::handleHeader(const char *ptr, size_t len)
{
  GooString *value;
  
  if (strncasecmp(ptr, httpInitial, strlen(httpInitial)) == 0) {
	  if (loader->getLatestHttpStatus() == 200) {
	    // HTTP status 200 means we're about to receive a full file response
	    writer->noteNonPartial();
	    isNonPartial = gTrue;
	  }
	} else if (strncasecmp(ptr, httpContentLength, strlen(httpContentLength)) == 0) {
    value = getHeaderValue(ptr, len);
  	dataLen = atoi(value->getCString());
  	
  	if (isNonPartial) writer->setLength(dataLen);
  	
    delete value;
  	
  	if (!dataLen) return 0; // error, invalid content-length
	} else if (strncasecmp(ptr, httpContentType, strlen(httpContentType)) == 0) {
	  value = getHeaderValue(ptr, len);
	  
	  if (value->cmpN(mimeMultipartByteranges, strlen(mimeMultipartByteranges)) == 0) {
	    char *boundary = value->getCString();
	    
	    // Skip past semicolon
	    boundary = strchr(boundary, mimeSeparator);
	    if (boundary == NULL) {
	  	  error(-1, "Server sent an invalid multipart response (no semicolon after multitype/byteranges)");
	  	  return 0;
	  	}
	  	boundary++;
	    
	    // Skip past "boundary"
	    while (1) {
  	    if (*boundary == '\0') {
	  	    error(-1, "Server sent an invalid multipart response (no boundary specified)");
	  	    return 0;
	      } else if (strncasecmp(boundary, mimeMultipartBoundary, strlen(mimeMultipartBoundary)) == 0) {
	        boundary += strlen(mimeMultipartBoundary);
	        break;
	  	  } else {
	  	    boundary++;
	  	  }
	    }
	  	
	  	// Skip past "="
	    boundary = strchr(boundary, mimeValueSeparator);
	    if (boundary == NULL) {
	  	  error(-1, "Server sent an invalid multipart response (no equals sign after \"boundary\")");
	  	  return 0;
	  	}
	  	boundary++;
	    
    	enableMultipart(boundary);
    }
    delete value;
	} else if (strncasecmp(ptr, httpContentRange, strlen(httpContentRange)) == 0) {
	  value = getHeaderValue(ptr, len);
	  
	  parseContentRange(value);
	  
	  delete value;
	}
	
	return len;
}

void CurlCachedFileResponseHandler::parseContentRange(const GooString *value)
{
  char *byteInfo = value->getCString();

  byteInfo = strstr(byteInfo, httpRangeUnit);
  if (byteInfo == NULL) {
    error(-1, "Server sent an invalid multipart response (no \"bytes\" in content-range part header)");
    return;
  }
  byteInfo += strlen(httpRangeUnit);

  char *startByteString = strtok(byteInfo, "-/");
  char *endByteString = strtok(NULL, "-/");
  char *totalByteString = strtok(NULL, "-/");

  dataLen = atoi(endByteString) - atoi(startByteString) + 1;
  state = curlResponseStateData;

  // And we can determine the total file size from this header as well
  writer->setLength(atoi(totalByteString));
}

size_t CurlCachedFileResponseHandler::handleBody(const char *ptr, size_t len)
{
	size_t written = 0;
	size_t resultLen;
  char *cursor;

  // If this is not a multipart response, we can pass it through unchanged
	if (!boundary) return (writer->write) (ptr, len);
	
  while (len) {
	  if (state == curlResponseStateData) {
	  	size_t writeLen = (len > dataLen) ? dataLen : len;
	  	
	  	resultLen = (writer->write) (ptr, writeLen);
	  	if (resultLen != writeLen) return 0;
	  	
	  	ptr += resultLen;
	  	len -= resultLen;
	  	dataLen -= resultLen;
	  	written += resultLen;
	  	
	  	if (dataLen == 0) {
	  		// End of data reached
	  		state = curlResponseStateMeta;
	  		bufferPos = 0;
	  	}
	  } else if (state == curlResponseStateMeta || state == curlResponseStatePreamble) {
	  	buffer[bufferPos++] = *ptr++;
	  	
	  	written++;
	  	len--;
	  	
	  	if (bufferPos == bufferLen) {
	  	  // TODO: Instead of failing, we should grow the buffer up to a maximum
	  	  error(-1, "Server sent an invalid multipart response (part headers too long)");
	  	  return 0;
	  	}
	  	
	  	if (buffer[bufferPos-1] == '\n' &&
	  	    buffer[bufferPos-2] == '\r' &&
	  	    buffer[bufferPos-3] == '\n' &&
	  	    buffer[bufferPos-4] == '\r') {
	  	  // End of multipart header, stop buffering
	  	  buffer[bufferPos] = '\0';
	  	  
	  	  // Skip boundary
	  	  char *subheader = strstr(buffer, boundary->getCString());
  	    if (state == curlResponseStatePreamble) {
  	      if (subheader == NULL) {
            // The preamble may contain double newlines, so we may just still
            // be in the preamble, let's keep buffering.
            continue;
          }
  	    } else {
  	      // After the preamble, all boundaries must line up exactly
  	      if (subheader != buffer) {
  	  	  	error(-1, "Server sent an invalid multipart response (boundary not found)");
     	  	  return 0;
     	  	}
    	  }
	  	  subheader += boundary->getLength();
	  	  
	  	  if (subheader[0] == '-' && subheader[1] == '-') {
	  	    // Reached the final boundary marker, denoting the epilogue
	  	    state = curlResponseStateEpilogue;
	  	    continue;
	  	  } else if (subheader[0] != '\r' || subheader[1] != '\n') {
	  	  	error(-1, "Server sent an invalid multipart response (missing CRLF after boundary)");
   	  	  return 0;
	  	  }
	  	  
	  	  dataLen = 0;
	  	  cursor = getNextHeader(subheader);
	  	  while (cursor != NULL) {
	  	    // Look for content-range header
          if (strncasecmp(cursor, httpContentRange, strlen(httpContentRange)) == 0) {
            GooString *value = getHeaderValue(cursor, strlen(cursor));
            
            parseContentRange(value);
            
            delete value;
            free(cursor);
            break;
          }

          free(cursor);
          cursor = getNextHeader(NULL);
	  	  }
	  	  
	  	  if (dataLen == 0) {
	    	  // No content-range header found
          error(-1, "Server sent an invalid multipart response (couldn't find content-range header for part)");
          return 0;
        }
	  	}
	  } else if (state == curlResponseStateEpilogue) {
	  	written++;
	  	len--;
	  }
	}
  
  return written;
}

//------------------------------------------------------------------------
// CurlCachedFileLoader
//------------------------------------------------------------------------

CurlCachedFileLoader::CurlCachedFileLoader()
{
  url = NULL;
  curl = curl_easy_init();
}

CurlCachedFileLoader::~CurlCachedFileLoader() {
  curl_easy_cleanup(curl);
}

void CurlCachedFileLoader::setUrl(GooString *urlA)
{
  url = urlA;
}

long CurlCachedFileLoader::getLatestHttpStatus()
{
  long code = NULL;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
  return code;
}

static
size_t head_cb(const char *ptr, size_t size, size_t nmemb, void *data)
{
  CurlCachedFileResponseHandler *handler = (CurlCachedFileResponseHandler *) data;
  return (handler->handleHeader) (ptr, size*nmemb);
}

static
size_t load_cb(const char *ptr, size_t size, size_t nmemb, void *data)
{
  CurlCachedFileResponseHandler *handler = (CurlCachedFileResponseHandler *) data;
  return (handler->handleBody) (ptr, size*nmemb);
}

int CurlCachedFileLoader::load(const GooVector<ByteRange> &ranges, CachedFileWriter *writer)
{
  CURLcode r = CURLE_OK;
  size_t fromByte, toByte;
  char *newUrl;
  GooString *range = new GooString();
  CurlCachedFileResponseHandler *handler = new CurlCachedFileResponseHandler(this, writer);
  
  for (size_t i = 0; i < ranges.size(); i++) {
		fromByte = ranges[i].offset;
		toByte = fromByte + ranges[i].length - 1;
		if (range->getLength()) range->append(',');
		range->appendf("{0:ud}-{1:ud}", fromByte, toByte);
  }
  
 start:

  curl_easy_setopt(curl, CURLOPT_URL, url->getCString());
  curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, head_cb);
  curl_easy_setopt(curl, CURLOPT_HEADERDATA, handler);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, load_cb);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, handler);
  curl_easy_setopt(curl, CURLOPT_RANGE, range->getCString());
  curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 25);
  r = curl_easy_perform(curl);
  
  curl_easy_getinfo(curl, CURLINFO_REDIRECT_URL, &newUrl);
  if (newUrl) {
    // Follow redirect and retry
    url->clear();
    url->append(newUrl);
    curl_easy_reset(curl);
    goto start;
  }
  
  curl_easy_reset(curl);
  
  writer->eof();

	delete handler;
  delete range;
  
  return r;
}

//------------------------------------------------------------------------

