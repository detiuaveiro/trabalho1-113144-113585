/// image8bit - A simple image processing module.
///
/// This module is part of a programming project
/// for the course AED, DETI / UA.PT
///
/// You may freely use and modify this code, at your own risk,
/// as long as you give proper credit to the original and subsequent authors.
///
/// João Manuel Rodrigues <jmr@ua.pt>
/// 2013, 2023

// Student authors (fill in below):
// NMec:113144  Name:João Viegas
// NMec:113585  Name:Henrique de Barbosa Mendonça Oliveira
// 
// 
// 
// Date:
//

#include "image8bit.h"

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include "instrumentation.h"
// The data structure
//
// An image is stored in a structure containing 3 fields:
// Two integers store the image width and height.
// The other field is a pointer to an array that stores the 8-bit gray
// level of each pixel in the image.  The pixel array is one-dimensional
// and corresponds to a "raster scan" of the image from left to right,
// top to bottom.
// For example, in a 100-pixel wide image (img->width == 100),
//   pixel position (x,y) = (33,0) is stored in img->pixel[33];
//   pixel position (x,y) = (22,1) is stored in img->pixel[122].
// 
// Clients should use images only through variables of type Image,
// which are pointers to the image structure, and should not access the
// structure fields directly.

// Maximum value you can store in a pixel (maximum maxval accepted)
const uint8 PixMax = 255;

// Internal structure for storing 8-bit graymap images
struct image {
  int width;
  int height;
  int maxval;   // maximum gray value (pixels with maxval are pure WHITE)
  uint8* pixel; // pixel data (a raster scan)
};


// This module follows "design-by-contract" principles.
// Read `Design-by-Contract.md` for more details.

/// Error handling functions

// In this module, only functions dealing with memory allocation or file
// (I/O) operations use defensive techniques.
// 
// When one of these functions fails, it signals this by returning an error
// value such as NULL or 0 (see function documentation), and sets an internal
// variable (errCause) to a string indicating the failure cause.
// The errno global variable thoroughly used in the standard library is
// carefully preserved and propagated, and clients can use it together with
// the ImageErrMsg() function to produce informative error messages.
// The use of the GNU standard library error() function is recommended for
// this purpose.
//
// Additional information:  man 3 errno;  man 3 error;

// Variable to preserve errno temporarily
static int errsave = 0;

// Error cause
static char* errCause;

/// Error cause.
/// After some other module function fails (and returns an error code),
/// calling this function retrieves an appropriate message describing the
/// failure cause.  This may be used together with global variable errno
/// to produce informative error messages (using error(), for instance).
///
/// After a successful operation, the result is not garanteed (it might be
/// the previous error cause).  It is not meant to be used in that situation!
char* ImageErrMsg() { ///
  return errCause;
}


// Defensive programming aids
//
// Proper defensive programming in C, which lacks an exception mechanism,
// generally leads to possibly long chains of function calls, error checking,
// cleanup code, and return statements:
//   if ( funA(x) == errorA ) { return errorX; }
//   if ( funB(x) == errorB ) { cleanupForA(); return errorY; }
//   if ( funC(x) == errorC ) { cleanupForB(); cleanupForA(); return errorZ; }
//
// Understanding such chains is difficult, and writing them is boring, messy
// and error-prone.  Programmers tend to overlook the intricate details,
// and end up producing unsafe and sometimes incorrect programs.
//
// In this module, we try to deal with these chains using a somewhat
// unorthodox technique.  It resorts to a very simple internal function
// (check) that is used to wrap the function calls and error tests, and chain
// them into a long Boolean expression that reflects the success of the entire
// operation:
//   success = 
//   check( funA(x) != error , "MsgFailA" ) &&
//   check( funB(x) != error , "MsgFailB" ) &&
//   check( funC(x) != error , "MsgFailC" ) ;
//   if (!success) {
//     conditionalCleanupCode();
//   }
//   return success;
// 
// When a function fails, the chain is interrupted, thanks to the
// short-circuit && operator, and execution jumps to the cleanup code.
// Meanwhile, check() set errCause to an appropriate message.
// 
// This technique has some legibility issues and is not always applicable,
// but it is quite concise, and concentrates cleanup code in a single place.
// 
// See example utilization in ImageLoad and ImageSave.
//
// (You are not required to use this in your code!)
 
int compLocateSubImage;  // variable to keep track of the comparisons made when LocateSubImage is called.

