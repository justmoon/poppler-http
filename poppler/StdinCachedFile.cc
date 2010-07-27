//========================================================================
//
// StdinCachedFile.cc
//
// This file is licensed under the GPLv2 or later
//
// Copyright 2010 Hib Eris <hib@hiberis.nl>
// Copyright 2010 Albert Astals Cid <aacid@kde.org>
// Copyright 2010 Jonathan Liu <net147@gmail.com>
//
//========================================================================

#include <config.h>

#include "StdinCachedFile.h"

#ifdef _WIN32
#include <fcntl.h> // for O_BINARY
#include <io.h>    // for setmode
#endif
#include <stdio.h>

int StdinCacheLoader::load(const GooVector<ByteRange> &ranges, CachedFileWriter *writer)
{
  size_t read, size = 0;
  char buf[CachedFileChunkSize];

#ifdef _WIN32
  setmode(fileno(stdin), O_BINARY);
#endif

  writer->noteNonPartial();
  do {
    read = fread(buf, 1, CachedFileChunkSize, stdin);
    (writer->write) (buf, CachedFileChunkSize);
    size += read;
  }
  while (read == CachedFileChunkSize);
  writer->eof();

  return 0;
}

