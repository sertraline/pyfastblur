#include <Python.h>
#include <iostream>
#include <fstream>
#include <cmath>
#include <chrono>
#include "png.h"

#define scl source_channel
#define tcl target_channel
#define sigma standard_deviation
#define SIGSIZE 8
#define CHUNK_SIZE 8192

bool validate(FILE* source) {
	// signature storage
	png_byte png_signature[SIGSIZE];

	fread(png_signature, 1, 8, source);

	int is_png = png_sig_cmp(png_signature, 0, SIGSIZE);
	return (is_png == 0);
}

// implementation of http://blog.ivank.net/fastest-gaussian-blur.html
int* boxes_for_gauss(int sigma, int num) {
	auto avg_filter_width = sqrt((12 * sigma * sigma) / num + 1);
	int wfloor = floor(avg_filter_width);
	if (wfloor % 2 == 0) wfloor--;
	int wu = wfloor + 2;

	int const_a = (12 * sigma * sigma - num * wfloor * wfloor - 4 * num * wfloor - 3 * num);
	int const_b = (-4 * wfloor - 4);
	int med_ideal = const_a / const_b;
	int med_round = round(med_ideal);

	int* sizes = new int[num];
	for (int i = 0; i < num; i++) {
		sizes[i] = i < med_round ? wfloor : wu;
	}
	return sizes;
}

void box_blur_horizontal(png_byte* scl, png_byte* tcl, int w, int h, int r) {
	float iarr = 1.f / (r + r + 1);
	for (int i = 0; i < h; i++) {
		int ti = i * w;
		int li = ti;
		int ri = ti + r;

		int first_val = scl[ti];
		int last_val = scl[ti + w - 1];
		int val = (r + 1) * first_val;

		for (int j = 0; j < r; j++) val += scl[ti + j];
		for (int j = 0; j <= r; j++) {
			val += scl[ri++] - first_val;
			tcl[ti++] = round(val * iarr);
		}
		for (int j = (r + 1); j < (w - r); j++) {
			val += scl[ri++] - scl[li++];
			tcl[ti++] = round(val * iarr);
		}
		for (int j = (w - r); j < w; j++) {
			val += last_val - scl[li++];
			tcl[ti++] = round(val * iarr);
		}
	}
}

void box_blur_total(png_byte* scl, png_byte* tcl, int w, int h, int r) {
	float iarr = 1.f / (r + r + 1);
	for (int i = 0; i < w; i++) {
		int ti = i;
		int li = ti;
		int ri = ti + r * w;

		int first_val = scl[ti];
		int last_val = scl[ti + w * (h - 1)];
		int val = (r + 1) * first_val;

		for (int j = 0; j < r; j++) val += scl[ti + j * w];
		for (int j = 0; j <= r; j++) {
			val += scl[ri] - first_val;
			tcl[ti] = round(val * iarr);
		}
		for (int j = (r + 1); j < (h - r); j++) {
			val += scl[ri] - scl[li];
			tcl[ti] = round(val * iarr);
		}
		for (int j = (h - r); j < h; j++) {
			val += last_val - scl[li];
			tcl[ti] = round(val * iarr);
		}
	}
}

void box_blur_full(png_byte* scl, png_byte* tcl, int w, int h, int r) {
	for (int i = 0; i < (w * h); i++) {
		tcl[i] = scl[i];
	}
	box_blur_horizontal(tcl, scl, w, h, r);
	box_blur_total(scl, tcl, w, h, r);
}

void gaussian_blur(png_byte* scl, png_byte* tcl, int w, int h, int r, int sb) {
	auto boxes = boxes_for_gauss(r, 3);
	box_blur_full(scl, tcl, w, h, (boxes[0] - 1) / 2);
	if (sb) {
		box_blur_full(tcl, scl, w, h, (boxes[1] - 1) / 2);
		box_blur_full(scl, tcl, w, h, (boxes[2] - 1) / 2);
	}
}