// Check a condition and set errCause to failmsg in case of failure.
// This may be used to chain a sequence of operations and verify its success.
// Propagates the condition.
// Preserves global errno!
static int check(int condition, const char* failmsg) {
  errCause = (char*)(condition ? "" : failmsg);
  return condition;
}


/// Init Image library.  (Call once!)
/// Currently, simply calibrate instrumentation and set names of counters.
void ImageInit(void) { ///
  InstrCalibrate();
  InstrName[0] = "pixmem";  // InstrCount[0] will count pixel array acesses
  // Name other counters here...
  
}

// Macros to simplify accessing instrumentation counters:
#define PIXMEM InstrCount[0]
// Macro intended for rounding a double to the nearest integer (useful for some operations)
#define ROUND 0.5
// Add more macros here...

// TIP: Search for PIXMEM or InstrCount to see where it is incremented!


/// Image management functions

// Create a new image with the specified width, height, and maximum gray value.
// Parameters:
//   width: The width of the new image.
//   height: The height of the new image.
//   maxval: The maximum gray value (pixels with maxval are pure WHITE).
// Returns:
//   On success, a pointer to the newly created image is returned.
//   On failure (due to memory allocation errors), returns NULL.
Image ImageCreate(int width, int height, uint8 maxval) { ///
  // Preconditions: Ensure that width and height are non-negative, and maxval is within the allowed range.
  assert(width >= 0);
  assert(height >= 0);
  assert(0 < maxval && maxval <= PixMax);

  // Allocate memory for the image structure.
  Image imag = malloc(sizeof(struct image));
  if (imag == NULL) {
    // Return NULL if memory allocation fails for the image structure.
    return NULL;
  }

  // Set the width, height, and maxval fields of the image structure.
  imag->width = width;
  imag->height = height;
  imag->maxval = maxval;

  // Allocate memory for the pixel data.
  imag->pixel = (uint8*)malloc(width * height * sizeof(uint8));
  if (imag->pixel == NULL) {
    // If memory allocation failed for pixel data,
    // free the previously allocated image structure memory.
    free(imag);
    return NULL;
  }

  // Return a pointer to the newly created image.
  return imag;
}


/// Destroy the image pointed to by (*imgp).
///   imgp : address of an Image variable.
/// If (*imgp)==NULL, no operation is performed.
/// Ensures: (*imgp)==NULL.
/// Should never fail, and should preserve global errno/errCause.
void ImageDestroy(Image* imgp) { ///
  assert(imgp != NULL);   // Preconditions: ensure that the pointr is not NULL.
  free((*imgp)->pixel);   // Free the memory occupied by the pixel data.
  (*imgp)->pixel = NULL;  // Set the pixel pointer to NULL to avoid dangling pointers.
  free(*imgp);            // Free the memory occupied by the image structure.
  *imgp = NULL;           // Set the image pointer to NULL to avoid dangling pointers.
}



/// PGM file operations

// See also:
// PGM format specification: http://netpbm.sourceforge.net/doc/pgm.html

// Match and skip 0 or more comment lines in file f.
// Comments start with a # and continue until the end-of-line, inclusive.
// Returns the number of comments skipped.
static int skipComments(FILE* f) {
  char c;
  int i = 0;
  while (fscanf(f, "#%*[^\n]%c", &c) == 1 && c == '\n') {
    i++;
  }
  return i;
}

/// Load a raw PGM file.
/// Only 8 bit PGM files are accepted.
/// On success, a new image is returned.
/// (The caller is responsible for destroying the returned image!)
/// On failure, returns NULL and errno/errCause are set accordingly.
Image ImageLoad(const char* filename) { ///
  int w, h;
  int maxval;
  char c;
  FILE* f = NULL;
  Image img = NULL;

  int success = 
  check( (f = fopen(filename, "rb")) != NULL, "Open failed" ) &&
  // Parse PGM header
  check( fscanf(f, "P%c ", &c) == 1 && c == '5' , "Invalid file format" ) &&
  skipComments(f) >= 0 &&
  check( fscanf(f, "%d ", &w) == 1 && w >= 0 , "Invalid width" ) &&
  skipComments(f) >= 0 &&
  check( fscanf(f, "%d ", &h) == 1 && h >= 0 , "Invalid height" ) &&
  skipComments(f) >= 0 &&
  check( fscanf(f, "%d", &maxval) == 1 && 0 < maxval && maxval <= (int)PixMax , "Invalid maxval" ) &&
  check( fscanf(f, "%c", &c) == 1 && isspace(c) , "Whitespace expected" ) &&
  // Allocate image
  (img = ImageCreate(w, h, (uint8)maxval)) != NULL &&
  // Read pixels
  check( fread(img->pixel, sizeof(uint8), w*h, f) == w*h , "Reading pixels" );
  PIXMEM += (unsigned long)(w*h);  // count pixel memory accesses

  // Cleanup
  if (!success) {
    errsave = errno;
    ImageDestroy(&img);
    errno = errsave;
  }
  if (f != NULL) fclose(f);
  return img;
}

