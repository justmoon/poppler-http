//========================================================================
//
// CachedFile.h
//
// Caching files support.
//
// This file is licensed under the GPLv2 or later
//
// Copyright 2009 Stefan Thomas <thomas@eload24.com>
// Copyright 2010 Hib Eris <hib@hiberis.nl>
// Copyright 2010 Albert Astals Cid <aacid@kde.org>
//
//========================================================================

#ifndef CACHEDFILE_H
#define CACHEDFILE_H

#include "poppler-config.h"

#include "goo/gtypes.h"
#include "Object.h"
#include "Stream.h"
#include "goo/GooVector.h"

//------------------------------------------------------------------------

#define CachedFileChunkSize 8192 // This should be a multiple of cachedStreamBufSize

class GooString;
class CachedFileLoader;

//------------------------------------------------------------------------
// CachedFile
//
// CachedFile gives FILE-like access to a document at a specified URI.
// In the constructor, you specify a CachedFileLoader that handles loading
// the data from the document. The CachedFile requests no more data then it
// needs from the CachedFileLoader.
//------------------------------------------------------------------------

class CachedFile {

friend class CachedFileWriter;

protected:

  enum ChunkState {
    chunkStateNew = 0,
    chunkStateLoaded
  };

public:

  CachedFile(CachedFileLoader *cacheLoader, GooString *uriA);

  void loadHeader();
  Guint getLength();
  CachedFileLoader *getLoader();
  
  long int tell();
  int seek(long int offset, int origin);
  size_t read(void * ptr, size_t unitsize, size_t count);
  size_t write(const char *ptr, size_t size, size_t fromByte);
  int cache(const GooVector<ByteRange> &ranges);
  
  virtual size_t getCacheSize() = 0;
  virtual void reserveCacheSpace(size_t len) = 0;
  virtual ChunkState getChunkState(Guint chunk) = 0;
  virtual void setChunkState(Guint chunk, ChunkState value) = 0;
  virtual const char *getChunkPointer(Guint chunkId) = 0;
  virtual char *startChunkUpdate(Guint chunkId) = 0;
  virtual void endChunkUpdate(Guint chunkId) = 0;

  // Reference counting.
  void incRefCnt();
  void decRefCnt();

protected:

  virtual ~CachedFile();
  
  virtual void setLength(Guint lengthA);
  Guint length;
  GBool lengthKnown;
  
private:

  int cache(size_t offset, size_t length);

  CachedFileLoader *loader;
  
  GooString *uri;

  size_t streamPos;

  int refCnt;  // reference count

};

//------------------------------------------------------------------------
// CachedFileWriter
//
// CachedFileWriter handles sequential writes to a CachedFile.
// On construction, you specify the CachedFile and the chunks of it to which data
// should be written.
//------------------------------------------------------------------------

class CachedFileWriter {

public:

  // Construct a CachedFile Writer.
  // The caller is responsible for deleting the cachedFile and chunksA.
  CachedFileWriter(CachedFile *cachedFile, GooVector<int> *chunksA);

  ~CachedFileWriter();
  
  // Notifies the writer that the loader was unable to retrieve a partial
  // response and will be sending a full response instead.
  void noteNonPartial();
  
  // Informs the writer of the total length of the file
  void setLength(size_t len);

  // Writes size bytes from ptr to cachedFile, returns number of bytes written.
  size_t write(const char *ptr, size_t size);
  
  // Signals to the writer that the operation is complete
  void eof();

private:

  CachedFile *cachedFile;
  GooVector<int> *chunks;
  GooVector<int>::iterator it;
  size_t offset;
  size_t pos; // used only in non-partial mode

};

//------------------------------------------------------------------------
// CachedFileLoader
//
// CachedFileLoader is an abstract class that specifies the interface for
// loading data into a CachedFile.
//------------------------------------------------------------------------

class CachedFileLoader {

public:

  virtual ~CachedFileLoader() {};
  
  // Set source url
  // The caller is responsible for deleting url.
  // Note that the loader may modify url, for example in case of a redirect.
  virtual void setUrl(GooString *urlA) = 0;

  // Loads speficified byte ranges and passes it to the writer to store them.
  // Returns 0 on success, Anything but 0 on failure.
  // The caller is responsible for deleting the writer.
  virtual int load(const GooVector<ByteRange> &ranges, CachedFileWriter *writer) = 0;

};

//------------------------------------------------------------------------

#endif
