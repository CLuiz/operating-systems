/*
 * Complete this file and run rpcgen -MN minifyjpeg.x
 */

struct image {
	opaque buffer<>;
};

/*
 * RPC service - downsize an image to a lower resolution
 */
program MINIFIY_PROGRAM {
	version MINIFY_VERSION {
		image MINIFY_IMAGE(image) = 1;
	} = 1;
} = 0x31230000;