/// Save image to PGM file.
/// On success, returns nonzero.
/// On failure, returns 0, errno/errCause are set appropriately, and
/// a partial and invalid file may be left in the system.
int ImageSave(Image img, const char* filename) { ///
  assert (img != NULL);
  int w = img->width;
  int h = img->height;
  uint8 maxval = img->maxval;
  FILE* f = NULL;

  int success =
  check( (f = fopen(filename, "wb")) != NULL, "Open failed" ) &&
  check( fprintf(f, "P5\n%d %d\n%u\n", w, h, maxval) > 0, "Writing header failed" ) &&
  check( fwrite(img->pixel, sizeof(uint8), w*h, f) == w*h, "Writing pixels failed" ); 
  PIXMEM += (unsigned long)(w*h);  // count pixel memory accesses

  // Cleanup
  if (f != NULL) fclose(f);
  return success;
}


/// Information queries

/// These functions do not modify the image and never fail.

/// Get image width
int ImageWidth(Image img) { ///
  assert (img != NULL);
  return img->width;
}

/// Get image height
int ImageHeight(Image img) { ///
  assert (img != NULL);
  return img->height;
}

/// Auxiliary function to get the total number of pixels in the image.
 int ImageGetSize(Image img){
  assert (img != NULL);
  return img->width*img->height;
}

/// Get image maximum gray level
int ImageMaxval(Image img) { ///
  assert (img != NULL);
  return img->maxval;
}

/// Pixel stats
/// Find the minimum and maximum gray levels in image.
/// On return,
/// *min is set to the minimum gray level in the image,
/// *max is set to the maximum.
void ImageStats(Image img, uint8* min, uint8* max) { ///
  assert(img != NULL); // Ensure that the image pointer is not NULL
  *min = 0; // Initialize the minimum value to 0.
  *max = 0; // Initialize the maximum value to 0.
  
  int count = ImageGetSize(img); // Get the total number of pixels in the image.
  
  // Iterate through each pixel in the image.
  for (size_t i = 0; i < count; i++) {
    // Check if the current pixel value is greater than the curent minimum.
    if ((img->pixel)[i] > *min) {
      *min = img->pixel[i]; // Update the minimum value.
    }
    
    // Check if the current pixel value is greater than the current maximum.
    if ((img->pixel)[i] > *max) {
      *max = img->pixel[i]; // Update the maximum value.
    }
  }
  
  return; // Exit the function
}


/// Check if pixel position (x,y) is inside img.
int ImageValidPos(Image img, int x, int y) { ///
  assert (img != NULL);
  return (0 <= x && x < img->width) && (0 <= y && y < img->height);
}

/// Check if rectangular area (x,y,w,h) is completely inside img.
int ImageValidRect(Image img, int x, int y, int w, int h) { ///
  assert (img != NULL);
  // Check if the starting position (lower left corner) of the rectangle is valid.
  // Check if the ending position (upper right corner) of the rectangle is valid.
  if (ImageValidPos(img,x,y) && ImageValidPos(img,w+x-1,h+y-1)) return 1;
  // Where the lower left corner is the position given (x,y)
  // and the upper right corner is the position given + the width and height of the rectangle (h and w)
  // minus 1 because indexing starts at 0.
  return 0;
  
}

/// Pixel get & set operations

/// These are the primitive operations to access and modify a single pixel
/// in the image.
/// These are very simple, but fundamental operations, which may be used to 
/// implement more complex operations.

