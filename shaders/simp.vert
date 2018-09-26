#version 150

in vec4 vert;
in vec4 col;
in vec4 norm;

uniform mat4 camera;
uniform mat4 model;

//out Vertex
//{
//    vec4 color;
//	vec4 normal;
//} vertex;

out vec4 VertexColor;
void main(){
	gl_Position = camera * model *vert; 
	VertexColor = col;
	//gl_Position = vert;
	//vertex.color = col;
	//vertex.normal = norm;
}
