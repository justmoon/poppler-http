//========================================================================
//
// pdfexref.cc
//
// Copyright (C) 2010 Hib Eris <hib@hiberis.nl>
//
// pdfexref is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// pdfexref is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
//
//========================================================================
//
// This file is based on pdftoppm.cc from the Poppler project, which has
// the following copyright:
//
//------------------------------------------------------------------------
//
// pdftoppm.cc
//
// Copyright 2003 Glyph & Cog, LLC
//
//========================================================================

//========================================================================
//
// Modified under the Poppler project - http://poppler.freedesktop.org
//
// All changes made under the Poppler project to this file are licensed
// under GPL version 2 or later
//
// Copyright (C) 2007 Ilmari Heikkinen <ilmari.heikkinen@gmail.com>
// Copyright (C) 2008 Richard Airlie <richard.airlie@maglabs.net>
// Copyright (C) 2009 Michael K. Johnson <a1237@danlj.org>
// Copyright (C) 2009 Shen Liang <shenzhuxi@gmail.com>
// Copyright (C) 2009 Stefan Thomas <thomas@eload24.com>
// Copyright (C) 2009, 2010 Albert Astals Cid <aacid@kde.org>
// Copyright (C) 2010 Adrian Johnson <ajohnson@redneon.com>
// Copyright (C) 2010 Hib Eris <hib@hiberis.nl>
//
// To see a description of the changes please see the Changelog file that
// came with your tarball or type make ChangeLog if you are building from git
//
//------------------------------------------------------------------------


#include "config.h"
#include <poppler-config.h>
#include "parseargs.h"
#include "goo/GooString.h"
#include "goo/GooList.h"
#include "GlobalParams.h"
#include "Object.h"
#include "Lexer.h"
#include "Parser.h"
#include "PDFDoc.h"
#include "PDFDocFactory.h"
#include "DCTStream.h"
#include "Gfx.h"
#include "GfxFont.h"
#include "GfxState.h"
#include "PSOutputDev.h"

static char ownerPassword[33] = "";
static char userPassword[33] = "";
static GBool quiet = gFalse;
static GBool printVersion = gFalse;
static GBool printHelp = gFalse;

static const ArgDesc argDesc[] = {
  {"-opw",    argString,   ownerPassword,  sizeof(ownerPassword),
   "owner password (for encrypted files)"},
  {"-upw",    argString,   userPassword,   sizeof(userPassword),
   "user password (for encrypted files)"},

  {"-q",      argFlag,     &quiet,         0,
   "don't print any messages or errors"},
  {"-v",      argFlag,     &printVersion,  0,
   "print copyright and version info"},
  {"-h",      argFlag,     &printHelp,     0,
   "print usage information"},
  {"-help",   argFlag,     &printHelp,     0,
   "print usage information"},
  {"--help",  argFlag,     &printHelp,     0,
   "print usage information"},
  {"-?",      argFlag,     &printHelp,     0,
   "print usage information"},
  {NULL}
};

GfxImageColorMap *get_colormap(Stream *str, GfxResources *res)
{
  GfxImageColorMap *colorMap;
  int bits;

  Object cs, decode;
  GfxColorSpace *colorSpace;

  str->getDict()->lookup("ColorSpace", &cs);
  if (cs.isNull()) {
    cs.free();
    str->getDict()->lookup("CS", &cs);
  }

  if (cs.isName() && res) {
    Object cs2;
    res->lookupColorSpace(cs.getName(), &cs2);
    if (!cs2.isNull()) {
      cs.free();
      cs = cs2;
    } else {
      cs2.free();
    }
  }

  colorSpace = GfxColorSpace::parse(&cs, NULL);
  cs.free();

  if (!colorSpace) {
    error(str->getPos(), "Bad image parameters");
    return NULL;
  }

  str->getDict()->lookupInt("BitsPerComponent", "BPC", &bits);

  str->getDict()->lookup("Decode", &decode);
  if (decode.isNull()) {
     decode.free();
     str->getDict()->lookup("D", &decode);
  }

  colorMap = new GfxImageColorMap(bits, &decode, colorSpace);
  decode.free();

  return colorMap;
}