// Transform (x, y) coords into linear pixel index.
// This internal function is used in ImageGetPixel / ImageSetPixel. 
// The returned index must satisfy (0 <= index < img->width*img->height)
static inline int G(Image img, int x, int y) {
  // Asserts to guarantee that the image pointer is not NULL and that the position is valid.
  assert(img != NULL);
  assert(0 <= x && x < img->width);
  assert(0 <= y && y < img->height);

  // Calculate the linear index based on (x, y) coordinates, counting from left to right, top to bottom. 
  return y * img->width + x;
}

/// Get the pixel (level) at position (x,y).
uint8 ImageGetPixel(Image img, int x, int y) { ///
  assert (img != NULL);
  assert (ImageValidPos(img, x, y));
  PIXMEM += 1;  // count one pixel access (read)
  return img->pixel[G(img, x, y)];
} 

/// Set the pixel at position (x,y) to new level.
void ImageSetPixel(Image img, int x, int y, uint8 level) { ///
  assert (img != NULL);
  assert (ImageValidPos(img, x, y));
  PIXMEM += 1;  // count one pixel access (store)
  img->pixel[G(img, x, y)] = level;
} 

/// Pixel transformations

/// These functions modify the pixel levels in an image, but do not change
/// pixel positions or image geometry in any way.
/// All of these functions modify the image in-place: no allocation involved.
/// They never fail.


/// Transform image to negative image.
/// This transforms dark pixels to light pixels and vice-versa,
/// resulting in a "photographic negative" effect.
void ImageNegative(Image img) {
  // Assert that the image is not NULL.
  assert(img != NULL);

  // Get the total number of pixels in the image.
  int count = ImageGetSize(img);

  // Iterate through each pixel in the image.
  for (size_t i = 0; i < count; i++) {
    // Transform the pixel level to its negative value by subtracting it from the maximum pixel value.
    img->pixel[i] = img->maxval - img->pixel[i];
  }
}


/// Apply threshold to image.
/// Transform all pixels with level<thr to black (0) and
/// all pixels with level>=thr to white (maxval).
void ImageThreshold(Image img, uint8 thr) {
  // Ensure that the image is not NULL
  assert(img != NULL);

  // Get the total number of pixels in the image.
  int count = ImageGetSize(img);

  // Iterate through each pixel in the image
  for (size_t i = 0; i < count; i++) {
    // Check if the current pixel value is greater than or equal to the threshold.
    if (img->pixel[i] >= thr) {
      // If the pixel is above (or in) the threshold, set the pixel value to the maximum pixel value.
      img->pixel[i] = img->maxval;
    } else {
      // if the pixel is below the threshold, set the pixel value to 0.
      img->pixel[i] = 0;
    }
  }
}




/// Brighten image by a factor.
/// Multiply each pixel level by a factor, but saturate at maxval.
/// This will brighten the image if factor>1.0 and
/// darken the image if factor<1.0.
void ImageBrighten(Image img, double factor) {
  // Assert that the image and factor are not NULL, and the factor is non-negative.
  assert(img != NULL);
  assert(factor >= 0.0);

  // Get the total number of pixels in the image.
  int count = ImageGetSize(img);

  // Iterate through each pixel in the image.
  for (size_t i = 0; i < count; i++) {
    // Multiply the pixel value by the specified brightness factor and round to the nearest integer.
    // adding ROUND (0.5) for fidelity to the original image.
    img->pixel[i] = (uint8)(img->pixel[i] * factor + ROUND);

    // Cap the pixel value at the maximum value if it exceeds the maximum.
    if (img->pixel[i] > img->maxval) {
      img->pixel[i] = img->maxval;
    }
  }
}


/// Geometric transformations

/// These functions apply geometric transformations to an image,
/// returning a new image as a result.
/// 
/// Success and failure are treated as in ImageCreate:
/// On success, a new image is returned.
/// (The caller is responsible for destroying the returned image!)
/// On failure, returns NULL and errno/errCause are set accordingly.

// Implementation hint: 
// Call ImageCreate whenever you need a new image!

