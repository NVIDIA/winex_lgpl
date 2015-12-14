/* g++ -Wall -pedantic png2c.C -o png2c -lpng */

#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <endian.h>

#include <png.h>

#define TRUE 1
#define FALSE 0

/* Load a png from a stdio FILE.
 *
 * Grayscale and paletted pngs are converted to RGBA.
 * There will be 8 bits per component, in host order.
 * There will be 4 components in RGBA order.
 * Rows have pitch that is a multiple of 4. This (and all other layout
 * parameters) corresponds to OpenGL's glPixelStore defaults.
 *
 * INF - specifies the file to read from. On return, the seek pointer and
 *	 error state are unspecified.
 * WIDTH - width of the image
 * HEIGHT - height of the image
 * NUM_COMPONENTS - number of "colour" components.
 *
 * The return value is a pointer to the image, allocated with malloc.
 * (The caller owns this pointer.)
 * If the read fails, the return value is NULL and all of WIDTH, HEIGHT,
 * COMPONENT_DEPTH and NUM_COMPONENTS are unchanged.
 */
void *read_png(FILE* inf, unsigned int* width, unsigned int* height,
	       unsigned int* num_components)
{
    png_uint_32 _width, _height;
    int bit_depth, color_type, components;
    int alpha;

    png_structp png_ptr;
    png_infop info_ptr;

    png_bytep *row_pointers = NULL;
    png_bytep image_data = NULL;

    png_uint_32 bpp, pitch;
    png_uint_32 i;

    png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr) return NULL;

    info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr)
    {
	png_destroy_read_struct(&png_ptr, &info_ptr, NULL);

	return NULL;
    }

    if (setjmp(png_jmpbuf(png_ptr)))
	goto error_exit;

    png_init_io(png_ptr, inf);

    png_read_info(png_ptr, info_ptr);

    png_get_IHDR(png_ptr, info_ptr, &_width, &_height, &bit_depth, &color_type,
		 NULL, NULL, NULL);

    alpha = color_type & PNG_COLOR_MASK_ALPHA;
    if (!alpha) png_set_filler(png_ptr, 0xff, PNG_FILLER_AFTER);

    if (color_type == PNG_COLOR_TYPE_PALETTE)
    {
	png_set_palette_to_rgb(png_ptr);
    }
    else if (!(color_type == PNG_COLOR_MASK_COLOR))
    {
	png_set_gray_to_rgb(png_ptr);
    }

    if (bit_depth < 8)
	png_set_expand(png_ptr);
    else if (bit_depth == 16)
	png_set_strip_16(png_ptr);

    bit_depth = 8;

    if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
    {
	png_set_tRNS_to_alpha(png_ptr);
	alpha = TRUE;
    }

    switch (color_type)
    {
    case PNG_COLOR_TYPE_GRAY:
    case PNG_COLOR_TYPE_GRAY_ALPHA:
	components = 1;
	break;

    case PNG_COLOR_TYPE_PALETTE:
    case PNG_COLOR_TYPE_RGB:
    case PNG_COLOR_TYPE_RGB_ALPHA:
	components = 3;
	break;
    }

    if (alpha) components++;

#if LITTLE_ENDIAN
    if (bit_depth == 16)
	png_set_swap(png_ptr);
#endif

    /* allocate the image */

    /* compute BYTES per pixel. we don't have any padding */
    bpp = (bit_depth / 8) * components;

    /* compute pitch in bytes. make it a multiple of 4. */
    pitch = (_width * bpp + 3) & ~3;

    image_data = static_cast<png_bytep>(malloc(pitch * _height));
    if (!image_data) goto error_exit;

    row_pointers = static_cast<png_bytep *>(malloc(sizeof(png_bytep) * _height));
    if (!row_pointers) goto error_exit;

    for (i=0; i < _height; i++)
	row_pointers[i] = image_data + pitch * i;

    png_read_image(png_ptr, row_pointers);

    free(row_pointers);

    png_read_end(png_ptr, info_ptr);

    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);

    *width = _width;
    *height = _height;
    *num_components = components;

    return image_data;

error_exit:
    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);

    if (row_pointers != NULL) free(row_pointers);
    if (image_data != NULL) free(image_data);

    return NULL;
}

int main(int argc, char *argv[])
{
    if (argc != 4)
    {
	printf("usage:\n\tpng2c symbol input.png output.c\n");
	return 0;
    }

    FILE* inf = fopen(argv[2], "r");

    unsigned int width, height, num_components;
    unsigned char* image = reinterpret_cast<unsigned char *>(read_png(inf, &width, &height, &num_components));
    fclose(inf);

    if (width != height)
    {
	fprintf(stderr, "image is not square\n");
	return EXIT_FAILURE;
    }

    FILE* outf = fopen(argv[3], "w");

    fprintf(outf, "static const unsigned char %s[] = {\n", argv[1]);

    /* compute BYTES per pixel. we don't have any padding */
    unsigned int pitch = (width * num_components + 3) & ~3;

    for (unsigned int i=0; i < height; i++)
    {
	for (unsigned int j=0; j < width; j++)
	{
	    fprintf(outf, "0x%x,0x%x,0x%x,0x%x,",
		    (unsigned int)image[i*pitch + j*num_components + 0],
		    (unsigned int)image[i*pitch + j*num_components + 1],
		    (unsigned int)image[i*pitch + j*num_components + 2],
		    (unsigned int)image[i*pitch + j*num_components + 3]);
	}
	fprintf(outf, "\n");
    }

    fprintf(outf, "};\n");

    fprintf(outf, "static const unsigned int %s_size = %u;\n", argv[1], width);

    fclose(outf);
}
