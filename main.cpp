/*
 main
 
 Copyright 2012 Thomas Dalling - http://tomdalling.com/

 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

     http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
 */
#include "platform.hpp"
#include <Windows.h>
// third-party libraries
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// standard C++ libraries
#include <cassert>
#include <iostream>
#include <stdexcept>
#include <cmath>
#include <sstream>

// tdogl classes
#include "tdogl/Program.h"
#include "tdogl/Texture.h"
#include "tdogl/Camera.h"
#include "tdogl/FrameBuffer.h"

//boost 
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>

//Video decoders
#define ffmpeg
//#define nvdec

#ifdef nvdec
#include "NvDec/NvDecodeGL.h"
#endif

#ifdef ffmpeg
#include "FFDecoder.h"
#endif

#include "RVLDecoder.h"

typedef unsigned int uint;
// constants
const glm::vec2 SCREEN_SIZE(1280, 720);

//Because current glew does not define it
#ifndef GL_SHADER_GLOBAL_ACCESS_BARRIER_BIT_NV
#define GL_SHADER_GLOBAL_ACCESS_BARRIER_BIT_NV             0x00000010
#endif

// globals
GLFWwindow* gWindow = NULL;
double gScrollY = 0.0;
tdogl::Program* blurProgram = NULL;
tdogl::Program* rttProgram = NULL;
tdogl::Program* diffProgram = NULL;
tdogl::Program* g2Program = NULL;
tdogl::Program* brushStrokeProgram = NULL;

tdogl::Camera gCamera;
FrameBuffer fbo;

GLuint gVAO;
std::vector<GLsizei> numElements;
GLfloat gDegreesRotated = 0.0f;
glm::vec2 lastMousePos(0, 0);
std::vector<float> resolutions;
unsigned int rtt_vbo, rtt_ibo, rtt_vao;
int wWidth, wHeight;
int gridSizeBig, gridSizeSmall;
bool paint = true;
int cloud = 0;
std::vector<glm::vec4> defColors;
glm::vec4 pBackgroundColor(1, 1, 1, 1);
float alph = 1.0f;
float saturation = 1.2f;
float epsilon = 0.3f;
// loads the vertex shader and fragment shader, and links them to make the program

///Config Variables
std::string videosDir;
std::string colorStreamName;
std::string depthStreamName;
std::string normalStreamName;
int layerNum;
#ifdef ffmpeg
	std::vector<FFDecoder*> colorStreams;
	std::vector<FFDecoder*> normalStreams;
#endif // ffmpeg
#ifdef nvdec
	std::vector<cNvDecoder*> colorStreams;
	std::vector<cNvDecoder*> normalStreams;
#endif
std::vector<RVLDecoder*> depthStreams;
std::vector<glm::mat4> calibStreams;
glm::mat3 intrinsics;
int vidWidth;
int vidHeight;

int style = 0;
using namespace std;
int normalMethod = 5;

GLuint vertexBufferName = 0;
GLuint vertexAttribName = 0;
std::vector<tdogl::ShaderMacroStruct>	shadersMacroList;

bool dirty = false;
int cleanframes = 0;

bool debug = false;
float debugCount = 0.0;
bool dualLayer = false;

// Brush Stroke Gaussians
float a = 5.86004f;
float sigmax = 0.0489997f; float sigmay = 0.0208999f;
float gamma = 1.6f;
float dev = 0.11f;
string exec_path;

int **_depthBuffers;
GLuint _depthArrayBuffer;

bool _next = true;

void resetShadersGlobalMacros(){
	shadersMacroList.clear();
}

void setShadersGlobalMacro(const char *macro, int val){
	tdogl::ShaderMacroStruct ms;
	ms.macro = std::string(macro);

	char buff[128];
	sprintf_s(buff, "%d", val);
	ms.value = std::string(buff);

	shadersMacroList.push_back(ms);
}
void setShadersGlobalMacro(const char *macro, float val){
	tdogl::ShaderMacroStruct ms;
	ms.macro = std::string(macro);

	char buff[128];
	sprintf_s(buff, "%ff", val);

	ms.value = std::string(buff);

	shadersMacroList.push_back(ms);
}

