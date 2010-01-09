//========================================================================
//
// pdftocairo.cc
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
// Copyright (C) 2009 Adrian Johnson <ajohnson@redneon.com>
//
// To see a description of the changes please see the Changelog file that
// came with your tarball or type make ChangeLog if you are building from git
//
//========================================================================

#include "config.h"
#include <poppler-config.h>
#include <stdio.h>
#include <math.h>
#include <cairo.h>
#include <cairo-ps.h>
#include <cairo-pdf.h>
#include <cairo-svg.h>
#include "parseargs.h"
#include "goo/gmem.h"
#include "goo/GooString.h"
#include "GlobalParams.h"
#include "Object.h"
#include "PDFDoc.h"
#include "CairoOutputDev.h"

#define OUT_FILE_SZ 512

static int firstPage = 1;
static int lastPage = 0;
static double resolution = 0.0;
static double x_resolution = 150.0;
static double y_resolution = 150.0;
static int scaleTo = 0;
static int x_scaleTo = 0;
static int y_scaleTo = 0;
static int x = 0;
static int y = 0;
static int w = 0;
static int h = 0;
static int sz = 0;
static GBool useCropBox = gFalse;
static GBool png = gFalse;
static GBool ps = gFalse;
static GBool pdf = gFalse;
static GBool svg = gFalse;
static char ownerPassword[33] = "";
static char userPassword[33] = "";
static GBool quiet = gFalse;
static GBool printVersion = gFalse;
static GBool printHelp = gFalse;

static const ArgDesc argDesc[] = {
  {"-f",      argInt,      &firstPage,     0,
   "first page to print"},
  {"-l",      argInt,      &lastPage,      0,
   "last page to print"},

  {"-r",      argFP,       &resolution,    0,
   "resolution, in DPI (default is 150)"},
  {"-rx",      argFP,       &x_resolution,    0,
   "X resolution, in DPI (default is 150)"},
  {"-ry",      argFP,       &y_resolution,    0,
   "Y resolution, in DPI (default is 150)"},
  {"-scale-to",      argInt,       &scaleTo,    0,
   "scales each page to fit within scale-to*scale-to pixel box"},
  {"-scale-to-x",      argInt,       &x_scaleTo,    0,
   "scales each page horizontally to fit in scale-to-x pixels"},
  {"-scale-to-y",      argInt,       &y_scaleTo,    0,
   "scales each page vertically to fit in scale-to-y pixels"},

  {"-x",      argInt,      &x,             0,
   "x-coordinate of the crop area top left corner"},
  {"-y",      argInt,      &y,             0,
   "y-coordinate of the crop area top left corner"},
  {"-W",      argInt,      &w,             0,
   "width of crop area in pixels (default is 0)"},
  {"-H",      argInt,      &h,             0,
   "height of crop area in pixels (default is 0)"},
  {"-sz",     argInt,      &sz,            0,
   "size of crop square in pixels (sets W and H)"},
  {"-cropbox",argFlag,     &useCropBox,    0,
   "use the crop box rather than media box"},

  {"-png",    argFlag,     &png,           0,
   "generate a PNG file"},
  {"-ps",   argFlag,     &ps,          0,
   "generate PostScript file"},
  {"-pdf",   argFlag,     &pdf,          0,
   "generate a PDF file"},
  {"-svg",   argFlag,     &svg,          0,
   "generate a SVG file"},


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


cairo_status_t write_to_stdout(void *closure, const unsigned char *data, unsigned int length)
{
  int result = fwrite(data, 1, length, stdout);
  
  return (result == length) ? CAIRO_STATUS_SUCCESS : CAIRO_STATUS_WRITE_ERROR;
}

static char *format_output_filename(char *outRoot,
           int pg_num_len, int pg)
{
  char *outFile = (char*)malloc(OUT_FILE_SZ);

  snprintf(outFile, OUT_FILE_SZ, "%.*s-%0*d",
    OUT_FILE_SZ - 32, outRoot, pg_num_len, pg);
  
  if (png) {
    strcat(outFile, ".png");
  } else if (ps) {
    strcat(outFile, ".ps");
  } else if (pdf) {
    strcat(outFile, ".pdf");
  } else if (svg) {
    strcat(outFile, ".svg");
  }
  
  return outFile;
}

static cairo_surface_t *start_page(char *outFile, int w, int h,
		       double x_res, double y_res, int rotate)
{
  cairo_surface_t *surface;
  
  if (png) {
    surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w*x_res/72.0, h*y_res/72.0);
  } else if (outFile != NULL) {
    if (ps) {
      surface = cairo_ps_surface_create(outFile, w, h);
    } else if (pdf) {
      surface = cairo_pdf_surface_create(outFile, w, h);
    } else if (svg) {
      surface = cairo_svg_surface_create(outFile, w, h);
    }
  } else {
    if (ps) {
      surface = cairo_ps_surface_create_for_stream(&write_to_stdout, NULL, w, h);
    } else if (pdf) {
      surface = cairo_pdf_surface_create_for_stream(&write_to_stdout, NULL, w, h);
    } else if (svg) {
      surface = cairo_svg_surface_create_for_stream(&write_to_stdout, NULL, w, h);
    }
  }
  
  return surface;
}

static void end_page(cairo_surface_t *surface, char *outFile)
{
  if (png) {
    if (outFile != NULL) {
      cairo_surface_write_to_png(surface, outFile);
    } else {
      cairo_surface_write_to_png_stream(surface, &write_to_stdout, NULL);
    }
  } else if (ps || pdf || svg) {
    cairo_surface_show_page(surface);
  }
}

