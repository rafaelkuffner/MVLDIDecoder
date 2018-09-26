#version 450

#extension GL_NV_gpu_shader5 : enable
#extension GL_EXT_bindable_uniform : enable

//Macros changed from the C++ side
#define SCREEN_WIDTH	512
#define SCREEN_HEIGHT	512
#define BACKGROUND_COLOR_B 1.000000f
#define BACKGROUND_COLOR_G 1.000000f
#define BACKGROUND_COLOR_R 1.000000f

smooth in vec4 fragPos;
in vec4 VertexColor;
in vec2 VertexUV;
in vec4 VertexTangent;

in float theta1;
in float theta2;
in float theta3;

uniform float dev;
uniform float gamma;
uniform float sigmax;
uniform float sigmay;
uniform float a;

uniform float alph;
uniform float saturation;
uniform bool dualPaint;



out vec4 finalColor;


float gaussian(float x, float x0, float y,float y0, float a, float sigmax, float sigmay, float gamma){
	return a*exp(-0.5 *( pow(pow(( x -x0)/sigmax,2),gamma/2)+pow(pow(( y -y0)/sigmay,2),gamma/2)));
}

float gaussianTheta(float x, float x0, float y,float y0, float a, float sigmax, float sigmay, float gamma,float theta){

	float x2 = cos(theta)*(x-x0)-sin(theta)*(y-y0)+x0;
	float y2 = sin(theta)*(x-x0)+cos(theta)*(y-y0)+y0;
	float z2 = a*exp(-0.5*( pow(pow((x2-x0)/sigmax,2),gamma/2)))*exp(-0.5*( pow(pow((y2-y0)/sigmay,2),gamma/2)));
	return z2;
}

vec4 sbrColor9Gaussian(float amp,float g, float sx,float sy, float desv, float sat){
	vec2 uv = VertexUV.xy;
	
	//------Brush stroke generation------//

	//ycenters and xcenters
	float xc[9]= {0.25f,0.5f,0.75f,0.25f,0.5f,0.75f,0.25f,0.5f,0.75f};
	float yc[9]= {0.25f,0.25f,0.25f,0.5f,0.5f,0.5f,0.75f,0.75f,0.75f};
	//float a =1.0f;
	//float sigmax = 0.15f; float sigmay = 0.12f;
	// float gamma =4;
	// float dev = 0.12;
	float alpha = 0;

	float a1 =gaussianTheta(uv.x, xc[0],uv.y,yc[0]+desv-theta1/6,amp,sx,sy,g,theta1);
	float a2 =gaussianTheta(uv.x, xc[1],uv.y,yc[1]+desv-theta2/6,amp*0.9,sx,sy,g,theta2); 
	float a3 =gaussianTheta(uv.x, xc[2],uv.y,yc[2]+desv-theta3/6,amp*0.57,sx,sy,g,theta3); 
	float a4 =gaussianTheta(uv.x, xc[3],uv.y,yc[3]-theta1/6,amp,sx,sy,g,theta1);
	float a5 =gaussianTheta(uv.x, xc[4],uv.y,yc[4]-theta2/6,amp*0.9,sx,sy,g,theta2);
	float a6 =gaussianTheta(uv.x, xc[5],uv.y,yc[5]-theta3/6,amp*0.7,sx,sy,g,theta3);
	float a7 =gaussianTheta(uv.x, xc[6],uv.y,yc[6]-desv-theta1/6,amp,sx,sy,g,theta1);
	float a8 =gaussianTheta(uv.x, xc[7],uv.y,yc[7]-desv-theta2/6,amp*0.9,sx,sy,g,theta2);
	float a9 =gaussianTheta(uv.x, xc[8],uv.y,yc[8]-desv-theta3/6,amp*0.57,sx,sy,g,theta3);
	
	//alpha = max(max(max(max(max(max(max(max(a1,a2),a3),a4),a5),a6),a7),a8),a9);
	alpha= a1+a2+a3+a4+a5+a6+a7+a8+a9;
//	alpha = gaussianTheta(uv.x, xc[4],uv.y,yc[4],a,sigmax,sigmay,gamma,0) + 0.001*(a1+a2+a3+a4+a5+a6+a7+a8+a9);
	//----------------------------------//

	alpha = alpha>1? 1:alpha;
	alpha = alpha < 1? alpha*1:alpha;
	alpha = alpha < 0.01? 0:alpha;
	vec4 t = vec4(1.0f,1.0f,1.0f,alpha);
	vec3 normal;
	if(t.a ==0){
		discard;
	}
	t = t*VertexColor;
	t.a = alph;
	float  P=sqrt(t.r*t.r*0.299+t.g*t.g*0.587+t.b*t.b*0.114 ) ;
	t.r=P+((t.r)-P)*(sat);
	t.g=P+((t.g)-P)*(sat);
	t.b=P+((t.b)-P)*(sat); 
	
	return  t;
}

