#pragma once

#include <GL/glew.h>
#include <string>
#include <Windows.h>

using namespace std;

class RVLDecoder
{
private:
	int *_buffer, *_pBuffer, _word, _nibblesWritten;
	byte *_input;
	HANDLE _inFile;
	byte _sizeBuffer[4];
	int _width, _height;
	string _inputPath;
	byte *_depthBuffer;
	GLuint _texid;
public:
	RVLDecoder();
	virtual ~RVLDecoder();

private:
	int DecodeVLE();

public:
	bool	InitDecoder(int width, int height, string inputPath);
	void	ResetDecoder();
	void	DecompressRVL( int numPixels);
	void	render();
};