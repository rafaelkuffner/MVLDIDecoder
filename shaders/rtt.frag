#version 150


uniform sampler2D texture_color;
uniform sampler2D texture_blur;

in vec2 texcoord;
out vec4 outColor;


void main(){
		vec4 texcol = texture(texture_color, texcoord);
		vec4 blurcol =  texture(texture_blur,texcoord);

		vec4 gs= vec4(0.299*texcol.r ,0.587*texcol.g ,0.114*texcol.b,1.0);
		float alpha = 1.1;
		texcol =( alpha * ( texcol )) + ((1.0-alpha) * gs);
		if(texcol == vec4(0,0,0,1))
			outColor =  blurcol;
		else
			outColor = (0.7*texcol+1.3*blurcol)/2;
}