float rand(vec2 co){
    return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453);
}

vec4 sbrColor6Gaussian(float amp,float g, float sx,float sy, float desv, float sat){
	vec2 uv = VertexUV.xy;
	
	//------Brush stroke generation------//

	//ycenters and xcenters
	float xc[9]= {0.3f,0.5f,0.75f,0.3f,0.5f,0.75f,0.3f,0.5f,0.75f};
	float yc[9]= {0.25f,0.25f,0.25f,0.5f,0.5f,0.5f,0.75f,0.75f,0.75f};
	//float a =1.0f;
	//float sigmax = 0.15f; float sigmay = 0.12f;
	// float gamma =4;
	// float dev = 0.12;
	float alpha = 0;

	float a1 =gaussianTheta(uv.x, xc[0],uv.y,yc[0]+desv-theta1/6,amp,sx,sy,g,theta1);
	float a2 =gaussianTheta(uv.x, xc[1],uv.y,yc[1]+desv-theta2/6,amp*0.9,sx,sy,g,theta2); 
	
	float a4 =gaussianTheta(uv.x, xc[3],uv.y,yc[3]-theta1/6,amp,sx,sy,g,theta1);

	float a6 =gaussianTheta(uv.x, xc[5],uv.y,yc[5]-theta3/6,amp*0.8,sx,sy,g,theta3);
	float a7 =gaussianTheta(uv.x, xc[6],uv.y,yc[6]-desv-theta1/6,amp,sx,sy,g,theta1);
	float a8 =gaussianTheta(uv.x, xc[7],uv.y,yc[7]-desv-theta2/6,amp*0.9,sx,sy,g,theta2);


	//alpha = max(max(max(max(max(max(max(max(a1,a2),a3),a4),a6),a8);
	alpha= a1+a2+a4+a6+a7+a8;
//	alpha = gaussianTheta(uv.x, xc[4],uv.y,yc[4],a,sigmax,sigmay,gamma,0.7853981634);
	//----------------------------------//

	alpha = alpha>1? 1:alpha;
	alpha = alpha < 0.01? 0:alpha;
	if(alpha < 1 && alpha > 0){
		alpha *=(1- mod(rand(vec2(VertexUV.x+fragPos.x,VertexUV.y+fragPos.y))*1000,0.2));
	}
	vec4 t = vec4(1.0f,1.0f,1.0f,alpha);
	vec3 normal;
	if(t.a ==0){
		discard;
	}
	t = t*VertexColor;
	t.a = alph;
	float  P=sqrt(t.r*t.r*0.299+t.g*t.g*0.587+t.b*t.b*0.114 ) ;
	t.r=P+((t.r)-P)*(saturation+0.3);
	t.g=P+((t.g)-P)*(saturation+0.3);
	t.b=P+((t.b)-P)*(saturation+0.3); 
	return  t;
}


