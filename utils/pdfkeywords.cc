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
#include "goo/GooHash.h"
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
static GBool relCoord = gFalse;
static GBool pctCoord = gFalse;
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
  {"-relcoord",argFlag,     &relCoord,      0,
   "make coords a fraction of page dimensions"},
  {"-pctcoord",argFlag,     &pctCoord,      0,
   "make coords a percentage of page dimensions"},
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

#define UNKNOWN_CHAR if (s->getLength() && s->getChar(s->getLength()-1) != '-') \
    s->append('-');

static GooString *normalize_keyword(GooString *original)
{
  GooString *s = new GooString();
  int i = 0;
  unsigned char lb;

  while ((lb = original->getChar(i))) {
    if (( lb & 0x80 ) == 0 ) {           // lead bit is zero, single ascii
      if ((lb >= 65 && lb <= 90)) { // A-Z
        s->append(lb+32); // to lowercase
      } else if ((lb >= 97 && lb <= 122) || // a-z
                 (lb >= 48 && lb <= 57) ||  // 0-9
                 lb == '-' || lb == '(' || lb == ')') {
        s->append(lb);
      } else {
        UNKNOWN_CHAR
      } 
      i++;
    } else if (( lb & 0xE0 ) == 0xC0 ) { // 110x xxxx; 2 octets
      switch (lb) {
      case 194:
		switch ((unsigned char)original->getChar(i+1)) {
        case 169: // ©
          s->append("(c)");
          break;
        case 174: // ®
          s->append("(r)");
          break;
        case 185: // ¹
          s->append('1');
          break;
        case 178: // ²
          s->append('2');
          break;
        case 179: // ³
          s->append('3');
          break;
        case 170: // ª
          s->append('a');
          break;
        case 186: // º
          s->append('o');
          break;
        case 181: // µ
          s->append('u');
          break;
        default:
          UNKNOWN_CHAR
        }
		break;
      case 195:
		switch ((unsigned char)original->getChar(i+1)) {
        case 164: // ä
          s->append("ae");
          break;
        case 132: // Ä
          s->append("ae");
          break;
        case 166: // æ
          s->append("ae");
          break;
        case 134: // Æ
          s->append("ae");
          break;
        case 182: // ö
          s->append("oe");
          break;
        case 150: // Ö
          s->append("oe");
          break;
        case 188: // ü
          s->append("ue");
          break;
        case 156: // Ü
          s->append("ue");
          break;
        case 159: // ß
          s->append("ss");
          break;
        case 128: // À
          s->append('a');
          break;
        case 129: // Á
          s->append('a');
          break;
        case 130: // Â
          s->append('a');
          break;
        case 131: // Ã
          s->append('a');
          break;
        case 133: // Å
          s->append('a');
          break;
        case 135: // Ç
          s->append('c');
          break;
        case 144: // Ð
          s->append('d');
          break;
        case 136: // È
          s->append('e');
          break;
        case 137: // É
          s->append('e');
          break;
        case 138: // Ê
          s->append('e');
          break;
        case 139: // Ë
          s->append('e');
          break;
        case 140: // Ì
          s->append('i');
          break;
        case 141: // Í
          s->append('i');
          break;
        case 142: // Î
          s->append('i');
          break;
        case 143: // Ï
          s->append('i');
          break;
        case 145: // Ñ
          s->append('n');
          break;
        case 146: // Ò
          s->append('o');
          break;
        case 147: // Ó
          s->append('o');
          break;
        case 148: // Ô
          s->append('o');
          break;
        case 149: // Õ
          s->append('o');
          break;
        case 152: // Ø
          s->append('o');
          break;
        case 153: // Ù
          s->append('u');
          break;
        case 154: // Ú
          s->append('u');
          break;
        case 155: // Û
          s->append('u');
          break;
        case 157: // Ý
          s->append('y');
          break;
        case 160: // à
          s->append('a');
          break;
        case 161: // á
          s->append('a');
          break;
        case 162: // â
          s->append('a');
          break;
        case 163: // ã
          s->append('a');
          break;
        case 165: // å
          s->append('a');
          break;
        case 167: // ç
          s->append('c');
          break;
        case 168: // è
          s->append('e');
          break;
        case 169: // é
          s->append('e');
          break;
        case 170: // ê
          s->append('e');
          break;
        case 171: // ë
          s->append('e');
          break;
        case 172: // ì
          s->append('i');
          break;
        case 173: // í
          s->append('i');
          break;
        case 174: // î
          s->append('i');
          break;
        case 175: // ï
          s->append('i');
          break;
        case 177: // ñ
          s->append('n');
          break;
        case 178: // ò
          s->append('o');
          break;
        case 179: // ó
          s->append('o');
          break;
        case 180: // ô
          s->append('o');
          break;
        case 181: // õ
          s->append('o');
          break;
        case 184: // ø
          s->append('o');
          break;
        case 185: // ù
          s->append('u');
          break;
        case 186: // ú
          s->append('u');
          break;
        case 187: // û
          s->append('u');
          break;
        case 151: // ×
          s->append('x');
          break;
        case 189: // ý
          s->append('y');
          break;
        case 191: // ÿ
          s->append('y');
          break;
        default:
          UNKNOWN_CHAR
            }
		break;
      default:
		UNKNOWN_CHAR
          }
      i+=2;
    } else if (( lb & 0xF0 ) == 0xE0 ) { // 1110 xxxx; 3 octets
      if (lb == 0xe2 &&
          original->getChar(i+1) == 0x84 &&
          original->getChar(i+2) == 0xa2) {
        s->append("(TM)");
      } else {
		UNKNOWN_CHAR
      }
      i+=3;
    } else if (( lb & 0xF8 ) == 0xF0 ) { // 1111 0xxx; 4 octets
      UNKNOWN_CHAR
      i+=4;
    } else
      error(-1, "Unrecognized lead byte (%02x)\n", lb);
  }

  i = s->getLength()-1;
  if (s->getChar(i) == '-') s->del(i);

  return s;
}

