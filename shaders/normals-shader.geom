#version 450

#define PI3 1.04719755 
#define PI6 0.523598776
#define PI12 0.261799388

uniform mat4 camera;
uniform mat4 model;
//uniform vec3 camPosition;
uniform float scale;
uniform int normalMethod;
uniform bool preCalcNormals;

layout(points) in;
layout(triangle_strip, max_vertices = 4) out;


in Vertex
{
    vec4 color;
	vec4 normal;
} vertex[];
     
out vec2 VertexUV;
out vec4 VertexColor;
out vec4 VertexTangent;

smooth out vec4 fragPos;

float rand(vec2 co){
    return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453);
}

out float theta1;
out float theta2;
out float theta3;

void main() {
 //  float dist = distance(camPosition,gl_in[0].gl_Position.xyz);
   //a variável fora da exponencial diz quantas patches vão ter quando a distância for 0,
   // o número da exponencial tem a ver com a distância a qual ele vai ser 1. 
  // float tiling =1+(0*(pow(20,-dist)));
   //se quiser desativar o tiling, é aqui.
   float tiling = 1;
    float size = 0.025*scale;
	vec3 right = vec3(0,0,0);
	vec3 up = vec3(0,0,0);
	vec4 c = vertex[0].color;
	if(c.a !=0){
		vec3 normal = vertex[0].normal.xyz;
	
		float nx = vertex[0].normal.x;
		float ny = vertex[0].normal.y;
		float nz = vertex[0].normal.z;

		theta1 = mod(rand(vec2(gl_in[0].gl_Position.x,gl_in[0].gl_Position.x))*1000,PI3)-PI6;
		theta2 = mod(rand(vec2(gl_in[0].gl_Position.y,gl_in[0].gl_Position.y))*1000,PI3)-PI6;

		if(abs(theta2-theta1)> PI6){
			theta2 = theta1 < 0? theta1+PI6:theta1-PI6;
		}
		theta3 = mod(rand(vec2(gl_in[0].gl_Position.z,gl_in[0].gl_Position.z))*1000,PI3)-PI6;
		if(abs(theta3-theta2)> PI6){
			theta3 = theta2 < 0? theta2+PI6:theta2-PI6;
		} 

		int nm = normalMethod;
		if(!preCalcNormals) nm = 6;

		 switch(nm){
	 
		//-------- Spherical-------------//
		 case 1: 
			float norm = sqrt(pow(normal.x, 2) + pow(normal.y, 2) + pow(normal.z, 2));
			float teta = atan(normal.y /normal.x);
			float phi = acos(normal.z / norm);

			//# r_vec, teta_vec, phi_vec #//
			normal = vec3(cos(teta) * sin(phi), sin(teta) * sin(phi), cos(phi));
			right =vec3(-sin(teta), cos(teta), 0);
			up = vec3(cos(teta) * sin(phi),sin(teta) *cos(phi), -sin(phi));
			break;
		//---------Paralelo ao plano base---------//
		case 2:
			float x1 = sqrt(1/(1+(pow(nx,2)/pow(nz,2))));
			float x2 = -x1;
			float z1 = - nx*x1/nz;


			right = vec3(x1,0,z1);
			up = cross(right,normal);
			break;

		//-------SquarePlate----------//
		case 3:
			vec3 v;
			if ((abs (normal.x) >= 0.0f && abs (normal.y) >= 0.0f) || (abs (normal.x) <= 0.0f && abs (normal.y) <= 0.0f)) {
				v = vec3 (normal.x + 1,normal.y - 1, normal.z);
			} else {
				v = vec3 (normal.x - 1,normal.y - 1, normal.z);
			}
			//# t and b #//
			right =normalize(cross (v, normal));
			up = normalize(cross (normal, right));
			break;
		//------------Eberly-------------//
		case 4:
			vec3 t;
			vec3 b;
			if (abs (normal.x) >= abs (normal.y)) 
			{
				t = vec3 (- normal.z / sqrt (pow (normal.x, 2) + pow (normal.z, 2)), 
						0.0f,
							normal.x / sqrt (pow (normal.x, 2) + pow (normal.z, 2)));

				b = vec3 ((normal.x * normal.y) / sqrt (pow (normal.x, 2) + pow (normal.z, 2)), 
						- sqrt (pow (normal.x, 2) +pow (normal.z, 2)),
						(normal.y * normal.z) / sqrt (pow (normal.x, 2) + pow (normal.z, 2)));
			} else 
			{
				t = vec3 (0.0f, 
						normal.z / sqrt (pow (normal.y, 2) + pow (normal.z, 2)),
						- normal.y / sqrt (pow (normal.y, 2) + pow (normal.z, 2)));
			
				b = vec3 (- sqrt (pow (normal.y, 2) + pow (normal.z, 2)), 
									(normal.x * normal.y) / sqrt (pow (normal.y, 2) + pow (normal.z, 2)),
									(normal.x * normal.z) / sqrt (pow (normal.y, 2) + pow (normal.z, 2)));
			}
			////# t and b #//
			right = normalize (t);
			up = normalize (b);
			break;
		//---------Householder---------//
		case 5:
			float nx = vertex[0].normal.x;
			float ny = vertex[0].normal.y;
			float nz = vertex[0].normal.z;
			float n = sqrt(pow(nx,2) + pow(ny,2) + pow(nz,2));
			float h1 = max( normal.x - n , nx + n );
			float h2 = ny;
			float h3 = nz;
			float h = sqrt(pow(h1,2) + pow(h2,2) + pow(h3,2));

			right =  vec3(-2*h1*h3/pow(h,2), -2*h2*h3/pow(h,2), 1 - 2*pow(h3,2)/pow(h,2));
			up = vec3(-2*h1*h2/pow(h,2), 1 - 2*pow(h2,2)/pow(h,2), -2*h2*h3/pow(h,2));
			right = normalize(right);
			up = normalize(up);
			break;
			//---------splat---------//
		case 6:
			mat4 MV = camera;
 
		   right = vec3(MV[0][0], 
							MV[1][0], 
							MV[2][0]);
 
		   up = vec3(MV[0][1], 
						 MV[1][1], 
						 MV[2][1]);
			right = normalize(right);
			up = normalize(up);
			break;
		}
	
	  
		vec3 P = gl_in[0].gl_Position.xyz; //+ vertex[0].normal.xyz;
     
     
		vec3 va = P - (right + up) * size;
		fragPos =  camera * model *vec4(va, 1.0);
		gl_Position = fragPos;
		VertexUV = vec2(0.0, 0.0);
		VertexColor = c;
		VertexTangent = vec4(up,1.0);
		EmitVertex();  
      
		vec3 vb = P - (right - up) * size;
		fragPos = camera * model *vec4(vb, 1.0);
		gl_Position = fragPos;
		VertexUV = vec2(0.0, tiling);
		VertexColor = c;
		VertexTangent = vec4(up,1.0);
		EmitVertex();  
     
		vec3 vd = P + (right - up) * size;
		fragPos = camera * model *vec4(vd, 1.0);
		gl_Position = fragPos;
		VertexUV = vec2(tiling, 0.0);
		VertexColor = c;
		VertexTangent = vec4(up,1.0);
		EmitVertex();  
     
		vec3 vc = P + (right + up) * size;
		fragPos =camera * model * vec4(vc, 1.0);
		gl_Position = fragPos;
		VertexUV = vec2(tiling, tiling);
		VertexColor = c;
		VertexTangent = vec4(up,1.0);
		EmitVertex();  
      
		EndPrimitive();  
	}
}