void process(FILE* source, FILE* target, int radius, int stronger_blur) {
	if (!validate(source)) {
		std::cerr << "Error: not a valid PNG file" << std::endl;
		abort();
	}
	png_structp png_ptr;
	png_infop info_ptr;

	png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (!png_ptr) {
		std::cerr << "Failure on establishing read struct" << std::endl;
		exit(1);
	}
	info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr) {
		std::cerr << "Failure on establishing info struct" << std::endl;
		exit(1);
	}
	// skip signature header
	png_set_sig_bytes(png_ptr, 8);

	png_init_io(png_ptr, source);

	png_uint_32 bitdepth = png_get_bit_depth(png_ptr, info_ptr);
	png_uint_32 color_type = png_get_color_type(png_ptr, info_ptr);

	png_bytepp pixel_data;
	png_read_png(png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, NULL);
	pixel_data = png_get_rows(png_ptr, info_ptr);

	// png_uint_32
	const int width = (int)png_get_image_width(png_ptr, info_ptr);
	const int height = (int)png_get_image_height(png_ptr, info_ptr);
	const int channels = (int)png_get_channels(png_ptr, info_ptr);

	const size_t size = width * height;
	const size_t alphasize = channels == 4 ? size : 0;

	// there has to be a better way, but I don't know it yet
	// allocate channels on the heap
	png_byte* in_channels[4] = { new png_byte[size], new png_byte[size],
								 new png_byte[size], new png_byte[alphasize] };
	png_byte* buffer_channel = new png_byte[size];

	if (setjmp(png_jmpbuf(png_ptr))) {
		png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)0);
		for (int i = 0; i < 4; i++) {
			delete[] in_channels[i];
		}
		delete[] buffer_channel;
		std::cerr << "Failure on processing the image" << std::endl;
		exit(1);
	}

	/* [i][j] [i][j+1] [i][j+2] [i][j+3]
		  R       G        B        A
		  Unpack channels from 2d array provided by libpng
	*/
	int counter = 0;
	for (int i = 0; i < height; i++) {
		for (int j = 0, c = 0; j < width * channels, c < width; j += channels, c++) {
			in_channels[0][counter + c] = pixel_data[i][j];
			in_channels[1][counter + c] = pixel_data[i][j + 1];
			in_channels[2][counter + c] = pixel_data[i][j + 2];
			if (channels == 4) {
				in_channels[3][counter + c] = pixel_data[i][j + 3];
			}
		}
		counter += width;
	}

	auto t1 = std::chrono::high_resolution_clock::now();
	for (int ch = 0; ch < channels; ch++) {
		gaussian_blur(in_channels[ch], buffer_channel, width, height, radius, stronger_blur);
	}
	auto t2 = std::chrono::high_resolution_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
	std::cout << "Blurring took " << duration << " ms\n";

	// pack channels back
	counter = 0;
	for (int i = 0; i < height; i++) {
		for (int j = 0, c = 0; j < width * channels, c < width; j += channels, c++) {
			pixel_data[i][j] = in_channels[0][counter + c];
			pixel_data[i][j + 1] = in_channels[1][counter + c];
			pixel_data[i][j + 2] = in_channels[2][counter + c];
			if (channels == 4) {
				pixel_data[i][j + 3] = in_channels[3][counter + c];
			}
		}
		counter += width;
	}

	// write to file
	png_structp png_wptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (!png_wptr) {
		std::cerr << "Failure on establishing write struct" << std::endl;
		exit(1);
	}

	png_infop info_wptr = png_create_info_struct(png_wptr);
	if (!info_wptr) {
		std::cerr << "Failure on establishing info write struct" << std::endl;
		exit(1);
	}

	if (setjmp(png_jmpbuf(png_wptr))) {
		png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)0);
		png_destroy_write_struct(&png_wptr, &info_wptr);
		std::cerr << "Failure on processing the image";
		abort();
	}

	png_init_io(png_wptr, target);

	png_set_IHDR(
		png_wptr,
		info_wptr,
		width,
		height,
		8,
		png_get_color_type(png_ptr, info_ptr),
		PNG_INTERLACE_NONE,
		PNG_COMPRESSION_TYPE_DEFAULT,
		PNG_FILTER_TYPE_DEFAULT
	);

	png_set_compression_level(png_wptr, 2);
	png_write_info(png_wptr, info_wptr);
	png_write_image(png_wptr, pixel_data);
	png_write_end(png_wptr, NULL);

	for (int i = 0; i < channels; i++) {
		delete[] in_channels[i];
	}
	delete[] buffer_channel;
	png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)0);
	png_destroy_write_struct(&png_wptr, &info_wptr);
}

static PyObject* blur(PyObject* self, PyObject* args)
{
	PyObject* obj;
	PyObject* read_meth;
	PyObject* result = NULL;
	PyObject* read_args;
	int radius;
	int stronger_blur;

	/* Consume a "file-like" object and write bytes to stdout */
	if (!PyArg_ParseTuple(args, "iOi", &radius, &obj, &stronger_blur)) {
		return NULL;
	}

	/* Get the read method of the passed object */
	if ((read_meth = PyObject_GetAttrString(obj, "read")) == NULL) {
		return NULL;
	}

	/* create tmp files */
	std::FILE* tmpf = std::tmpfile();
	std::FILE* tmpf_out = std::tmpfile();

	/* Build the argument list to read() */
	read_args = Py_BuildValue("(i)", CHUNK_SIZE);
	while (1) {
		PyObject* data;
		char* buf;
		Py_ssize_t len;

		/* Call read() */
		if ((data = PyObject_Call(read_meth, read_args, NULL)) == NULL) {
			Py_DECREF(read_meth);
			Py_DECREF(read_args);
			break;
		}

		/* Check for EOF */
		if (PySequence_Length(data) == 0) {
			Py_DECREF(data);
			break;
		}

		/* Extract underlying buffer data */
		PyBytes_AsStringAndSize(data, &buf, &len);

		std::fwrite(buf, 1, len, tmpf);

		Py_DECREF(data);
	}
	std::rewind(tmpf);

	process(tmpf, tmpf_out, radius, stronger_blur);
	fclose(tmpf);

	std::rewind(tmpf_out);
	// build _io.TextIOWrapper for open file descriptor
	PyObject* retbuf = PyFile_FromFd(fileno(tmpf_out), NULL, "rb", -1, NULL, NULL, NULL, 1);

	Py_DECREF(read_meth);
	Py_DECREF(read_args);
	return retbuf;
}

static PyMethodDef module_methods[] = {
	{"blur",  blur, METH_VARARGS,
	 "Blur the image."},
	{NULL, NULL, 0, NULL}
};

static struct PyModuleDef module = {
	PyModuleDef_HEAD_INIT, "pyfastblur", "Apply fast blur to PNG images", -1, module_methods
};

PyMODINIT_FUNC
PyInit_pyfastblur_cpp(void) {
	return PyModule_Create(&module);
}
