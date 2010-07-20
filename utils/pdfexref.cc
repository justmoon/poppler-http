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
#include "PDFDoc.h"
#include "PDFDocFactory.h"
#include "Extractor.h"
//#include "Gfx.h"
//#include "GfxFont.h"
//#include "GfxState.h"

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
  Extractor *extractor;

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
         "  <spec> = <num>[.<ref>[.<begin>[.<end>]]] |\n"
         "           inline.<page>.<begin> |\n"
         "           font.<page>.<num>[.<ref>]\n",
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
    extractor = new Extractor(f);

    if (strncmp(p, "inline.", 7) == 0) {
      p += 7;
      separator = strstr(p, ".");
      if (separator) {
        *separator = '\0';
        int page = atoi(p);
        begin = atoi(separator+1);
        r = extractor->extractInlineImage(doc, page, begin);
      } else {
        error(3, "Invalid spec");
        r = 3;
      }
    } else if (strncmp(p, "font.", 5) == 0) {
      p += 5;
      separator = strstr(p, ".");
      if (separator) {
        *separator = '\0';
        int page = atoi(p);

        p = separator+1;

        separator = strstr(p, ".");
        if (separator) {
          *separator = '\0';
        }
        int num = atoi(p);
        int gen = 0;
        if (separator) {
           gen = atoi(separator+1);
        }
        r = extractor->extractFont(doc, page, num, gen);
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
        r = extractor->extract(doc, num, gen, begin, end);
      } else {
        error(3, "Invalid spec");
        r = 3;
      }
    }
    delete extractor;

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


