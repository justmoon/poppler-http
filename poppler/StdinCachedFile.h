//========================================================================
//
// StdinCachedFile.h
//
// This file is licensed under the GPLv2 or later
//
// Copyright 2010 Hib Eris <hib@hiberis.nl>
// Copyright 2010 Albert Astals Cid <aacid@kde.org>
//
//========================================================================

#ifndef STDINCACHELOADER_H
#define STDINCACHELOADER_H

#include "CachedFile.h"

class StdinCacheLoader : public CachedFileLoader {

public:

  void setUrl(GooString *urlA) {}
  int load(const GooVector<ByteRange> &ranges, CachedFileWriter *writer);

};

#endif