static void checkError(const char* msg){
	GLenum error;
	error = glGetError();
	if (error != GL_NO_ERROR)
		std::cerr << "OpenGL Error " << msg << " " << error << std::endl;

}
vector<string> GetFilesInDirectory(const string directory)
{
	HANDLE dir;
	WIN32_FIND_DATA file_data;
	vector<string> out;
	string direc = directory + "/*";
	const char* temp = direc.c_str();

	if ((dir = FindFirstFile(temp, &file_data)) == INVALID_HANDLE_VALUE)
		return out;

	do {
		const string file_name = file_data.cFileName;
		const string full_file_name = directory + "\\" + file_name;
		const bool is_directory = (file_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;

		if (file_name[0] == '.')
			continue;

		if (is_directory)
			continue;

		out.push_back(full_file_name);
	} while (FindNextFile(dir, &file_data));

	FindClose(dir);
	return out;
}

vector<string> split(const std::string &s, char delim) {
	vector<string> elems;
	std::stringstream ss(s);
	std::string item;
	while (std::getline(ss, item, delim)) {
		elems.push_back(item);
	}
	return elems;
}

void loadConfig() {
	boost::property_tree::ptree pt;
	boost::property_tree::ini_parser::read_ini("output.ini", pt);
	videosDir = pt.get<std::string>("videosDir");
	colorStreamName = pt.get<std::string>("colorStreamName");
	depthStreamName = pt.get<std::string>("depthStreamName");
	normalStreamName = pt.get<std::string>("normalStreamName");
	vidWidth = pt.get<int>("vidWidth");
	vidHeight = pt.get<int>("vidHeight");
	layerNum = pt.get<int>("numLayers");
	stringstream ss;
	for (int i = 0; i < layerNum; i++)
	{
		glm::mat4 calibration;
		ss.swap(stringstream());
		ss << i;
		string calib = pt.get<std::string>(ss.str());
		vector<string> values = split(calib, ';');
		int m = 0;
		for (int j = 0; j < 4; j++) {
			for (int k = 0; k < 4; k++) {
				calibration[k][j] = stof(values[m++].c_str());
			}
		}
		//glm::mat4 scale(1);
		
	 	/*scale[0][0] = -1;
	    calibration = scale *calibration ;*/
		
		calibStreams.push_back(calibration);
	}
	intrinsics = glm::mat3(351.001462, 0, 255.5, 0, 351.001462, 211.5, 0, 0, 1);
	
//	intrinsics = glm::mat3(351.0014612, 0, 0, 0, 351.001462, 0, 255.5, 211.5, 1);

}

tdogl::Program *createProgram(std::string vertexShader, std::string fragmentShader, string output, std::string geometryShader=""){
	std::vector<tdogl::Shader> shaders;
	shaders.push_back(tdogl::Shader::shaderFromFile(vertexShader, GL_VERTEX_SHADER,shadersMacroList));
	shaders.push_back(tdogl::Shader::shaderFromFile(fragmentShader, GL_FRAGMENT_SHADER, shadersMacroList));
	if (geometryShader != "")
		shaders.push_back(tdogl::Shader::shaderFromFile(geometryShader, GL_GEOMETRY_SHADER, shadersMacroList));

	tdogl::Program *res = new tdogl::Program(shaders);
	glBindFragDataLocation(res->object(), 0, output.c_str());
	return res;
}

static void LoadShaders_3D(){

		
	rttProgram = createProgram("rtt.vert", "rtt.frag", "outColor");

	blurProgram = createProgram("rtt.vert", "blur.frag", "FragmentColor");

	g2Program = createProgram("simp.vert", "simp.frag", "outColor");

	brushStrokeProgram = createProgram("vertex-shader.vert", "frag-brush-shader.frag", "finalColor", "normals-shader.geom");
	
}

static void LoadShaders() {

	resetShadersGlobalMacros();

	setShadersGlobalMacro("SCREEN_WIDTH", wWidth);
	setShadersGlobalMacro("SCREEN_HEIGHT", wHeight);

	setShadersGlobalMacro("BACKGROUND_COLOR_R", pBackgroundColor.r);
	setShadersGlobalMacro("BACKGROUND_COLOR_G", pBackgroundColor.g);
	setShadersGlobalMacro("BACKGROUND_COLOR_B", pBackgroundColor.b);

	LoadShaders_3D();

	GLenum error = glGetError();
	if (error != GL_NO_ERROR)
		std::cerr << "OpenGL Error LoadShaders" << error << std::endl;
}

static void LoadRTTVariables(){
	glGenVertexArrays(1, &rtt_vao);
	glBindVertexArray(rtt_vao);

	// make and bind the VBO
	glGenBuffers(1, &rtt_vbo);
	glBindBuffer(GL_ARRAY_BUFFER, rtt_vbo);

	// Put the three triangle vertices (XYZ) and texture coordinates (UV) into the VBO
	GLfloat vertexData[] = {
		//  X     Y     Z       U     V
		-1.0f, 1.0f, 0.0f, 0.0f, 1.0f,
		-1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
		1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
		1.0f, 1.0f, 0.0f, 1.0f, 1.0f,
		-1.0f, 1.0f, 0.0f, 0.0f, 1.0f,
		1.0f, -1.0f, 0.0f, 1.0f, 0.0f
	};
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertexData), vertexData, GL_STATIC_DRAW);

	// connect the xyz to the "vert" attribute of the vertex shader
	glEnableVertexAttribArray(rttProgram->attrib("in_position"));
	glVertexAttribPointer(rttProgram->attrib("in_position"), 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), NULL);

	// connect the uv coords to the "vertTexCoord" attribute of the vertex shader
	glEnableVertexAttribArray(rttProgram->attrib("in_texcoord"));
	glVertexAttribPointer(rttProgram->attrib("in_texcoord"), 2, GL_FLOAT, GL_TRUE, 5 * sizeof(GLfloat), (const GLvoid*)(3 * sizeof(GLfloat)));

	// unbind the VAO
	glBindVertexArray(0);
}