/// Rotate an image.
/// Returns a rotated version of the image.
/// The rotation is 90 degrees anti-clockwise.
/// Ensures: The original img is not modified.
/// 
/// On success, a new image is returned.
/// (The caller is responsible for destroying the returned image!)
/// On failure, returns NULL and errno/errCause are set accordingly.
Image ImageRotate(Image img) { ///
  // Assert that the image is not NULL.
  assert (img != NULL);
  // Create an image with the height and width of the original image swapped.
  Image newImg = ImageCreate(img->height, img->width, img->maxval);
  // Check if the image was created successfully.
  if (newImg == NULL){
    return NULL;
  }
  // Iterate through each pixel in the original image
 for (int y = 0; y < ImageHeight(img); y++){
  for (int x = 0; x < ImageWidth(img); x++){
    // In order to rotate the image 90 degrees anti-clockwise, we must set the corresponding pixel (x,y) in img to
    // the corresponding pixel in the new image is the pixel (xRot,yRot) where xRot = y  and 
    // yRot = ImageWidth(img) - x - 1 because the height and width are swapped
    // (-1 in the yRot because indexing starts at 0).
    ImageSetPixel(newImg, y, ImageWidth(img)-x-1, ImageGetPixel(img,x,y));
 }}
  return newImg;
}

/// Mirror an image = flip left-right.
/// Returns a mirrored version of the image.
/// Ensures: The original img is not modified.
/// 
/// On success, a new image is returned.
/// (The caller is responsible for destroying the returned image!)
/// On failure, returns NULL and errno/errCause are set accordingly.
Image ImageMirror(Image img) {
  // Ensure that the input image is not NULL.
  assert(img != NULL);

  // Create a new image with the same width and height.
  Image newImg = ImageCreate(img->height, img->width, img->maxval);

  // Check if the image was created successfully.
  if (newImg == NULL) {
    return NULL;
  }

  // Iterate through each pixel in the original image
  for (int y = 0; y < ImageHeight(img); y++) {
    for (int x = 0; x < ImageWidth(img); x++) {
      // Using auxiliary functions, set the pixel value in the new image by swapping x and y coordinates 
      // (creating a mirroing effect).
      ImageSetPixel(newImg, ImageWidth(img) - x - 1, y, ImageGetPixel(img, x, y));
    }
  }

  // Return the mirrored image.
  return newImg;
}


/// Crop a rectangular subimage from img.
/// The rectangle is specified by the top left corner coords (x, y) and
/// width w and height h.
/// Requires:
///   The rectangle must be inside the original image.
/// Ensures:
///   The original img is not modified.
///   The returned image has width w and height h.
/// 
/// On success, a new image is returned.
/// (The caller is responsible for destroying the returned image!)
/// On failure, returns NULL and errno/errCause are set accordingly.
Image ImageCrop(Image img, int x, int y, int w, int h) {
  // Ensure that the input image is not NULL.
  assert(img != NULL);

  // Ensure that the to-crop rectangle is valid.
  assert(ImageValidRect(img, x, y, w, h));

  // Create a new image with the specified width, height, and maximum pixel value of the original image.
  Image newImg = ImageCreate(h, w, img->maxval);

  // Check if the image was created successfully.
  if (newImg == NULL) {
    return NULL;
  }

  // Iterate through each pixel in the cropping rectangle.
  for (int i = 0; i < h; i++) {
    for (int j = 0; j < w; j++) {
      // Using auxiliary functions, set the pixel value in the new image by copying 
      // from the corresponding position in the original image.
      ImageSetPixel(newImg, j, i, ImageGetPixel(img, j + x, i + y));
    }
  }

  // Return the cropped image.
  return newImg;
}


/// Operations on two images

/// Paste an image into a larger image.
/// Paste img2 into position (x, y) of img1.
/// This modifies img1 in-place: no allocation involved.
/// Requires: img2 must fit inside img1 at position (x, y).
void ImagePaste(Image img1, int x, int y, Image img2) {
  // Assert that the first and second images aren't NULL.
  assert(img1 != NULL);
  assert(img2 != NULL);

  // Assert that the maximum pixel value of the second image does not exceed the maximum pixel value of the first image.
  assert(ImageMaxval(img2) <= ImageMaxval(img1));

  // Assert that the pasting position and size are within the size limits of the first image.
  assert(ImageValidRect(img1, x, y, img2->width, img2->height));

  // Iterate through each pixel in the second image.
  for (int i = 0; i < img2->height; i++) {
    for (int j = 0; j < img2->width; j++) {
      // Set the pixel value in the first image by copying from the corresponding position in the second image
      // (Similarly to how ImageCrop was developed).
      ImageSetPixel(img1, j + x, i + y, ImageGetPixel(img2, j, i));
    }
  }
}

