#version 150

uniform mat4 camera;
uniform mat4 model;
layout(points) in;
layout(triangle_strip, max_vertices = 4) out;

uniform float resolution;
uniform float scale;

in Vertex
{
    vec4 color;
	vec4 normal;
} vertex[];
     
out vec4 VertexColor;

void main() {
	  mat4 MV = camera;
 
		vec4 c = vertex[0].color;
		float size = resolution;
	  vec3 right = vec3(MV[0][0], 
						MV[1][0], 
						MV[2][0]);
 
	  vec3 up = vec3(MV[0][1], 
					 MV[1][1], 
					 MV[2][1]);

	vec3 P = gl_in[0].gl_Position.xyz + vertex[0].normal.xyz*(scale -1);
 
     
      vec3 va = P - (right + up) * size;
      gl_Position =  camera * model *vec4(va, 1.0);
      VertexColor = c;
      EmitVertex();  
      
      vec3 vb = P - (right - up) * size;
      gl_Position = camera * model *vec4(vb, 1.0);
      VertexColor = c;
      EmitVertex();  
     
      vec3 vd = P + (right - up) * size;
      gl_Position = camera * model *vec4(vd, 1.0);
      VertexColor = c;
      EmitVertex();  
     
      vec3 vc = P + (right + up) * size;
      gl_Position =camera * model * vec4(vc, 1.0);
      VertexColor = c;
      EmitVertex();  
      
      EndPrimitive();  
}