static int render_page(CairoOutputDev *output_dev, PDFDoc *doc,
		       cairo_surface_t *surface,
		       GBool printing, int pg,
		       int x, int y, int w, int h,
		       double pg_w, double pg_h,
		       double x_res, double y_res)
{
  cairo_t *cr;
  cairo_status_t status;
  TextPage *text = NULL;

  if (w == 0) w = (int)ceil(pg_w);
  if (h == 0) h = (int)ceil(pg_h);
  w = (x+w > pg_w ? (int)ceil(pg_w-x) : w);
  h = (y+h > pg_h ? (int)ceil(pg_h-y) : h);

  cr = cairo_create (surface);
  cairo_save (cr);
  output_dev->setCairo (cr);
  output_dev->setPrinting (printing);

  if (!printing)
    cairo_scale (cr, x_res/72.0, y_res/72.0);

  text = new TextPage(gFalse);
  if (!printing)
    output_dev->setTextPage (text);

  /* NOTE: instead of passing -1 we should/could use cairo_clip_extents()
   * to get a bounding box */
  cairo_save (cr);
  doc->displayPageSlice(output_dev, pg, /* page */
			72.0, 72.0, 0,
			gFalse, /* useMediaBox */
			!useCropBox, /* Crop */
			printing,
			x, y, w, h);
  cairo_restore(cr);

  output_dev->setCairo(NULL);
  output_dev->setTextPage(NULL);

  if (!printing) {
    cairo_save(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_DEST_OVER);
    cairo_set_source_rgb(cr, 1, 1, 1);
    cairo_paint(cr);
    cairo_restore(cr);
  }

  status = cairo_status(cr);
  if (status)
    fprintf(stderr, "cairo error: %s\n", cairo_status_to_string (status));
  cairo_destroy (cr);

  if (text != NULL)
    text->decRefCnt();

  return 0;
}

int main(int argc, char *argv[]) {
  PDFDoc *doc;
  GooString *fileName = NULL;
  char *outRoot = NULL;
  char *outFile;
  GooString *ownerPW, *userPW;
  GBool ok;
  int exitCode;
  int pg, pg_num_len;
  double pg_w, pg_h, tmp;
  char *p;
  CairoOutputDev *output_dev;
  cairo_surface_t *surface;
  cairo_status_t status;
  GBool printing;

  exitCode = 99;

  // parse args
  ok = parseArgs(argDesc, &argc, argv);

  if ( resolution != 0.0 &&
       (x_resolution == 150.0 ||
        y_resolution == 150.0)) {
    x_resolution = resolution;
    y_resolution = resolution;
  }
  if (!ok || argc < 2 || argc > 3 || printVersion || printHelp) {
    fprintf(stderr, "pdftocairo version %s\n", PACKAGE_VERSION);
    fprintf(stderr, "%s\n", popplerCopyright);
    fprintf(stderr, "%s\n", xpdfCopyright);
    if (!printVersion) {
      printUsage("pdftocairo", "PDF-file [output-file]", argDesc);
    }
    goto err0;
  }

  if (!png && !ps && !pdf && !svg) {
    fprintf(stderr, "One of -png, -ps, -pdf or -svg must be specified\n");
    goto err0;
  }

  if (argc > 1) fileName = new GooString(argv[1]);
  if (argc == 3) {
    outRoot = strdup(argv[2]);

    p = strrchr(outRoot, '.');
    if (p)
      *p = 0;
  }

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
  doc = new PDFDoc(fileName, ownerPW, userPW);
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

  // get page range
  if (firstPage < 1)
    firstPage = 1;
  if (lastPage < 1 || lastPage > doc->getNumPages())
    lastPage = doc->getNumPages();

  if (sz != 0)
    w = h = sz;

  output_dev = new CairoOutputDev();
  output_dev->startDoc(doc->getXRef(), doc->getCatalog());

  pg_num_len = (int)ceil(log((double)doc->getNumPages()) / log((double)10));
  for (pg = firstPage; pg <= lastPage; ++pg) {
    if (useCropBox) {
      pg_w = doc->getPageCropWidth(pg);
      pg_h = doc->getPageCropHeight(pg);
    } else {
      pg_w = doc->getPageMediaWidth(pg);
      pg_h = doc->getPageMediaHeight(pg);
    }

    if (scaleTo != 0) {
      resolution = (72.0 * scaleTo) / (pg_w > pg_h ? pg_w : pg_h);
      x_resolution = y_resolution = resolution;
    } else {
      if (x_scaleTo != 0) {
        x_resolution = (72.0 * x_scaleTo) / pg_w;
      }
      if (y_scaleTo != 0) {
        y_resolution = (72.0 * y_scaleTo) / pg_h;
      }
    }
    
    // Enable printing mode for all output types except PNG
    printing = (png) ? gFalse : gTrue;
    
    // Determine filename (adds page number and correct format)
    if (outRoot != NULL) outFile = format_output_filename(outRoot, pg_num_len, pg);
    else outFile = NULL; // NULL means stdout

    surface = start_page(outFile, pg_w, pg_h, x_resolution, y_resolution, doc->getPageRotate(pg));
    render_page(output_dev, doc, surface, printing, pg,
		x, y, w, h, pg_w, pg_h, x_resolution, y_resolution);
    end_page(surface, outFile);
    
    if (outFile != NULL) free(outFile);
  }
  cairo_surface_finish(surface);
  status = cairo_surface_status(surface);
  if (status)
    fprintf(stderr, "cairo error: %s\n", cairo_status_to_string(status));
  cairo_surface_destroy(surface);
  delete output_dev;

  exitCode = 0;

  // clean up
 err1:
  delete doc;
  delete globalParams;
 err0:

  // check for memory leaks
  Object::memCheck(stderr);
  gMemReport(stderr);

  return exitCode;
}