/// Blend an image into a larger image.
/// Blend img2 into position (x, y) of img1.
/// This modifies img1 in-place: no allocation involved.
/// Requires: img2 must fit inside img1 at position (x, y).
/// alpha usually is in [0.0, 1.0], but values outside that interval
/// may provide interesting effects.  Over/underflows should saturate.
void ImageBlend(Image img1, int x, int y, Image img2, double alpha) {
  // Assert that the first image is not NULL.
  assert(img1 != NULL);

  // Assert that the second image is not NULL.
  assert(img2 != NULL);

  // Assert that the blending position and size are within the size limits of the first image.
  assert(ImageValidRect(img1, x, y, img2->width, img2->height));

  // Iterate through each pixel in the blended region.
  for (int i = 0; i < img2->height; i++) {
    for (int j = 0; j < img2->width; j++) {
      // Calculate the blended pixel value using the specified alpha value:
      // blendedPixel = pixel2 * alpha + pixel1 * (1 - alpha) + ROUND (adding ROUND for pixel value fidelity).
      uint8 valPixel = (uint8)(ImageGetPixel(img2, j, i) * alpha +
                               ImageGetPixel(img1, j + x, i + y) * (1 - alpha) + ROUND); 

      // Set the pixel value in the first image at the blended position.
      ImageSetPixel(img1, j + x, i + y, valPixel);
    }
  }
}


/// Compare an image to a subimage of a larger image.
/// Returns 1 (true) if img2 matches subimage of img1 at pos (x, y).
/// Returns 0, otherwise.
int ImageMatchSubImage(Image img1, int x, int y, Image img2) {
  assert(img1 != NULL);  // Assert img1 is not NULL.
  assert(img2 != NULL);  // Assert img2 is not NULL.
  assert(ImageValidPos(img1, x, y));  // Assert the specified position (x, y) is valid in img1.
  assert(ImageValidRect(img1, x, y, img2->width, img2->height));  // Assert the region specified by img2 fits within img1.

  // Loop through each pixel in img2.
  for (int i = 0; i < img2->height; i++) {
    for (int j = 0; j < img2->width; j++) {
      compLocateSubImage++; // Increment the variable after each pixel comparison.

      // Compare corresponding pixels in img1 and img2
      if (ImageGetPixel(img2, j, i) != ImageGetPixel(img1, j + x, i + y)) {
        return 0;  // If any pixel doesn't match, return 0 (false).
      }
    }
  }

  return 1;  // If the function hasn't exited with a return value by now, it means all pixels matched, return 1 (true).
}

// Locate a subimage inside another image.
/// Searches for img2 inside img1.
/// If a match is found, returns 1 and matching position is set in vars (*px, *py).
/// If no match is found, returns 0 and (*px, *py) are left untouched.
int ImageLocateSubImage(Image img1, int* px, int* py, Image img2) {
  assert(img1 != NULL);  // Assert img1 is not NULL.
  assert(img2 != NULL);  // Assert img2 is not NULL.
  compLocateSubImage = 0; // Global variable set to 0 (before calling ImageMatchSubImage, where the comparisons will be made).

  // Iterate over possible positions for img2 within img1 (ie: unnecessary to keep checking for a 3x3 img inside a 5x5 if no match has been made
  // until (2,2) (including  pixel (2,2)), since no 3x3 img can fit inside the remaining pixels (2,3) onwards).
  for (int i = 0; i < ImageHeight(img1) - img2->height + 1; i++) {
    for (int j = 0; j < ImageWidth(img1) - img2->width + 1; j++) {
      // Call ImageMatchSubImage to check if img2 matches the subimage of img1 at position (j, i).
      if (ImageMatchSubImage(img1, j, i, img2) == 1) {
        // if there's a match:
        *px = j;  // Set the matching position in the variable pointed to by px.
        *py = i;  // Set the matching position in the variable pointed to by py.
        printf("Comparacoes: %d\n", compLocateSubImage);
        return 1;  // Return 1 (true) to indicate a match.
      }
    }
  }

  // If no match is found, leave (*px, *py) untouched and return 0 (false).
  return 0;
}

/// Filtering

