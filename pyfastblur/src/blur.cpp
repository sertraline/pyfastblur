#include <Python.h>
#include <iostream>
#include <fstream>
#include <chrono>
#include <cmath>

#include "png.h"
#include "blur.h"

#define SIGSIZE 8
#define CHUNK_SIZE 8192

bool validate(FILE* source) {
	// signature storage
	png_byte png_signature[SIGSIZE];

	fread(png_signature, 1, 8, source);

	int is_png = png_sig_cmp(png_signature, 0, SIGSIZE);
	return (is_png == 0);
}

int process(FILE* source, FILE* target, int radius, int stronger_blur) {
	if (!validate(source)) {
		std::cerr << "Error: not a valid PNG file" << std::endl;
		return NULL;
	}
	png_structp png_ptr;
	png_infop info_ptr;

	png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (!png_ptr) {
		std::cerr << "Failure on establishing read struct" << std::endl;
		return NULL;
	}
	info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr) {
		std::cerr << "Failure on establishing info struct" << std::endl;
		return NULL;
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
	png_byte* in_channels[4] = {
			new png_byte[size], new png_byte[size],
			new png_byte[size], new png_byte[alphasize]
	};
	png_byte* buffer_channel = new png_byte[size];

	if (setjmp(png_jmpbuf(png_ptr))) {
		png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)0);
		for (int i = 0; i < 4; i++) {
			delete[] in_channels[i];
		}
		delete[] buffer_channel;
		std::cerr << "Failure on processing the image" << std::endl;
		return NULL;
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
		png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)0);
		for (int i = 0; i < 4; i++) {
			delete[] in_channels[i];
		}
		delete[] buffer_channel;
		std::cerr << "Failure on establishing write struct" << std::endl;
		return NULL;
	}

	png_infop info_wptr = png_create_info_struct(png_wptr);
	if (!info_wptr) {
		png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)0);
		for (int i = 0; i < 4; i++) {
			delete[] in_channels[i];
		}
		delete[] buffer_channel;
		std::cerr << "Failure on establishing info write struct" << std::endl;
		return NULL;
	}

	if (setjmp(png_jmpbuf(png_wptr))) {
		for (int i = 0; i < 4; i++) {
			delete[] in_channels[i];
		}
		delete[] buffer_channel;
		png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)0);
		png_destroy_write_struct(&png_wptr, &info_wptr);
		std::cerr << "Failure on processing the image";
		return NULL;
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

	for (int i = 0; i < 4; i++) {
		delete[] in_channels[i];
	}
	delete[] buffer_channel;
	png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)0);
	png_destroy_write_struct(&png_wptr, &info_wptr);
	return 1;
}

static PyObject* blur(PyObject* self, PyObject* args)
{
	PyObject* obj;
	PyObject* read_meth;
	PyObject* result = NULL;
	PyObject* read_args;
	int radius;
	int stronger_blur;

	/* Parse input args */
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
	int ret;
	ret = process(tmpf, tmpf_out, radius, stronger_blur);
	if(!ret) {
		std::fclose(tmpf);
		std::fclose(tmpf_out);
		Py_DECREF(read_meth);
		Py_DECREF(read_args);
		return Py_BuildValue("");
	}
	std::fclose(tmpf);

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
