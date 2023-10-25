#include "ra_internal.h"
#include "opt/png/png.h"
#include "opt/fs/fs.h"

/* Autoscreencap for one file, main entry point.
 */
 
int ra_autoscreencap(uint32_t gameid,const char *rompath) {
  int status=-1;
  void *serial=0;
  int serialc=file_read(&serial,rompath);
  if (serialc<0) return -1;
  
  struct png_image *image=png_decode(serial,serialc);
  free(serial);
  if (!image) return -1;
  
  /* A standard Pico-8 image is 160x205, with the interesting part at (16,24,128,128).
   */
  if ((image->w==160)&&(image->h==205)) {
    struct png_image *crop=png_image_reformat(image,16,24,128,128,8,2,0);
    if (crop) {
      serial=0;
      if ((serialc=png_encode(&serial,crop))>=0) {
        char *blobpath=db_blob_compose_path(ra.db,gameid,"scap",4,".png",4);
        if (blobpath) {
          if (file_write(blobpath,serial,serialc)>=0) {
            status=0;
            db_invalidate_blobs(ra.db);
          }
          free(blobpath);
        }
        free(serial);
      }
      png_image_del(crop);
    }
  }
  
  png_image_del(image);
  return status;
}