/// Blur an image by a applying a (2dx+1)x(2dy+1) mean filter.
/// Each pixel is substituted by the mean of the pixels in the rectangle
/// [x-dx, x+dx]x[y-dy, y+dy].
/// The image is changed in-place.
/// This implementation is a two-pass algorithm that uses computes cumulative sums to apply the blur. 
/// We consider this to be the most efficient implementation we could come up with. 
void ImageBlur(Image img, int dx, int dy) {
  assert(dx >= 0);    // Ensure dx is non-negative
  assert(dy >= 0);    // Ensure dy is non-negative

  double start_time, finish_time, exec_time;
  start_time = cpu_time();  // Record the start time for performance measurement

  int i, j;            // Loop variables
  int x, y;            // Pixel position
  int valPixel;        // Pixel value (double to avoid overflow)
  int comparacoes = 0; // Variable to count the number of pixel comparisons
  int height = ImageHeight(img);
  int width = ImageWidth(img);
  int matrixValPixelSUM[height * width];

  // First pass: Compute the cumulative sum over rows
  for (i = 0; i < height; i++) {
    for (j = 0; j < width; j++) {
      valPixel = ImageGetPixel(img, j, i);
      valPixel += j > 0 ? matrixValPixelSUM[G(img, j - 1, i)] : 0;              // Cumulative sum over the previous row
      valPixel += i > 0 ? matrixValPixelSUM[G(img, j, i - 1)] : 0;              // Cumulative sum over the previous column
      valPixel -= i > 0 && j > 0 ? matrixValPixelSUM[G(img, j - 1, i - 1)] : 0; // Remove the overlap to avoid double counting
      comparacoes += 3;
      matrixValPixelSUM[G(img, j, i)] = valPixel; // Store the cumulative sum at the current position
    }
  }

  // Second pass: Compute the weighted average over the specified region
  for (i = 0; i < height; i++) {
    for (j = 0; j < width; j++) {
      x = j + dx < width ? j + dx : width - 1;    // Compute the x-coordinate for the weighted average region
      y = i + dy < height ? i + dy : height - 1;  // Compute the y-coordinate for the weighted average region
      valPixel = matrixValPixelSUM[G(img, x, y)]; // Get the cumulative sum at the target position
      valPixel += j - dx > 0 && i - dy > 0 ? matrixValPixelSUM[G(img, j - dx - 1, i - dy - 1)] : 0; // Remove the overlap from the previous region
      valPixel -= i - dy > 0 ? matrixValPixelSUM[G(img, x, i - dy - 1)] : 0;  // Remove the overlap from the previous row
      valPixel -= j - dx > 0 ? matrixValPixelSUM[G(img, j - dx - 1, y)] : 0;  // Remove the overlap from the previous column
      int h = i - dy > 0 ? i - dy : 0, w = j - dx > 0 ? j - dx : 0;           // Compute the height and width of the region
      comparacoes += 7;                                                       // Increment the number of pixel comparisons
      valPixel = (uint8)((double)valPixel / ((x - w + 1) * (y - h + 1)) + ROUND); // Compute the weighted average and round it
      ImageSetPixel(img, j, i, valPixel); // Set the pixel value in the output image
    }
  }

  finish_time = cpu_time();
  exec_time = finish_time - start_time;
  printf("Tempo: %f\n", exec_time);  // Print the execution time
}



/* LEAST EFFICIENT BLUR EXECUTION 
void ImageBlur(Image img, int dx, int dy) {
  int i,j,a,b,heightMax,widthMax,heightMin,widthMin,Area,comparacoes=0;
  int height = ImageHeight(img);
  int width = ImageWidth(img);
  Image copy = ImageCreate(width,height,ImageMaxval(img));
  double valPixel;
  ImagePaste(copy,0,0,img);
  for (i = 0; i < height; i++){
    for (j = 0; j < width ; j++){
      valPixel = 0;
      for (a = -dy; a <= dy; a++){
        for (b = -dx; b <= dx; b++){
          valPixel+=ImageValidPos(copy,j+b,i+a)?ImageGetPixel(copy,j+b,i+a):0;
          comparacoes++;
        }
      }
      heightMin=i - dy > 0?i-dy:0,
      widthMin =j - dx > 0?j-dx:0;
      heightMax = i+dy < height ? i+dy : height-1;
      widthMax = j+dx < width ? j+dx : width-1;
      comparacoes+=4;
      Area=(widthMax - widthMin +1) * (heightMax - heightMin +1);
      valPixel = (uint8)((double)valPixel / (Area) + ROUND);
      ImageSetPixel(img,j,i,valPixel);     
    }
  }
    printf("Comparações: %d\n",comparacoes);
    ImageDestroy(&copy);  // Destroy the copy sense it's not needed anymore
}*/
//Algures dá erro nos indices do ImageGetPixel
//Na imagem 2 passa do limite de PixMax permitido