int main(int argc, char *argv[]) {
  PDFDoc *doc;
  GooString *fileName;
  GooString *textFileName;
  GooString *ownerPW, *userPW;
  TextOutputDev *textOut;
  TextWordList *wordList;
  int wordListLength;
  GooHash *keywordIndex;
  FILE *f;
  UnicodeMap *uMap;
  Object info;
  GBool ok;
  char *p;
  const char *coordFormat;
  char buffer[128];
  int exitCode, bufferLen, coordMult;
  double xMin, xMax, yMin, yMax, width, height, pageWidth = 0.0, pageHeight = 0.0;
  GooHashIter *iter;
  GooString *key;
  void *value;

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

  if (pctCoord) relCoord = gTrue;
  coordFormat = relCoord ? "%.3f" : "%.0f";
  coordMult = pctCoord ? 100 : 1;

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

  keywordIndex = new GooHash();

  for (int page = firstPage; page <= lastPage; ++page) {
    if ((w==0) && (h==0) && (x==0) && (y==0)) {
      doc->displayPages(textOut, page, page, resolution, resolution,
                        0, gTrue, gFalse, gFalse);
    } else {
      doc->displayPageSlice(textOut, page, resolution, resolution,
                            0, gTrue, gFalse, gFalse,
                            x, y, w, h);
    }

    if (relCoord) {
      pageWidth = doc->getPageMediaWidth(page);
      pageHeight = doc->getPageMediaHeight(page);
    }

    wordList = textOut->makeWordList();
    wordListLength = wordList->getLength();

    for (int word = 0; word < wordListLength; ++word) {
      TextWord *w = wordList->get(word);
      GooString *s = normalize_keyword(w->getText());

      if (s->getLength() == 0) {
        delete s;
        continue;
      }

      // fetch keyword string if it exists, otherwise add it
      value = keywordIndex->lookup(s);

      if (value == NULL) {
        keywordIndex->add(new GooString(s), (void*)s);
      } else {
        delete s;
        s = (GooString*) value;
      }

      w->getBBox(&xMin, &yMin, &xMax, &yMax);

      width = xMax - xMin;
      height = yMax - yMin;

      if (relCoord) {
        xMin = xMin/pageWidth * coordMult;
        yMin = yMin/pageHeight * coordMult;
        width = width/pageWidth * coordMult;
        height = height/pageHeight * coordMult;
      }

      s->append(";");
      s->appendf("{0:d}", page);
      s->append(":");
      bufferLen = sprintf(buffer, coordFormat, xMin);
      s->append(buffer, bufferLen);
      s->append(":");
      bufferLen = sprintf(buffer, coordFormat, yMin);
      s->append(buffer, bufferLen);
      s->append(":");
      bufferLen = sprintf(buffer, coordFormat, width);
      s->append(buffer, bufferLen);
      s->append(":");
      bufferLen = sprintf(buffer, coordFormat, height);
      s->append(buffer, bufferLen);
    }
  }
  delete textOut;

  if (!textFileName->cmp("-")) {
    f = stdout;
  } else {
    if (!(f = fopen(textFileName->getCString(), "w"))) {
      error(-1, "Couldn't open text file '%s'", textFileName->getCString());
      exitCode = 2;
      goto err3;
    }
  }

  keywordIndex->startIter(&iter);
  while (keywordIndex->getNext(&iter, &key, &value)) {
    // just using key as a temp var because it happens to be a GooString*
    key = (GooString *) value;
    fputs(key->getCString(), f);
    fputc('\n', f);
  }
  keywordIndex->killIter(&iter);
  deleteGooHash (keywordIndex, GooString);

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