int dump_image(FILE *f, Dict *dict, Stream *str, GfxResources *res)
{
  Object filter;
  int c;

  dict->lookup("Filter", &filter);
  if (filter.isName("DCTDecode") || filter.isName("JPXDecode")) {
    // dump JPEG / JPEG2000 file

    str = str->getUndecodedStream();
    str->reset();
    while ((c = str->getChar()) != EOF) {
      fputc(c, f);
    }

  } else  {

    int width, height;

    dict->lookupInt("Width", "W", &width);
    dict->lookupInt("Height", "H", &height);

    GfxImageColorMap *colorMap = get_colormap(str, res);
    if (!colorMap) {
      error(str->getPos(), "Invalid colormap");
      return -1;
    }
    if (colorMap->getNumPixelComps() == 1 && colorMap->getBits() == 1) {
      // dump PBM file

      Guchar zero = 0;
      GfxGray gray;
      int pbm_mask = 0xff;

      fprintf(f, "P4\n");
      fprintf(f, "%d %d\n", width, height);

      str->reset();

      // if 0 comes out as 0 in the color map, the we _flip_ stream bits
      // otherwise we pass through stream bits unmolested
      colorMap->getGray(&zero, &gray);
      if(colToByte(gray))
        pbm_mask = 0;

      // copy the stream
      int size = height * ((width + 7) / 8);
      for (int i = 0; i < size; ++i) {
        fputc(str->getChar() ^ pbm_mask, f);
      }
    } else {
      // dump PPM file

      GfxRGB rgb;

      fprintf(f, "P6\n");
      fprintf(f, "%d %d\n", width, height);
      fprintf(f, "255\n");

      // initialize stream
      ImageStream *imgStr = new ImageStream(str,
           width, colorMap->getNumPixelComps(), colorMap->getBits());
      imgStr->reset();

      // for each line...
      for (int y = 0; y < height; ++y) {
        // write the line
        Guchar *p = imgStr->getLine();
        for (int x = 0; x < width; ++x) {
          colorMap->getRGB(p, &rgb);
          fputc(colToByte(rgb.r), f);
          fputc(colToByte(rgb.g), f);
          fputc(colToByte(rgb.b), f);
          p += colorMap->getNumPixelComps();
        }
      }

      imgStr->close();
      delete imgStr;
    }
    delete colorMap;
  }
  filter.free();
  str->close();
  return 0;
}

static void outputToFile(void *stream, char *data, int len) {
  fwrite(data, 1, len, (FILE *)stream);
}

int dump_font(FILE *f, PDFDoc *doc, Dict *dict, Dict *parentResDict)
{
  PSOutputDev *psOut;
  GfxFont *font;

  int firstPage = 0;
  int lastPage = 0;
  PSOutMode mode = psModePS;
  int paperWidth = -1;
  int paperHeight = -1;
  GBool duplex = gFalse;
  char *title = "dummy title";
  int imgLLX = 0;
  int imgLLY = 0;
  int imgURX = 0;
  int imgURY = 0;
  GBool forceRasterize = gFalse;
  GBool manualCrtl = gTrue;
  psOut = new PSOutputDev(&outputToFile, (void *) f, title,
      doc, doc->getXRef(), doc->getCatalog(), firstPage, lastPage, mode,
      paperWidth, paperHeight, duplex, imgLLX, imgLLY, imgURX, imgURY,
      forceRasterize, manualCrtl);
  if (!psOut->isOk()) {
    delete psOut;
    error(2, "Failed to create ps output device");
    return 2;
  }

  char *dummyTag = "dummy";
  Ref dummyRef;
  dummyRef.num = 0;
  dummyRef.gen = 0;
  font = GfxFont::makeFont(doc->getXRef(), dummyTag, dummyRef, dict);
  psOut->setupFont(font, parentResDict);
  font->decRefCnt();

  delete psOut;

  return 0;
}

