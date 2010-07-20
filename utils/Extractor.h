//========================================================================
//
// Extractor.h
//
// This file is licensed under the GPLv2 or later
//
// Copyright (C) 2010 Hib Eris <hib@hiberis.nl>
//
//------------------------------------------------------------------------

#ifndef EXTRACTOR_H
#define EXTRACTOR_H

#include "Object.h"
#include "Gfx.h"
#include "GfxState.h"

//------------------------------------------------------------------------
// Extractor
//------------------------------------------------------------------------

class Extractor {

public:

  Extractor(FILE *fA);
  ~Extractor() {};

  int extract(PDFDoc *doc, int num, int gen, int begin, int end);
  int extractInlineImage(PDFDoc *doc, int pageNum, int begin);
  int extractFont(PDFDoc *doc, int pageNum, int num, int gen);

private:

  GfxImageColorMap *getColormap(Stream *str, GfxResources *res);
  int extractImage(Dict *dict, Stream *str, GfxResources *res);
  int extractFontDict(PDFDoc *doc, Dict *dict, Dict *parentResDict);

  FILE *f;

};

#endif



