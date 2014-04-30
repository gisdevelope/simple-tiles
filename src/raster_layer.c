#include "raster_layer.h"
#include "gdal_in_mem_warp.h"
#include "util.h"
#include "error.h"
#include <gdal.h>
#include <cpl_error.h>
#include "memory.h"

// Add in an error function.
SIMPLET_ERROR_FUNC(raster_layer_t)

simplet_raster_layer_t*
simplet_raster_layer_new(const char *datastring) {
  simplet_raster_layer_t *layer;
  if (!(layer = malloc(sizeof(*layer))))
    return NULL;

  memset(layer, 0, sizeof(*layer));
  layer->source = simplet_copy_string(datastring);
  layer->type   = SIMPLET_RASTER;
  layer->status = SIMPLET_OK;

  // need some checking here
  simplet_retain((simplet_retainable_t *)layer);

  return layer;
}

// Free a layer object, and associated layers.
void
simplet_raster_layer_free(simplet_raster_layer_t *layer){
  if(simplet_release((simplet_retainable_t *)layer) > 0) return;
  if(layer->error_msg) free(layer->error_msg);
  free(layer->source);
  free(layer);
}

// stackoverflow.com/a/13446094
static simplet_status_t
coords_to_pixels(GDALDatasetH dataset, double points[4], simplet_map_t* map) {
  double gt[6];

  if(GDALGetGeoTransform(dataset, gt) != CE_None)
    return SIMPLET_ERR;

  points[2] = ((map->bounds->nw.x - gt[0]) / gt[1]);
  points[3] = ((map->bounds->nw.y - gt[3]) / gt[5]);
  points[0] = ((map->bounds->se.x - gt[0]) / gt[1]);
  points[1] = ((map->bounds->se.y - gt[3]) / gt[5]);

  return SIMPLET_OK;
}

simplet_status_t
simplet_raster_layer_process(simplet_raster_layer_t *layer, simplet_map_t *map, cairo_t *ctx) {
  // process the map
  int width  = map->width;
  int height = map->height;
  double pixel_bounds[4];

  GDALDatasetH source;
  GDALDatasetH dst_mem;

  source = GDALOpen(layer->source, GA_ReadOnly);
  if(source == NULL)
    return set_error(layer, SIMPLET_GDAL_ERR, "error opening raster source");

  // TODO: allow for many bands
  if(GDALGetRasterCount(source) < 4)
    return set_error(layer, SIMPLET_GDAL_ERR, "raster layer must have 4 bands");

  if(coords_to_pixels(source, pixel_bounds, map) == SIMPLET_ERR)
    return set_error(layer, SIMPLET_GDAL_ERR, "cannot transform dataset");


  char *dst_ptr = malloc(sizeof(char) * width * height * 4);

  const char *format = "GTiff";
  char *target = map->proj;
  dst_mem = GDALWarpCreateOutput(
                      source,
                      dst_ptr,
                      format,
                      target,
                      map->proj,
                      0,
                      NULL,
                      // dfMinX, dfMinY, dfMaxX, dfMaxY
                      pixel_bounds[2], pixel_bounds[1], pixel_bounds[0], pixel_bounds[3],
                      width, height,
                      width, height
                    );

  if (dst_mem == NULL)
    return set_error(layer, SIMPLET_GDAL_ERR, "error creating in-memory raster");


  free(dst_mem);
  return SIMPLET_OK;
}