static void LoadBuffer() {

	uint *varray = (uint*)malloc(sizeof(uint)*vidWidth*vidHeight*2);
	checkError("load buffer 1");
	glGenVertexArrays(1, &gVAO);
	glBindVertexArray(gVAO);

	

	int k = 0;
	for (uint i = 0; i < vidHeight; i++){
		for (uint j = 0; j < vidWidth; j++){
			varray[k++] = j;
			varray[k++] = i;
		}
	}
	
	GLuint gVBO1;
	
	glGenBuffers(1, &gVBO1);
	glBindBuffer(GL_ARRAY_BUFFER, gVBO1);
	glBufferData(GL_ARRAY_BUFFER, sizeof(GLuint)*vidWidth*vidHeight * 2, varray, GL_STATIC_DRAW);
	// connect the xyz to the "vert" attribute of the vertex shader
	glEnableVertexAttribArray(brushStrokeProgram->attrib("vert"));
	glVertexAttribIPointer(brushStrokeProgram->attrib("vert"), 2, GL_UNSIGNED_INT,0, NULL);
	checkError("load buffer 2");

	//------------------------------------------------------------- 


	glGenBuffers(1, &_depthArrayBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, _depthArrayBuffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(GLint)* vidWidth*vidHeight , 0, GL_STREAM_DRAW);
	// connect the xyz to the "dValue" attribute of the vertex shader
	glEnableVertexAttribArray(brushStrokeProgram->attrib("dValue"));
	glVertexAttribIPointer(brushStrokeProgram->attrib("dValue"), 1, GL_INT,0,NULL);
	checkError("load buffer 2.1");
		
	free(varray);
	
	// unbind the VAO
	glBindVertexArray(0);
}
static void renderPass(float resolutionMult, int outbuf,bool next){

	fbo.bind();
	{
		checkError("first pass -1");
		GLenum DrawBuffers[1] = { GL_COLOR_ATTACHMENT0 + outbuf };
		glDrawBuffers(1, DrawBuffers);
		checkError("first pass 0");
		// clear everything
		glClearColor(pBackgroundColor.r, pBackgroundColor.g, pBackgroundColor.b, pBackgroundColor.a);
		// black
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// bind the program (the shaders)
		brushStrokeProgram->use();
		// set the "camera" uniform
		glm::mat4 matrix = gCamera.matrix();
		brushStrokeProgram->setUniform("camera", matrix);
		// set the "model" uniform in the vertex shader, based on the gDegreesRotated global
		glm::mat4 rotate = glm::rotate(glm::mat4(), glm::radians(gDegreesRotated), glm::vec3(0, 1, 0));
		brushStrokeProgram->setUniform("model", rotate);
		checkError("first pass 1");

		glBindVertexArray(rtt_vao);
		brushStrokeProgram->setUniform("dev", dev);
		brushStrokeProgram->setUniform("gamma", gamma);
		brushStrokeProgram->setUniform("sigmax", sigmax);
		brushStrokeProgram->setUniform("sigmay", sigmay);
		brushStrokeProgram->setUniform("a", a);
		float alph = 1.0;
		float s = 1.0;
		brushStrokeProgram->setUniform("normalMethod", normalMethod);
		brushStrokeProgram->setUniform("saturation", s);
		brushStrokeProgram->setUniform("alph", alph);
		brushStrokeProgram->setUniform("scale", resolutionMult);
		brushStrokeProgram->setUniform("intrinsics", intrinsics);
		brushStrokeProgram->setUniform("width", vidWidth);
		brushStrokeProgram->setUniform("height", vidHeight);
		checkError("first pass 2");
		///CHAMAR AS PARADAS DOS VIDEOS
		for (int i = 0; i < colorStreams.size(); i++)
		{
			glActiveTexture(GL_TEXTURE0); 
			colorStreams[i]->renderVideoFrame(next);
			brushStrokeProgram->setUniform("colorTex", 0);
			
			glActiveTexture(GL_TEXTURE1);
			if(normalStreamName != "")
			{
				normalStreams[i]->renderVideoFrame(next);
				brushStrokeProgram->setUniform("normalTex", 1);
				brushStrokeProgram->setUniform("preCalcNormals", true);
			}
			else 
			{
				brushStrokeProgram->setUniform("preCalcNormals", false);
			}
			glBindVertexArray(gVAO);
			glBindBuffer(GL_ARRAY_BUFFER, _depthArrayBuffer);
			checkError("load render mapbuffer");
			if(next){
				depthStreams[i]->DecompressRVL(_depthBuffers[i], vidWidth*vidHeight);
			}
			glBufferSubData(GL_ARRAY_BUFFER, 0, vidWidth*vidHeight * sizeof(int), _depthBuffers[i]);
						
			brushStrokeProgram->setUniform("calib", calibStreams[i]);
			glDrawArrays(GL_POINTS, 0, vidWidth*vidHeight);
		}

		// unbind the VAO, the program and the texturesd
		glBindVertexArray(0);
		glBindTexture(GL_TEXTURE_2D, 0);
		brushStrokeProgram->stopUsing();
	}
	fbo.unbind();

}