vec4 sbrColor1Gaussian(float amp,float g, float sx,float sy, float desv, float sat){
	vec2 uv = VertexUV.xy;
	
	//------Brush stroke generation------//

	//ycenters and xcenters
	float xc[9]= {0.25f,0.5f,0.75f,0.25f,0.5f,0.75f,0.25f,0.5f,0.75f};
	float yc[9]= {0.25f,0.25f,0.25f,0.5f,0.5f,0.5f,0.75f,0.75f,0.75f};
	//float a =1.0f;
	//float sigmax = 0.15f; float sigmay = 0.12f;
	// float gamma =4;
	// float dev = 0.12;
	float alpha = 0;

	float a1 =gaussianTheta(uv.x, xc[0],uv.y,yc[0]+desv-theta1/6,amp,sx,sy,g,theta1);
	float a2 =gaussianTheta(uv.x, xc[1],uv.y,yc[1]+desv-theta2/6,amp*0.9,sx,sy,g,theta2); 
	float a3 =gaussianTheta(uv.x, xc[2],uv.y,yc[2]+desv-theta3/6,amp*0.9,sx,sy,g,theta3); 
	float a4 =gaussianTheta(uv.x, xc[3],uv.y,yc[3]-theta1/6,amp,sx,sy,g,theta1);
	float a5 =gaussianTheta(uv.x, xc[4],uv.y,yc[4]-theta2/6,amp*0.9,sx,sy,g,theta2);
	float a6 =gaussianTheta(uv.x, xc[5],uv.y,yc[5]-theta3/6,amp*0.9,sx,sy,g,theta3);
	float a7 =gaussianTheta(uv.x, xc[6],uv.y,yc[6]-desv-theta1/6,amp,sx,sy,g,theta1);
	float a8 =gaussianTheta(uv.x, xc[7],uv.y,yc[7]-desv-theta2/6,amp*0.9,sx,sy,g,theta2);
	float a9 =gaussianTheta(uv.x, xc[8],uv.y,yc[8]-desv-theta3/6,amp*0.9,sx,sy,g,theta3);
	
	//alpha = max(max(max(max(max(max(max(max(a1,a2),a3),a4),a5),a6),a7),a8),a9);
	//alpha= a1+a2+a3+a4+a5+a6+a7+a8+a9;
	alpha = gaussianTheta(uv.x, xc[4],uv.y,yc[4],a,sigmax,sigmay,gamma,0) + 0.0001*(a1+a2+a3+a4+a5+a6+a7+a8+a9);
	//----------------------------------//

	alpha = alpha>1? 1:alpha;
	alpha = alpha < 1? alpha*1:alpha;
	alpha = alpha < 0.5? 0:alpha;
	vec4 t = vec4(1.0f,1.0f,1.0f,alpha);
	vec3 normal;
	if(t.a ==0 ){
		discard;
	}
	t = t*VertexColor + (VertexTangent*0.0001);
	t.a = alph;
	float  P=sqrt(t.r*t.r*0.299+t.g*t.g*0.587+t.b*t.b*0.114 ) ;

	t.r=P+((t.r)-P)*(sat);
	t.g=P+((t.g)-P)*(sat);
	t.b=P+((t.b)-P)*(sat); 
	
	return  t;
}

void main() { 
	//vec4 backgroundColor=vec4(VertexColor.r,VertexColor.g, VertexColor.b, 0.0f);
	vec4 col1 = sbrColor9Gaussian(a,gamma,sigmax,sigmay,dev,saturation);
	//if(dualPaint){
	//	vec4 col2 = sbrColor9Gaussian(a*2,gamma-0.3,sigmax/2,sigmay/2,0,saturation+0.35);
	//	col1.rgb = col1.rgb*col1.a;
	//	col2.rgb = col2.rgb*col2.a;
	//	finalColor = finalColor + col2*(1.0f-finalColor.a);
	//	finalColor = finalColor + col1*(1.0f-finalColor.a);
	//	finalColor=finalColor+backgroundColor*(1.0f-finalColor.a);	
	//}else{
		finalColor = col1;	
	//}
	
}