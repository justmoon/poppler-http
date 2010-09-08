//========================================================================
//
// pdfkeywords.cc
//
// Copyright 1997-2003 Glyph & Cog, LLC
//
// Modified for Debian by Hamish Moffatt, 22 May 2002.
//
//========================================================================

//========================================================================
//
// Modified under the Poppler project - http://poppler.freedesktop.org
//
// All changes made under the Poppler project to this file are licensed
// under GPL version 2 or later
//
// Copyright (C) 2006 Dominic Lachowicz <cinamod@hotmail.com>
// Copyright (C) 2007-2008, 2010 Albert Astals Cid <aacid@kde.org>
// Copyright (C) 2009 Jan Jockusch <jan@jockusch.de>
// Copyright (C) 2010 Hib Eris <hib@hiberis.nl>
// Copyright (C) 2010 Stefan Thomas <thomas@txtbear.com>
//
// To see a description of the changes please see the Changelog file that
// came with your tarball or type make ChangeLog if you are building from git
//
//========================================================================

#include "config.h"
#include <poppler-config.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include "parseargs.h"
#include "printencodings.h"
#include "goo/GooString.h"
#include "goo/gmem.h"
#include "GlobalParams.h"
#include "Object.h"
#include "Stream.h"
#include "Array.h"
#include "Dict.h"
#include "XRef.h"
#include "Catalog.h"
#include "Page.h"
#include "PDFDoc.h"
#include "PDFDocFactory.h"
#include "TextOutputDev.h"
#include "CharTypes.h"
#include "UnicodeMap.h"
#include "Error.h"

static int firstPage = 1;
static int lastPage = 0;
static double resolution = 72.0;
static int x = 0;
static int y = 0;
static int w = 0;
static int h = 0;
static GBool physLayout = gFalse;
static GBool rawOrder = gFalse;
static char textEncName[128] = "";
static char textEOL[16] = "";
static GBool noPageBreaks = gFalse;
static char ownerPassword[33] = "\001";
static char userPassword[33] = "\001";
static GBool quiet = gFalse;
static GBool printVersion = gFalse;
static GBool printHelp = gFalse;
static GBool printEnc = gFalse;

static const ArgDesc argDesc[] = {
  {"-f",       argInt,      &firstPage,     0,
   "first page to convert"},
  {"-l",       argInt,      &lastPage,      0,
   "last page to convert"},
  {"-r",      argFP,       &resolution,    0,
   "resolution, in DPI (default is 72)"},
  {"-x",      argInt,      &x,             0,
   "x-coordinate of the crop area top left corner"},
  {"-y",      argInt,      &y,             0,
   "y-coordinate of the crop area top left corner"},
  {"-W",      argInt,      &w,             0,
   "width of crop area in pixels (default is 0)"},
  {"-H",      argInt,      &h,             0,
   "height of crop area in pixels (default is 0)"},
  {"-layout",  argFlag,     &physLayout,    0,
   "maintain original physical layout"},
  {"-raw",     argFlag,     &rawOrder,      0,
   "keep strings in content stream order"},
  {"-enc",     argString,   textEncName,    sizeof(textEncName),
   "output text encoding name"},
  {"-listenc",argFlag,     &printEnc,      0,
   "list available encodings"},
  {"-eol",     argString,   textEOL,        sizeof(textEOL),
   "output end-of-line convention (unix, dos, or mac)"},
  {"-nopgbrk", argFlag,     &noPageBreaks,  0,
   "don't insert page breaks between pages"},
  {"-opw",     argString,   ownerPassword,  sizeof(ownerPassword),
   "owner password (for encrypted files)"},
  {"-upw",     argString,   userPassword,   sizeof(userPassword),
   "user password (for encrypted files)"},
  {"-q",       argFlag,     &quiet,         0,
   "don't print any messages or errors"},
  {"-v",       argFlag,     &printVersion,  0,
   "print copyright and version info"},
  {"-h",       argFlag,     &printHelp,     0,
   "print usage information"},
  {"-help",    argFlag,     &printHelp,     0,
   "print usage information"},
  {"--help",   argFlag,     &printHelp,     0,
   "print usage information"},
  {"-?",       argFlag,     &printHelp,     0,
   "print usage information"},
  {NULL}
};

static void TextOutputDev_outputNoop(void *stream, char *text, int len) {
  
}

