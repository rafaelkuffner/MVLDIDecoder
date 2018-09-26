#version 450


in int dValue;
in uvec2 vert;

uniform sampler2DRect colorTex;
uniform sampler2DRect normalTex;


uniform mat4 calib;
uniform int height;
uniform int width;

uniform mat3 intrinsics;
uniform mat4 camera;
uniform mat4 model;

out Vertex
{
    vec4 color;
	vec4 normal;
} vertex;
   
void main() {
    //colors
	vec4 c = texture(colorTex,vec2(vert.x,vert.y));
	vec4 n = texture(normalTex,vec2(vert.x,vert.y));
	vec4 pos;
	pos.z = dValue / 1000.0;
	float vertx = float(width-vert.x);
	float verty = float(height -vert.y);
	pos.x =  pos.z*(vertx- intrinsics[0][2])/intrinsics[0][0];
	pos.y =  pos.z*(verty- intrinsics[1][2])/intrinsics[1][1];
	pos.w = 1;
	//transform
	pos = calib*pos;
	
	//normals
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

	if(dValue == 0)		
		c.a = 0;
	gl_Position =  pos;
    vertex.color = c;
	vertex.normal = n;
}
	




