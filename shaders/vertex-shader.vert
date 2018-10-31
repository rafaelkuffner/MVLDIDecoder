#version 450


//in int dValue;
in uvec2 vert;

uniform sampler2DRect colorTex;
uniform sampler2DRect normalTex;
uniform sampler2DRect depthTex;


uniform mat4 calib;
uniform int height;
uniform int width;

uniform mat3 intrinsics;
uniform mat4 camera;
uniform mat4 model;

uniform bool preCalcNormals;

out Vertex
{
    vec4 color;
	vec4 normal;
} vertex;
   

int textureToDepth(uint x, uint y)
{
	vec4 d = texture(depthTex,vec2(x,y));
	int dr = int(d.r*255);
	int dg = int(d.g*255);
	int db = int(d.b*255);
	int da = int(d.a*255);
	int dValue = int(db | (dg << 0x8) | (dr << 0x10) | (da << 0x18));
	return dValue;
}

int medianFilterDepth(int depth,uint x, uint y)
{	
	int _SizeFilter = 2;

	if(_SizeFilter == 0) return depth;
	uvec2 texCoord = uvec2(x,y);
	int sizeArray = (_SizeFilter*2 + 1)*(_SizeFilter*2 + 1);

	int arr[121];

	int k = 0;
	for (float i = -_SizeFilter; i <= _SizeFilter; i ++){
		for (float j = -_SizeFilter; j <= _SizeFilter; j ++){
			uvec2 pos = uvec2(i, j);
			uvec2 coords = texCoord + pos;
			int d = textureToDepth(coords.x,coords.y);
			arr[k] = d;
			k++;
		}
	}

	for (int j = 1; j < sizeArray; ++j)
	{
		int key = arr[j];
		int i = j - 1;
		while (i >= 0 && arr[i] > key)
		{
			arr[i+1] = arr[i];
			--i;
		}
		arr[i+1] = key;
	}
	int index = (_SizeFilter*2)+1;
	return arr[index];
}

vec4 estimateNormal(uint x, uint y)
{
	int width = 512;
	int height = 424;
	float yScale = 0.1;
	float xzScale = 1;
	float sx = textureToDepth(x< width-1 ? x+1 : x, y) -textureToDepth(x>0 ? x-1 : x, y);
	if (x == 0 || x == width-1)
		sx *= 2;

	float sy = textureToDepth(x, y<height-1 ? y+1 : y) - textureToDepth(x, y>0 ?  y-1 : y);
	if (y == 0 || y == height -1)
		sy *= 2;

	vec4 n =  vec4(-sx*yScale, sy*yScale,2*xzScale,1);
	n = normalize(n);
	return n;
}

void main() {
    //colors
	vec4 c = texture(colorTex,vec2(vert.x,vert.y));
	int dValue = textureToDepth(vert.x,vert.y);
	dValue = medianFilterDepth(dValue,vert.x,vert.y);
	//placeholder
	vec4 n = vec4(0,0,1,1);
	
	//normals
	if(preCalcNormals)
	{
		n = texture(normalTex,vec2(vert.x,vert.y));
	}else
	{
		n = estimateNormal(vert.x,vert.y);
	}

	int mask = int(n.a * 255);
	if((mask & 1 )== 1) n.x = -n.x;
	if((mask & 2) == 2) n.y = -n.y;
	if((mask & 4) == 4) n.z = -n.z;
	n = normalize(n);
	n.a = 1;
	mat4 rotation = calib;
	rotation[0][3]= 0;
	rotation[1][3]= 0;
	rotation[2][3]= 0;
	n = rotation *n;
	
	vec4 pos;
	pos.z = dValue / 1000.0;
	
	float vertx = float(width-vert.x);
	float verty = float(height -vert.y);
	pos.x =  pos.z*(vertx- intrinsics[0][2])/intrinsics[0][0];
	pos.y =  pos.z*(verty- intrinsics[1][2])/intrinsics[1][1];
	pos.w = 1;
	//transform
	pos = calib*pos;
	
	

	if(dValue == 0)		
		c.a = 0;
	gl_Position =  pos;
    vertex.color = c;
	vertex.normal = n;
}
	




