//========================================================================
//
// MemoryCachedFile.cc
//
// This file is licensed under the GPLv2 or later
//
// Copyright 2010 Hib Eris <hib@hiberis.nl>
// Copyright 2010 Albert Astals Cid <aacid@kde.org>
// Copyright 2009-2010 Stefan Thomas <thomas@txtbear.com>
//
//========================================================================

#include <config.h>
#include "MemoryCachedFile.h"

//------------------------------------------------------------------------
// MemoryCachedFile
//------------------------------------------------------------------------

MemoryCachedFile::MemoryCachedFile(CachedFileLoader *cachedFileLoaderA, GooString *uriA) :
  CachedFile(cachedFileLoaderA, uriA)
{
  chunks = new GooVector<Chunk>();
  
  chunks->resize(getLength()/CachedFileChunkSize + 1);
}

MemoryCachedFile::~MemoryCachedFile()
{
  delete chunks;
}

//------------------------------------------------------------------------