static void blurPass(int iterations){

	fbo.bind();
	glBindVertexArray(rtt_vao);
	glActiveTexture(GL_TEXTURE0 + 1);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	blurProgram->use();
	blurProgram->setUniform("image", 1);
	blurProgram->setUniform("width", (float)wWidth);
	blurProgram->setUniform("height", (float)wHeight);
	
	GLenum db1[1] = { GL_COLOR_ATTACHMENT1 };
	GLenum db2[1] = { GL_COLOR_ATTACHMENT2 };
	//pass 2.1, horiz uses fbo text as input
	glDrawBuffers(1, db1);
	glClearColor(pBackgroundColor.r, pBackgroundColor.g, pBackgroundColor.b, pBackgroundColor.a);

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glBindTexture(GL_TEXTURE_2D, fbo.getColorTexture(0));
	blurProgram->setUniform("d", 0);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	
	//pass 2.2, horiz uses fbo text as input
	glDrawBuffers(1, db2);
	glClearColor(pBackgroundColor.r, pBackgroundColor.g, pBackgroundColor.b, pBackgroundColor.a);

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glBindTexture(GL_TEXTURE_2D, fbo.getColorTexture(1));
	blurProgram->setUniform("d", 1);
	glDrawArrays(GL_TRIANGLES, 0, 6);


	for (int i = 0; i < iterations; i++){

		glDrawBuffers(1, db1);
		glClearColor(pBackgroundColor.r, pBackgroundColor.g, pBackgroundColor.b, pBackgroundColor.a);

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glBindTexture(GL_TEXTURE_2D, fbo.getColorTexture(2));
		blurProgram->setUniform("d", 0);
		glDrawArrays(GL_TRIANGLES, 0, 6);
		

		glDrawBuffers(1, db2);
		glClearColor(pBackgroundColor.r, pBackgroundColor.g, pBackgroundColor.b, pBackgroundColor.a);

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glBindTexture(GL_TEXTURE_2D, fbo.getColorTexture(1));
		blurProgram->setUniform("d", 1);
		glDrawArrays(GL_TRIANGLES, 0, 6);
	
	}

	// unbind the VAO, the program and the texture
	glBindVertexArray(0);
	blurProgram->stopUsing();
	fbo.unbind();
}

