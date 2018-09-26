#include "RVLDecoder.h"
#include <sstream>
#include <iostream>

RVLDecoder::RVLDecoder() {
	_input = NULL;
}

RVLDecoder::~RVLDecoder() {
	CloseHandle(_inFile);
	delete _input;
}

bool RVLDecoder::InitDecoder(int width, int height, string inputPath) {
	_width = width;
	_height = height;
	_inputPath = inputPath;
	std::wstring stempb = std::wstring(inputPath.begin(), inputPath.end());
	LPCWSTR swb = stempb.c_str();
	_inFile = CreateFileW(swb,GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if(_input == NULL){
		_input = (byte*)malloc(sizeof(short)*width*height);
	}
	return true;
}

void RVLDecoder::ResetDecoder() {
	CloseHandle(_inFile);
	InitDecoder(_width, _height, _inputPath);
}

int RVLDecoder::DecodeVLE()
{
	unsigned int nibble;
	int value = 0, bits = 29;
	do
	{
		if (!_nibblesWritten)
		{
			_word = *_pBuffer++; // load word
			_nibblesWritten = 8;
		}
		nibble = _word & 0xf0000000;
		unsigned int nibblebits = (nibble << 1) >> bits;
		value |= nibblebits;
		_word <<= 4;
		_nibblesWritten--;
		bits -= 3;
	} while (nibble & 0x80000000);
	return value;
}

void RVLDecoder::DecompressRVL(int* output, int numPixels)
{
	DWORD bytesRead;
	ReadFile(_inFile, _sizeBuffer, 4, &bytesRead, NULL);
	if (bytesRead == 0) {
		ResetDecoder();
		ReadFile(_inFile, _sizeBuffer, 4, &bytesRead, NULL);
	}
	int size = (_sizeBuffer[0] << 24) | (_sizeBuffer[1] << 16) | (_sizeBuffer[2] << 8) | (_sizeBuffer[3]);

	ReadFile(_inFile, _input, size, &bytesRead, NULL);

	_buffer = _pBuffer = (int*)_input;
	_nibblesWritten = 0;
	int current, previous = 0;
	int numPixelsToDecode = numPixels;
	while (numPixelsToDecode)
	{
		int zeros = DecodeVLE(); // number of zeros
		numPixelsToDecode -= zeros;
		for (; zeros; zeros--){
 			*output++ = 0;
		}
		int nonzeros = DecodeVLE(); // number of nonzeros
		numPixelsToDecode -= nonzeros;
		for (; nonzeros; nonzeros--)
		{
			int positive = DecodeVLE(); // nonzero value
			int delta = (positive >> 1) ^ -(positive & 1);
			current = previous + delta;
			*output++ = current;
			previous = current;
		}
	}
}