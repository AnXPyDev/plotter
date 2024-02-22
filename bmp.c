#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <malloc.h>
#include <time.h>
#include <stdint.h>

#define BMP_HEADER_SIZE 0x36
#define BMP_INFO_HEADER_SIZE 40
#define BMP_BIT_DEPTH 24

struct BMPHeader {
	char data[BMP_HEADER_SIZE];
};

struct BMPHeaderProperty {
	const char *name;
	size_t offset;
	size_t size;
};

const struct BMPHeaderProperty BMP_Signature_Property = { "Signature", 0x0, 2 };
const struct BMPHeaderProperty BMP_FileSize_Property = { "File Size", 0x2, 4 };
const struct BMPHeaderProperty BMP_DataOffset_Property = { "Data Offset", 0xA, 4 };
const struct BMPHeaderProperty BMP_Width_Property = { "Image Width", 0x12, 4 };
const struct BMPHeaderProperty BMP_Height_Property = { "Image Height", 0x16, 4 };
const struct BMPHeaderProperty BMP_BitDepth_Property = { "Bit Depth", 0x1C, 2 };
const struct BMPHeaderProperty BMP_InfoHeaderSize_Property = { "Info Header Size", 0xE, 4 };
const struct BMPHeaderProperty BMP_Planes_Property = { "Number of Planes", 0x1A, 2 };


void BMP_load_header(struct BMPHeader *this, FILE *instream) {
	fread(this->data, 1, BMP_HEADER_SIZE, instream);
}

long int BMP_get_header_property(const struct BMPHeader *this, const struct BMPHeaderProperty *property) {
	long int val = 0;
	memcpy(&val, this->data + property->offset, property->size);
	return val;
}

void BMP_set_header_property(struct BMPHeader *this, const struct BMPHeaderProperty *property, long int value) {
	memcpy(this->data + property->offset, &value, property->size);
}

void BMP_clear_header(struct BMPHeader *this) {
	memset(this->data, 0, BMP_HEADER_SIZE);
}

void BMP_write_header(const struct BMPHeader *this, FILE *outstream) {
	fwrite(this->data, 1, BMP_HEADER_SIZE, outstream);
}

void BMP_print_property(const struct BMPHeader *header, const struct BMPHeaderProperty *property, FILE *outstream) {
	long int val = BMP_get_header_property(header, property);
	char *bytes = (char*)&val;
	fprintf(outstream, "%s: %ld (%02x%02x%02x%02x)\n", property->name, val, bytes[0], bytes[1], bytes[2], bytes[3]);
}

typedef struct {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} BMP_color;

void BMP_create(FILE *out, int w, int h, BMP_color *framebuffer) {
	const char signature[2] = "BM";
	const int data_offset = 0x36;
	const int pixel_count = w * h;

	const int file_size = (pixel_count * BMP_BIT_DEPTH) / 8 + BMP_HEADER_SIZE;
	struct BMPHeader header;

	BMP_clear_header(&header);

	BMP_set_header_property(&header, &BMP_Signature_Property, *(long int*)signature);
	BMP_set_header_property(&header, &BMP_FileSize_Property, file_size);
	BMP_set_header_property(&header, &BMP_DataOffset_Property, data_offset);
	BMP_set_header_property(&header, &BMP_Width_Property, w);
	BMP_set_header_property(&header, &BMP_Height_Property, h);
	BMP_set_header_property(&header, &BMP_BitDepth_Property, BMP_BIT_DEPTH);
	BMP_set_header_property(&header, &BMP_InfoHeaderSize_Property, BMP_INFO_HEADER_SIZE);
	BMP_set_header_property(&header, &BMP_Planes_Property, 1);

	BMP_write_header(&header, out);

    fwrite(framebuffer, sizeof(BMP_color), w * h, out);
}