static void finalPass(int blend){
	glClearColor(pBackgroundColor.r, pBackgroundColor.g, pBackgroundColor.b, pBackgroundColor.a);

	// glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	
	rttProgram->use();
	glActiveTexture(GL_TEXTURE0 + 2);
	glBindTexture(GL_TEXTURE_2D, fbo.getColorTexture(0));
	rttProgram->setUniform("texture_color", 2); 
	glActiveTexture(GL_TEXTURE0 + 3);
	glBindTexture(GL_TEXTURE_2D, fbo.getColorTexture(blend));
	rttProgram->setUniform("texture_blur", 3);

	glBindVertexArray(rtt_vao);

	// draw the VAO
	glDrawArrays(GL_TRIANGLES, 0, 6);

	// unbind the VAO, the program and the texture

	glBindVertexArray(0);
	rttProgram->stopUsing();
}


static void threeDRender(bool next){
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	//glEnable(GL_CULL_FACE);
	////glEnable(GL_STENCIL_TEST);
	glDepthMask(GL_TRUE);

	renderPass(1 + epsilon, 0,next);
	checkError("render pass");
	//pass 2: blur 
	blurPass(1);
	checkError("blur pass");

	finalPass(2);
	//debugPass();
}

static void threeDRenderNoBlur(bool next){
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	glDepthMask(GL_TRUE);

	renderPass(1 + epsilon, 0, next);
	blurPass(1);
	finalPass(2);
	checkError("first pass");
}

static void cloudRender(bool next){
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	checkError("begin2");
	//glEnable(GL_CULL_FACE);
	////glEnable(GL_STENCIL_TEST);
	glDepthMask(GL_TRUE);
	glPointSize(2.0);
	glLineWidth(3.0);
	// clear everything
	glClearColor(pBackgroundColor.r, pBackgroundColor.g, pBackgroundColor.b, pBackgroundColor.a);

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// bind the program (the shaders)
	g2Program->use();

	// set the "camera" uniform
	g2Program->setUniform("camera", gCamera.matrix());

	// set the "model" uniform in the vertex shader, based on the gDegreesRotated global
	g2Program->setUniform("model", glm::rotate(glm::mat4(), glm::radians(gDegreesRotated), glm::vec3(0, 1, 0)));


	// bind the texture and set the "tex" uniform in the fragment shader

	///CHAMAR AS PARADAS DOS VIDEOS
	for (int i = 0; i < layerNum; i++)
	{
		glActiveTexture(GL_TEXTURE0);
		colorStreams[i]->renderVideoFrame(next);
		brushStrokeProgram->setUniform("colorTex", 0);
		if(normalStreamName != "")
		{
			normalStreams[i]->renderVideoFrame(next);
			glActiveTexture(GL_TEXTURE1);
			brushStrokeProgram->setUniform("normalTex", 1);	
			brushStrokeProgram->setUniform("preCalcNormals", true);
		}
		else
		{
			brushStrokeProgram->setUniform("preCalcNormals", false);
		}


		brushStrokeProgram->setUniform("calib", calibStreams[i]);
		glDrawArrays(GL_POINTS, 0, vidWidth*vidHeight);
			
	}

	checkError("draw");
	// unbind the VAO, the program and the texture
	glBindVertexArray(0);
	g2Program->stopUsing();
}

