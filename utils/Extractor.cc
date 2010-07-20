//========================================================================
//
// Extractor.cc
//
// This file is licensed under the GPLv2 or later
//
// Copyright (C) 2010 Hib Eris <hib@hiberis.nl>
//
//------------------------------------------------------------------------

#include "config.h"
#include "goo/GooString.h"
#include "goo/GooList.h"
#include "GlobalParams.h"
#include "Object.h"
#include "Lexer.h"
#include "Parser.h"
#include "PDFDoc.h"
#include "DCTStream.h"
#include "Gfx.h"
#include "GfxFont.h"
#include "GfxState.h"
#include "PSOutputDev.h"
#include "Extractor.h"
#include "ErrorCodes.h"

Extractor::Extractor(FILE *fA)
{
  f = fA;
}

GfxImageColorMap *
Extractor::getColormap(Stream *str, GfxResources *res)
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


int Extractor::extractImage(Dict *dict, Stream *str, GfxResources *res)
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

    GfxImageColorMap *colorMap = getColormap(str, res);
    if (!colorMap) {
      error(str->getPos(), "Invalid colormap");
      return errDamaged;
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
  return errNone;
}

int Extractor::extractInlineImage(PDFDoc *doc, int pageNum, int begin)
{
  int r;
  Object contents;
  Object dict;

  if (pageNum < 1 || pageNum > doc->getNumPages()) {
    error(-1, "Invalid page number %d", pageNum);
    return errBadPageNum;
  }
  Page *page = doc->getPage(pageNum);

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
      error(parser->getPos(),
        "Inline image dictionary key must be a name object");
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
    r = extractImage(str->getDict(), str, res);
    delete res;
    delete str;
  } else {
    error(parser->getPos(), "Parsing inline image failed");
    r = errDamaged;
  }

  delete parser;

  return r;
}

static void outputToFile(void *stream, char *data, int len) {
  fwrite(data, 1, len, (FILE *)stream);
}

int Extractor::extractFontDict(PDFDoc *doc, Dict *dict, Dict *parentResDict)
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
  psOut = new PSOutputDev(&outputToFile, (void *) f, title, doc,
      doc->getXRef(), doc->getCatalog(), firstPage, lastPage, mode,
      paperWidth, paperHeight, duplex, imgLLX, imgLLY, imgURX, imgURY,
      forceRasterize, manualCrtl);
  if (!psOut->isOk()) {
    delete psOut;
    error(-1, "Failed to create ps output device");
    return errFileIO;
  }

  char *dummyTag = "dummy";
  Ref dummyRef;
  dummyRef.num = 0;
  dummyRef.gen = 0;
  font = GfxFont::makeFont(doc->getXRef(), dummyTag, dummyRef, dict);
  psOut->setupFont(font, parentResDict);
  font->decRefCnt();

  delete psOut;

  return errNone;
}

int Extractor::extractFont(PDFDoc *doc, int pageNum, int num, int gen)
{
  int r;

  if (pageNum < 1 || pageNum > doc->getNumPages()) {
    error(-1, "Invalid page number %d", pageNum);
    return errBadPageNum;
  }
  Page *page = doc->getPage(pageNum);

  Object obj;
  doc->getXRef()->fetch(num, gen, &obj);
  if (obj.isDict("Font")) {
    r = extractFontDict(doc, obj.getDict(), page->getResourceDict());
  } else {
    error(-1, "Object is not a font dictionary");
    r = errDamaged;
  }
  obj.free();

  return r;
}

int Extractor::extract(PDFDoc *doc, int num, int gen, int begin, int end)
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
    r = errNone;
  } else if (obj.isInt()) {
    fprintf (f, "%d\n", obj.getInt());
    r = errNone;
  } else if (obj.isReal()) {
    fprintf (f, "%f\n", obj.getReal());
    r = errNone;
  } else if (obj.isString()) {
    fprintf (f, "%s\n", obj.getString()->getCString());
    r = errNone;
  } else if (obj.isStream()) {
    dict = obj.streamGetDict();
    str = obj.getStream();
    dict->lookup("Subtype", &subtype);
    if (subtype.isName("Image")) {
      r = extractImage(dict, str, NULL);
    } else {
      int c;
      str->reset();
      if (begin > 0) {
        for (int i = 0; i < begin; i++) {
          if (str->getChar() == EOF) {
            error(-1, "Failed to read byte %d from stream", i);
            r = errDamaged;
            break;
          }
        }
      }
      if (end > 0) {
        for (int i = begin; i < end; i++) {
          if ((c = str->getChar()) == EOF) {
            error(-1, "Failed to read byte %d from stream", i);
            r = errDamaged;
            break;
          }
          if (fputc(c, f) == EOF ) {
            error(-1, "Failed to write byte %d to file", i);
            r = errFileIO;
            break;
          }
        }
      } else {
        while ((c = str->getChar()) != EOF) {
          if (fputc(c, f) == EOF) {
            error(-1, "Failed to write byte to file");
            r = errFileIO;
            break;
          }
        }
      }
      r = errNone;
    }
    subtype.free();
  } else if (obj.isDict()) {
    obj.getDict()->lookup("Subtype", &subtype);
    if (subtype.isName("Type3")) {
      //Note: See PDF spec, 9.6.5 table 112, Resources.
      //      For Type3 fonts, a font may reference named resources that
      //      are only specified in the resource dictionay of the page on
      //      which the font is used.
      //      To find this dictionay, we would have to know the page.
      //
      //      We print an error and continue anyway.
      error(-1,"No page resources available when extracting Type3 font.");
    }
    if (subtype.isName("Type0") ||
        subtype.isName("Type1") ||
        subtype.isName("MMType1") ||
        subtype.isName("TrueType") ||
        subtype.isName("CIDFontType0") ||
        subtype.isName("CIDFontType2") ||
        subtype.isName("Type3")
       ) {
      r = extractFontDict(doc, obj.getDict(), NULL);
    } else {
      error(-1, "Do not know how to extract dict object.");
      r = errDamaged;
    }
    subtype.free();
  } else {
    error(-1, "Do not know how to extract object.");
    r = errDamaged;
  }

  obj.free();
  return r;
}