int dump_object(FILE *f, PDFDoc *doc, int num, int gen, int begin, int end)
{
  int r;
  Object obj;
  Stream *str;
  Dict *dict;
  Object subtype;

  doc->getXRef()->fetch(num, gen, &obj);
  while (obj.isRef()) {
    num = obj.getRefNum();
    gen = obj.getRefGen();
    obj.free();
    doc->getXRef()->fetch(num, gen, &obj);
  }

  if (obj.isBool()) {
    fprintf (f, "%s\n", obj.getBool() ? "true" : "false");
    r = 0;
  } else if (obj.isInt()) {
    fprintf (f, "%d\n", obj.getInt());
    r = 0;
  } else if (obj.isReal()) {
    fprintf (f, "%f\n", obj.getReal());
    r = 0;
  } else if (obj.isString()) {
    fprintf (f, "%s\n", obj.getString()->getCString());
    r = 0;
  } else if (obj.isStream()) {
    dict = obj.streamGetDict();
    str = obj.getStream();
    dict->lookup("Subtype", &subtype);
    if (subtype.isName("Image")) {
      r = dump_image(f, dict, str, NULL);
    } else {
      int c;
      str->reset();
      if (begin > 0) {
        for (int i = 0; i < begin; i++) {
          if (str->getChar() == EOF) {
            error(1, "Failed to read byte %d from stream", i);
            r = 1;
            break;
          }
        }
      }
      if (end > 0) {
        for (int i = begin; i < end; i++) {
          if ((c = str->getChar()) == EOF) {
            error(1, "Failed to read byte %d from stream", i);
            r = 1;
            break;
          }
          if (fputc(c, f) == EOF ) {
            error(2, "Failed to write byte %d to file", i);
            r = 2;
            break;
          }
        }
      } else {
        while ((c = str->getChar()) != EOF) {
          if (fputc(c, f) == EOF) {
            error(2, "Failed to write byte to file");
            r = 2;
            break;
          }
        }
      }
      r = 0;
    }
    subtype.free();
  } else if (obj.isDict()) {
    obj.getDict()->lookup("Subtype", &subtype);
    if (subtype.isName("Type0") ||
        subtype.isName("Type1") ||
        subtype.isName("MMType1") ||
        subtype.isName("TrueType") ||
        subtype.isName("CIDFontType0") ||
        subtype.isName("CIDFontType2")
       ) {
      r = dump_font(f, doc, obj.getDict(), NULL);
    } else if (subtype.isName("Type3")) {
      //TODO: See PDF spec, 9.6.5 table 112, Resources.
      //      For Type3 fonts, a font may reference named resources that
      //      are only specified in the resource dictionay of the page on
      //      which the font is used.
      //      To find this dictionay, we would have to know the page. This
      //      would require a setup much like in dump_inline_image().
      //
      //      For now, we just set it to NULL and hope for the best.
      Dict *parentResDict = NULL;
      fprintf(stderr,
               "Warning: Type3 fonts may need page resources to extract. "
               "The lookup of these is not implemented and may "
               "result in errors.\n");
      r = dump_font(f, doc, obj.getDict(), parentResDict);
    } else {
      error(3, "Do not know how to extract dict object.");
      r = 3;
    }
    subtype.free();
  } else {
    error(3, "Do not know how to extract object.");
    r = 3;
  }

  obj.free();
  return r;
}

int dump_inline_image(FILE *f, PDFDoc *doc, int pageNum, int begin)
{
  int r;
  Object contents;
  Object dict;

  if (pageNum < 1 || pageNum > doc->getNumPages()) {
    error(1, "Invalid page number %d", pageNum);
    return 1;
  }
//  Page *page = doc->getPage(pageNum);
  Page *page = doc->getCatalog()->getPage(pageNum);

  page->getContents(&contents);

  Lexer *lexer = new Lexer(doc->getXRef(), &contents);
  contents.free();
  for (int i = 0; i < begin; i++) {
    lexer->skipChar();
  }
  Parser *parser = new Parser(doc->getXRef(), lexer, gFalse);

  Object obj;
  dict.initDict(doc->getXRef());
  parser->getObj(&obj);
  while (!obj.isCmd("ID") && !obj.isEOF()) {
    char *key;
    if (!obj.isName()) {
      error(parser->getPos(), "Inline image dictionary key must be a name object");
      obj.free();
    } else {
      key = copyString(obj.getName());
      obj.free();
      parser->getObj(&obj);
      if (obj.isEOF() || obj.isError()) {
         gfree(key);
         break;
      }
      dict.dictAdd(key, &obj);
    }
    parser->getObj(&obj);
  }
  if (obj.isEOF()) {
    error(parser->getPos(), "End of file in inline image");
  }
  obj.free();

  if (parser->getStream()) {
    Stream *str = new EmbedStream(parser->getStream(), &dict, gFalse, 0);
    str = str->addFilters(&dict);
    GfxResources *res = new GfxResources(
        doc->getXRef(), page->getResourceDict(), NULL);
    r = dump_image(f, str->getDict(), str, res);
    delete res;
    delete str;
  } else {
    error(parser->getPos(), "Parsing inline image failed");
    r = 1;
  }

  delete parser;

  return r;
}