void drawQuad(tdogl::Program *prog) {

	glUseProgram(prog->object());

	glBindVertexArray(vertexAttribName);
	checkError("error pointer");
	glDrawArrays(GL_TRIANGLES, 0, 24);

	checkError ("drawQuad");
}

// draws a single frame
static void Render(bool next) {
	threeDRenderNoBlur(next);
	//cloudRender(next);
	glfwSwapBuffers(gWindow);
}

float paintCount = 0;
float dualCount = 0;
// update the scene based on the time elapsed since last update
void Update(float secondsElapsed){
    //rotate the cube
  //  const GLfloat degreesPerSecond = 20.0f;
  //  gDegreesRotated += secondsElapsed * degreesPerSecond;
  //  if(gDegreesRotated > 360.0f) 
		//gDegreesRotated -= 360.0f;

    //move position of camera based on WASD keys, and XZ keys for up and down
    const float moveSpeed = 0.5; //units per second
    if(glfwGetKey(gWindow, 'S')){
        gCamera.offsetPosition(secondsElapsed * moveSpeed * -gCamera.forward());
		dirty = true;
    } else if(glfwGetKey(gWindow, 'W')){
		gCamera.offsetPosition(secondsElapsed * moveSpeed * gCamera.forward());
		dirty = true;
    }
    if(glfwGetKey(gWindow, 'A')){
		gCamera.offsetPosition(secondsElapsed * moveSpeed * -gCamera.right());
		dirty = true;
    } else if(glfwGetKey(gWindow, 'D')){
		gCamera.offsetPosition(secondsElapsed * moveSpeed * gCamera.right());
		dirty = true;
    }
    if(glfwGetKey(gWindow, 'Q')){
		gCamera.offsetPosition(secondsElapsed * moveSpeed * -glm::vec3(0, 1, 0));
		dirty = true;
    } else if(glfwGetKey(gWindow, 'E')){
		gCamera.offsetPosition(secondsElapsed * moveSpeed * glm::vec3(0, 1, 0));
		dirty = true;
	}else if (glfwGetKey(gWindow, 'P')){
		_next = !_next;
		dirty = true;
	}
	else if (glfwGetKey(gWindow, 'F')){
		//alph += 0.01;
		dev += 0.01f;
		dirty = true;
		//if (alph > 1) alph = 1;
	}
	else if (glfwGetKey(gWindow, 'V')){
		//alph -= 0.01;
		dev -= 0.01f;
		dirty = true;
		//if (alph < 0) alph = 0;
	}
	else if (glfwGetKey(gWindow, 'G')){
		//saturation += 0.01;
		gamma += 0.1f;
		dirty = true;
	}
	else if (glfwGetKey(gWindow, 'B')){
		//saturation -= 0.01;
		gamma -= 0.1f;
		dirty = true;
		//if (saturation < 0) saturation = 0;
	}
	else if (glfwGetKey(gWindow, 'H')){
		epsilon += 0.01f;
		dirty = true;
	}
	else if (glfwGetKey(gWindow, 'N')){
		epsilon -= 0.01f;
		dirty = true;
	}
	else if (glfwGetKey(gWindow, 'J')){
		//patchScale += 0.001;
		sigmax += 0.001f;
		dirty = true;
	}
	else if (glfwGetKey(gWindow, 'M')){
		//patchScale -= 0.001;
		sigmax -= 0.001f;
		dirty = true;
	}
	else if (glfwGetKey(gWindow, 'K')){
		sigmay += 0.001f;
		dirty = true;
		//style++;
		//if (style > 30){
			//style = 0;
		//	dirty = true;
	//	}
	}
	else if (glfwGetKey(gWindow, 'L')){
		sigmay -= 0.001f;
		dirty = true;
	}
	else if (glfwGetKey(gWindow, 'I')){
		a += 0.01f;
		dirty = true;
	}
	else if (glfwGetKey(gWindow, 'O')){
		a -= 0.01f;
		dirty = true;
	}

	else if (glfwGetKey(gWindow, '1')){
		normalMethod = 1;
		dirty = true;
	}
	else if (glfwGetKey(gWindow, '2')){
		normalMethod = 2;
		dirty = true;
	}
	else if (glfwGetKey(gWindow, '3')){
		normalMethod = 3;
		dirty = true;
	}
	else if (glfwGetKey(gWindow, '4')){
		normalMethod = 4;
		dirty = true;
	}
	else if (glfwGetKey(gWindow, '5')){
		normalMethod = 5;
		dirty = true;
	}
	else if (glfwGetKey(gWindow, '6')){
		normalMethod = 6;
		dirty = true;
	}
	else if (glfwGetKey(gWindow, '7')){
		debugCount += secondsElapsed;
		if (debugCount > 1) {
			debug = !debug;
			debugCount = 0;
			}
		}
	else if (glfwGetKey(gWindow, '8')){
		dualCount += secondsElapsed;
		if (dualCount > 1){
			dualLayer = !dualLayer;
			dualCount = 0;
			dirty = true;
		}
	}



	if (debug)
		cout << " a = " << a << "| dev = " << dev << "| gamma = " << gamma << "| sigmax = " << sigmax << "| sigmay = " << sigmay << endl;

    //rotate camera based on mouse movement
    const float mouseSensitivity = 0.1f;
    double mouseX, mouseY;
	int state = glfwGetMouseButton(gWindow, GLFW_MOUSE_BUTTON_LEFT);
	glfwGetCursorPos(gWindow, &mouseX, &mouseY);
	if (state == GLFW_PRESS){
		dirty = true;
		double deltaY = mouseY - lastMousePos.y;
		double deltaX = mouseX - lastMousePos.x;
		gCamera.offsetOrientation(mouseSensitivity * (float)deltaY, mouseSensitivity * (float)deltaX);
	
	}
	lastMousePos.x = (float)mouseX;
	lastMousePos.y = (float)mouseY;

    //increase or decrease field of view based on mouse wheel
    const float zoomSensitivity = -0.2f;
    float fieldOfView = gCamera.fieldOfView() + zoomSensitivity * (float)gScrollY;
    if(fieldOfView < 5.0f) fieldOfView = 5.0f;
    if(fieldOfView > 130.0f) fieldOfView = 130.0f;
    gCamera.setFieldOfView(fieldOfView);
    gScrollY = 0;
}