int main(int argc, char *argv[]) {
  PDFDoc *doc;
  GooString *fileName;
  GooString *textFileName;
  GooString *ownerPW, *userPW;
  TextOutputDev *textOut;
  TextWordList *wordList;
  int wordListLength;
  FILE *f;
  UnicodeMap *uMap;
  Object info;
  GBool ok;
  char *p;
  char buffer[128];
  int exitCode, bufferLen;
  double xMin, xMax, yMin, yMax;

  exitCode = 99;

  // parse args
  ok = parseArgs(argDesc, &argc, argv);
  if (!ok || (argc < 2 && !printEnc) || argc > 3 || printVersion || printHelp) {
    fprintf(stderr, "pdfkeywords version %s\n", PACKAGE_VERSION);
    fprintf(stderr, "%s\n", popplerCopyright);
    fprintf(stderr, "%s\n", xpdfCopyright);
    if (!printVersion) {
      printUsage("pdfkeywords", "<PDF-file> [<text-file>]", argDesc);
    }
    goto err0;
  }

  // read config file
  globalParams = new GlobalParams();

  if (printEnc) {
    printEncodings();
    delete globalParams;
    goto err0;
  }

  fileName = new GooString(argv[1]);

  if (textEncName[0]) {
    globalParams->setTextEncoding(textEncName);
  }
  if (textEOL[0]) {
    if (!globalParams->setTextEOL(textEOL)) {
      fprintf(stderr, "Bad '-eol' value on command line\n");
    }
  }
  if (noPageBreaks) {
    globalParams->setTextPageBreaks(gFalse);
  }
  if (quiet) {
    globalParams->setErrQuiet(quiet);
  }

  // get mapping to output encoding
  if (!(uMap = globalParams->getTextEncoding())) {
    error(-1, "Couldn't get text encoding");
    delete fileName;
    goto err1;
  }

  // open PDF file
  if (ownerPassword[0] != '\001') {
    ownerPW = new GooString(ownerPassword);
  } else {
    ownerPW = NULL;
  }
  if (userPassword[0] != '\001') {
    userPW = new GooString(userPassword);
  } else {
    userPW = NULL;
  }

  if (fileName->cmp("-") == 0) {
      delete fileName;
      fileName = new GooString("fd://0");
  }

  doc = PDFDocFactory().createPDFDoc(*fileName, ownerPW, userPW);

  if (userPW) {
    delete userPW;
  }
  if (ownerPW) {
    delete ownerPW;
  }
  if (!doc->isOk()) {
    exitCode = 1;
    goto err2;
  }

#ifdef ENFORCE_PERMISSIONS
  // check for copy permission
  if (!doc->okToCopy()) {
    error(-1, "Copying of text from this document is not allowed.");
    exitCode = 3;
    goto err2;
  }
#endif

  // construct text file name
  if (argc == 3) {
    textFileName = new GooString(argv[2]);
  } else if (fileName->cmp("fd://0") == 0) {
     error(-1, "You have to provide an output filename when reading form stdin.");
     goto err2;
  } else {
    p = fileName->getCString() + fileName->getLength() - 4;
    if (!strcmp(p, ".pdf") || !strcmp(p, ".PDF")) {
      textFileName = new GooString(fileName->getCString(),
				 fileName->getLength() - 4);
    } else {
      textFileName = fileName->copy();
    }
    textFileName->append(".txt");
  }

  // get page range
  if (firstPage < 1) {
    firstPage = 1;
  }
  if (lastPage < 1 || lastPage > doc->getNumPages()) {
    lastPage = doc->getNumPages();
  }

  // write text file
  textOut = new TextOutputDev(TextOutputDev_outputNoop, NULL,
			      physLayout, rawOrder);
  if (!textOut->isOk()) {
    delete textOut;
    exitCode = 2;
    goto err3;
  }

  if (!textFileName->cmp("-")) {
    f = stdout;
  } else {
    if (!(f = fopen(textFileName->getCString(), "w"))) {
      error(-1, "Couldn't open text file '%s'", textFileName->getCString());
      exitCode = 2;
      goto err3;
    }
  }   

  for (int page = firstPage; page <= lastPage; ++page) {
    if ((w==0) && (h==0) && (x==0) && (y==0)) {
      doc->displayPages(textOut, page, page, resolution, resolution,
                        0, gTrue, gFalse, gFalse);
    } else {
      doc->displayPageSlice(textOut, page, resolution, resolution,
                            0, gTrue, gFalse, gFalse,
                            x, y, w, h);
    }

    wordList = textOut->makeWordList();
    wordListLength = wordList->getLength();

    for (int word = 0; word < wordListLength; ++word) {
      TextWord *w = wordList->get(word);
      GooString *s = new GooString();

      w->getBBox(&xMin, &yMin, &xMax, &yMax);

      bufferLen = sprintf(buffer, "%.0f", xMin);
      s->append(buffer, bufferLen);
      s->append(":");
      bufferLen = sprintf(buffer, "%.0f", yMin);
      s->append(buffer, bufferLen);
      s->append(":");
      bufferLen = sprintf(buffer, "%.0f", xMax);
      s->append(buffer, bufferLen);
      s->append(":");
      bufferLen = sprintf(buffer, "%.0f", yMax);
      s->append(buffer, bufferLen);
      s->append(":");

      s->append(w->getText());

      s->append("\n");

      fputs(s->getCString(), f);
    }
  }
  delete textOut;

  if (f != stdout) {
    fclose(f);
  }

  exitCode = 0;

  // clean up
 err3:
  delete textFileName;
 err2:
  delete doc;
  delete fileName;
  uMap->decRefCnt();
 err1:
  delete globalParams;
 err0:

  // check for memory leaks
  Object::memCheck(stderr);
  gMemReport(stderr);

  return exitCode;
}