int main(int argc, char *argv[]) {
  int exitCode;
  GBool ok;
  GooString *specString = NULL;
  GooString *fileName = NULL;
  char *extRoot = NULL;
  GooString *ownerPW, *userPW;
  PDFDoc *doc;
  GooList *specs;
  char *separator;
  char *p;

  exitCode = 99;

  // parse args
  ok = parseArgs(argDesc, &argc, argv);

  if (!ok || argc > 4 || printVersion || printHelp) {
    fprintf(stderr, "pdfexref version %s\n", PACKAGE_VERSION);
    fprintf(stderr, "%s\n", popplerCopyright);
    fprintf(stderr, "%s\n", xpdfCopyright);
    if (!printVersion) {
      fprintf(stderr, "\n");
      printUsage("pdfexref",
         "[<spec>,*]<spec> [PDF-file [Extract-file-prefix]]\n"
         "\n"
         "  <spec> = <num>[.<ref>[.<begin>[.<end>]]] | inline.<page>.<begin>\n",
         argDesc);
      fprintf(stderr, "\n");
    }
    goto err0;
  }
  if (argc > 1) specString = new GooString(argv[1]);
  if (argc > 2) fileName = new GooString(argv[2]);
  if (argc == 4) extRoot = argv[3];

  // read config file
  globalParams = new GlobalParams();

  if (quiet) {
    globalParams->setErrQuiet(quiet);
  }

  // open PDF file
  if (ownerPassword[0]) {
    ownerPW = new GooString(ownerPassword);
  } else {
    ownerPW = NULL;
  }
  if (userPassword[0]) {
    userPW = new GooString(userPassword);
  } else {
    userPW = NULL;
  }

  if (fileName == NULL) {
    fileName = new GooString("fd://0");
  }
  if (fileName->cmp("-") == 0) {
    delete fileName;
    fileName = new GooString("fd://0");
  }
  doc = PDFDocFactory().createPDFDoc(*fileName, ownerPW, userPW);
  delete fileName;

  if (userPW) {
    delete userPW;
  }
  if (ownerPW) {
    delete ownerPW;
  }
  if (!doc->isOk()) {
    exitCode = 1;
    goto err1;
  }

  specs = new GooList();
  p = specString->getCString();
  specs->append(p);

  while (*p && (separator = strstr(p, ","))) {
    *separator = '\0';
    p = separator+1;
    specs->append(p);
  }

  exitCode = 0;
  for (int i = 0; i < specs->getLength(); i++) {

    FILE *f;
    char *extFile = NULL;
    int r;

    int num = 0;
    int gen = 0;
    int begin = 0;
    int end = 0;

    p = (char *) specs->get(i);

    if (extRoot != NULL) {
      extFile = (char *) malloc(strlen(extRoot) + 1 + strlen(p) + 4 + 1);
      sprintf(extFile, "%s-%s.bin", extRoot, p);
      f = fopen(extFile, "wb");
      if (!f) {
        error(2, "Couldn't open file '%s'", extFile);
        exitCode = 2;
        continue;
      }
    } else {
      f = stdout;
    }

    if (strncmp(p, "inline.", 7) == 0) {
      p += 7;
      separator = strstr(p, ".");
      if (separator) {
        *separator = '\0';
        int page = atoi(p);
        begin = atoi(separator+1);
        r = dump_inline_image(f, doc, page, begin);
      } else {
        error(3, "Invalid spec");
        r = 3;
      }
    } else {
      num = atoi(p);

      separator = strstr(p, ".");
      if (separator) *separator = '\0';

      if (separator) {

        p = separator+1;

        separator = strstr(p, ".");
        if (separator) *separator = '\0';

        gen = atoi(p);

        if (separator) {

          p = separator+1;

          separator = strstr(p, ".");
          if (separator) *separator = '\0';

          begin = atoi(p);

          if (separator) {

            p = separator+1;

            separator = strstr(p, ".");
            if (separator) *separator = '\0';

            end = atoi(p);
          }
        }
      }

      if (end == 0 || begin < end) {
        r = dump_object(f, doc, num, gen, begin, end);
      } else {
        error(3, "Invalid spec");
        r = 3;
      }
    }

    if (r)
      exitCode = r;

    if (extRoot != NULL) {
      fclose(f);
      if (r) {
        unlink(extFile);
      }
      free(extFile);
    }

  }

  // clean up
  delete specs;
  delete specString;
 err1:
  delete doc;
  delete globalParams;
 err0:

  // check for memory leaks
  Object::memCheck(stderr);
  gMemReport(stderr);

  return exitCode;
}