// records how far the y axis has been scrolled
void OnScroll(GLFWwindow* window, double deltaX, double deltaY) {
    gScrollY += deltaY;
}

void OnError(int errorCode, const char* msg) {
    throw std::runtime_error(msg);
}

void OnResize(GLFWwindow* window, int width, int height){
	wWidth = width;
	wHeight = height;
	glViewport(0, 0, width, height);
	gCamera.setViewportAspectRatio((float)width / (float)height);
	//LoadShade(); 
	fbo.resize(width, height);
	
}

void init(){
	loadConfig();
	wWidth =(int) SCREEN_SIZE.x;
	wHeight = (int)SCREEN_SIZE.y;

	// load vertex and fragment shaders into opengl
	LoadShaders();
	stringstream ss;
	//cNvDecoder::InitCuda();
	_depthBuffers = (int**)malloc(layerNum * sizeof(int*));
	for (int i = 0; i < layerNum; i++)
	{
		if (colorStreamName != ""){
			ss.swap(stringstream());
			ss << videosDir << "\\" << i << colorStreamName;
			glActiveTexture(GL_TEXTURE0);
#ifdef nvdec
			cNvDecoder *dec = new cNvDecoder(ss.str(),0,exec_path);
#endif
#ifdef ffmpeg
			FFDecoder *dec = new FFDecoder(ss.str());
#endif
			dec->init();
			colorStreams.push_back(dec);
			
		}	

		if (normalStreamName != ""){
			ss.swap(stringstream());
			ss << videosDir << "\\" << i << normalStreamName;
			glActiveTexture(GL_TEXTURE1);
#ifdef nvdec
			cNvDecoder *dec = new cNvDecoder(ss.str(), 0, exec_path);	
#endif
#ifdef ffmpeg
			FFDecoder *dec = new FFDecoder(ss.str());
#endif
			dec->init();
			normalStreams.push_back(dec);
		}

		if (depthStreamName != "") {
			ss.swap(stringstream());
			ss << videosDir << "\\" << i << depthStreamName;
			RVLDecoder *dec = new RVLDecoder();
			dec->InitDecoder(vidWidth, vidHeight, ss.str());
			_depthBuffers[i] = (int*)malloc(sizeof(int)*vidWidth*vidHeight);
			depthStreams.push_back(dec);
		}

	}
	// create buffer and fill it with the image coordinates
	LoadBuffer();

	LoadRTTVariables();

	//create frame buffer
	gridSizeBig = 25;
	gridSizeSmall = 18;
	fbo.GenerateFBO(wWidth, wHeight, 4);

	// setup gCamera
	gCamera.setPosition(glm::vec3(0, 0, 0));
	gCamera.setViewportAspectRatio(SCREEN_SIZE.x / SCREEN_SIZE.y);
}

// the program starts here
void AppMain() {
    // initialise GLFW
    glfwSetErrorCallback(OnError);
    if(!glfwInit())
        throw std::runtime_error("glfwInit failed");
    
    // open a window with GLFW
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_RESIZABLE, GL_TRUE);
    gWindow = glfwCreateWindow((int)SCREEN_SIZE.x, (int)SCREEN_SIZE.y, "OpenGL Tutorial", NULL, NULL);
    if(!gWindow)
        throw std::runtime_error("glfwCreateWindow failed. Can your hardware handle OpenGL 3.2?");

    // GLFW settings
    glfwSetInputMode(gWindow, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
    glfwSetCursorPos(gWindow, 0, 0);
    glfwSetScrollCallback(gWindow, OnScroll);
	glfwMakeContextCurrent(gWindow);
	glfwSetFramebufferSizeCallback(gWindow, OnResize);
    // initialise GLEW
    glewExperimental = GL_TRUE; //stops glew crashing on OSX :-/
    if(glewInit() != GLEW_OK)
        throw std::runtime_error("glewInit failed");
    
    // GLEW throws some errors, so discard all the errors so far
    while(glGetError() != GL_NO_ERROR) {}

    // print out some info about the graphics drivers
    std::cout << "OpenGL version: " << glGetString(GL_VERSION) << std::endl;
    std::cout << "GLSL version: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << std::endl;
    std::cout << "Vendor: " << glGetString(GL_VENDOR) << std::endl;
    std::cout << "Renderer: " << glGetString(GL_RENDERER) << std::endl;

    // make sure OpenGL version 3.2 API is available
    if(!GLEW_VERSION_3_2)
        throw std::runtime_error("OpenGL 3.2 API is not available.");

	init();

    // run while the window is open
    double lastTime = glfwGetTime();
	double lastFrameTime = lastTime;
	int nbFrames = 0;
	double lastPrint = lastTime;
    while(!glfwWindowShouldClose(gWindow)){
        // process pending events
        glfwPollEvents();
        
        // update the scene based on the time elapsed since last update
        double thisTime = glfwGetTime();
		Update((float)(thisTime - lastTime));
		lastTime = thisTime;
     
		nbFrames++;

		if (thisTime - lastPrint >= 1.0){ // If last prinf() was more than 1 sec ago
			// printf and reset timer
			printf("%f frames/s \n", double(nbFrames));
			nbFrames = 0;
			lastPrint += 1.0;
		}   
        // draw one frame
		bool requestNewFrame = false;
		if(thisTime - lastFrameTime > 0.03){
			requestNewFrame = true;
			lastFrameTime = thisTime;
		}

		Render(requestNewFrame && _next);
        
		// check for errors
        GLenum error = glGetError();
        if(error != GL_NO_ERROR)
            std::cerr << "OpenGL Error " << error << std::endl;

        //exit program if escape key is pressed
        if(glfwGetKey(gWindow, GLFW_KEY_ESCAPE))
            glfwSetWindowShouldClose(gWindow, GL_TRUE);
    }

	for (int i = 0; i < layerNum; i++)
	{
		if (i < colorStreams.size()){
			delete colorStreams[i];
		}
		if (i < depthStreams.size()) {
			delete depthStreams[i];
		}
		if (i < normalStreams.size()){
			delete normalStreams[i];
		}
	}
    // clean up and exit
    glfwTerminate();
}


int main(int argc, char *argv[]) {
    try {
		exec_path = argv[0];
		AppMain();
    } catch (const std::exception& e){
        std::cerr << "ERROR: " << e.what() << std::endl;
		getchar();
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
