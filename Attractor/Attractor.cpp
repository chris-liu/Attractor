#define WIN32_LEAN_AND_MEAN
#define WIN32_EXTRA_LEAN

#include <windows.h>
#include <gl\gl.h>
#include <gl\glu.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

#include "glext.h"
#include "Font.h"

//screen resolution
#define WIDTH 1440
#define HEIGHT 960
#define FULLSCREEN 1
#define BLUR 1
#define MAXBUFFER 3
#define MAXSHADERS 3
#define GLOWRES 8

//attractor parameters
#define MAXVERTS 200000
#define MAXCYCLE 1000
#define CYCLETHRESHOLD 50
#define NEARTHRESHOLD 0.0001
#define COLORMULTIPLIER 3.0
#define INITSEED 1234

//opengl extensions
PFNGLCREATEPROGRAMPROC glCreateProgram = NULL;
PFNGLCREATESHADERPROC glCreateShader = NULL;
PFNGLSHADERSOURCEPROC glShaderSource = NULL;
PFNGLCOMPILESHADERPROC glCompileShader = NULL;
PFNGLATTACHSHADERPROC glAttachShader = NULL;
PFNGLGETSHADERINFOLOGPROC glGetShaderInfoLog = NULL;
PFNGLLINKPROGRAMPROC glLinkProgram = NULL;
PFNGLUSEPROGRAMPROC glUseProgram = NULL;
PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation = NULL;
PFNGLUNIFORM1FPROC glUniform1f = NULL;
PFNGLUNIFORM2FPROC glUniform2f = NULL;
PFNGLUNIFORM1DPROC glUniform1d = NULL;
PFNGLUNIFORM2DPROC glUniform2d = NULL;
PFNGLUNIFORM1IPROC glUniform1i = NULL;
PFNGLGETPROGRAMINFOLOGPROC glGetProgramInfoLog = NULL;
PFNGLBINDFRAMEBUFFERPROC glBindFramebuffer = NULL;
PFNGLGENFRAMEBUFFERSPROC glGenFramebuffers = NULL;
PFNGLGENRENDERBUFFERSEXTPROC glGenRenderbuffers = NULL;
PFNGLBINDRENDERBUFFERPROC glBindRenderbuffer = NULL;
PFNGLRENDERBUFFERSTORAGEPROC glRenderbufferStorage = NULL;
PFNGLFRAMEBUFFERTEXTURE2DPROC glFramebufferTexture2D = NULL;
PFNGLFRAMEBUFFERRENDERBUFFERPROC glFramebufferRenderbuffer = NULL;
PFNGLUNIFORM1FVPROC glUniform1fv = NULL;
PFNGLACTIVETEXTUREARBPROC glActiveTexture = NULL;
PFNGLDELETEFRAMEBUFFERSPROC glDeleteFramebuffers = NULL;
PFNGLDELETERENDERBUFFERSPROC glDeleteRenderbuffers = NULL;
PFNGLBLENDEQUATIONPROC glBlendEquation = NULL;

//function prototypes
int WINAPI WinMain(HINSTANCE hinstance, HINSTANCE hprevinstance, LPSTR lpcmdline, int ncmdshow);
LRESULT	CALLBACK wndprc(HWND hwnd, UINT umsg, WPARAM wparam, LPARAM lparam);
void gencoefficients(double *c, int seed);
void keydown(WPARAM key);
void keyup(WPARAM key);
void drawquad();

//system variables
BOOL key[256];
BOOL active = TRUE;
BOOL exitswitch = FALSE;
int c_select = 0;
BOOL blur = TRUE;

//attractor variables
int seed = INITSEED;
double c[30];
int dir = 1;
BOOL reset = FALSE;
bool autosearch=true;

//view variables
float viewx = 0.0;
float viewy = 0.0;
float scale = 0.5;

int WINAPI WinMain(HINSTANCE hinstance, HINSTANCE hprevinstance, LPSTR lpcmdline, int ncmdshow){

	//windows variables
	HDC hdc = NULL;
	HGLRC hrc = NULL;
	HWND hwnd = NULL;
	MSG msg;
	PIXELFORMATDESCRIPTOR pfd = { sizeof(PIXELFORMATDESCRIPTOR), 1, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER, PFD_TYPE_RGBA, 16, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 16, 0, 0, PFD_MAIN_PLANE, 0, 0, 0, 0 };
	GLuint pixelformat;
	WNDCLASS wc;
	DWORD dwexstyle;
	DWORD dwstyle;
	int width, height;
	int screenwidth = GetSystemMetrics(SM_CXSCREEN);
	int screenheight = GetSystemMetrics(SM_CYSCREEN);

	//attractor variables
	float *v;
	float *color;
	double x = 0.1;
	double y = 0.1;
  double z = 0.1;
	double xn, yn, zn;
  clock_t t1,t2;
  double center[3];

	//frame buffer
	GLuint renderbuffer[MAXBUFFER];
	GLuint framebuffer[MAXBUFFER];
	GLuint texture[MAXBUFFER];
	int *buffer[MAXBUFFER];

	//texture passthrough source
	char *passthrough =
		"#version 120\n"
		"uniform sampler2D texture;\n"
		"uniform vec2 resolution;\n"
		"void main(){\n"
		"  vec2 p=gl_FragCoord.xy/resolution.xy;\n"
		"  gl_FragColor=texture2D(texture,p);\n"
		"}\n";

	//horizontal glow source
	char *horizontalglow =
		"#version 120\n"
		"uniform sampler2D texture;\n"
		"uniform vec2 resolution;\n"
		"void main(){\n"
		"  float k[15]=float[](0.0212,0.0327,0.0472,0.0637,0.0805,0.0951,0.1051,0.1086,0.1051,0.0951,0.0805,0.0637,0.0472,0.0327,0.0212);\n"
		"  vec2 p=gl_FragCoord.xy/resolution.xy;\n"
		"  vec2 d=vec2(1.0)/resolution.xy;\n"
		"  vec4 c=vec4(0.0);\n"
		"  vec4 ck;\n"
		"  float l;"
		"  for(int i=-7;i<=7;i++){\n"
		"    ck=texture2D(texture,p+d*vec2(float(i),0.0));\n"
		"    c+=1.5*k[i+7]*ck;\n"
		"  }\n"
		"  gl_FragColor=1.5*c;\n"
		"}\n";

	//horizontal glow source
	char *verticalglow =
		"#version 120\n"
		"uniform sampler2D texture;\n"
		"uniform vec2 resolution;\n"
		"void main(){\n"
		"  float k[15]=float[](0.0212,0.0327,0.0472,0.0637,0.0805,0.0951,0.1051,0.1086,0.1051,0.0951,0.0805,0.0637,0.0472,0.0327,0.0212);\n"
		"  vec2 p=gl_FragCoord.xy/resolution.xy;\n"
		"  vec2 d=vec2(1.0)/resolution.xy;\n"
		"  vec4 c=vec4(0.0);\n"
		"  vec4 ck;\n"
		"  float l;"
		"  for(int i=-7;i<=7;i++){\n"
		"    ck=texture2D(texture,p+d*vec2(0.0,float(i)));\n"
		"    c+=k[i+7]*ck;\n"
		"  }\n"
		"  gl_FragColor=c;\n"
		"}\n";

	//shaders
	int shaderprogram[MAXSHADERS];
	int fragmentshader[MAXSHADERS];

	//window attributes
	wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
	wc.lpfnWndProc = (WNDPROC)wndprc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = hinstance;
	wc.hIcon = LoadIcon(NULL, IDI_WINLOGO);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = NULL;
	wc.lpszMenuName = NULL;
	wc.lpszClassName = "Attractor";
	RegisterClass(&wc);

	//create window
#if FULLSCREEN
	DEVMODE dmScreenSettings;
	memset(&dmScreenSettings, 0, sizeof(dmScreenSettings));
	dmScreenSettings.dmSize = sizeof(dmScreenSettings);
	dmScreenSettings.dmPelsWidth = screenwidth;
	dmScreenSettings.dmPelsHeight = screenheight;
	dmScreenSettings.dmBitsPerPel = 16;
	dmScreenSettings.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;
	ChangeDisplaySettings(&dmScreenSettings, CDS_FULLSCREEN);
	dwexstyle = WS_EX_APPWINDOW;
	dwstyle = WS_POPUP;
	ShowCursor(FALSE);
	width = screenwidth;
	height = screenheight;
	hwnd = CreateWindowEx(dwexstyle, "Attractor", "Attractor", dwstyle | WS_CLIPSIBLINGS | WS_CLIPCHILDREN, 0, 0, width, height, NULL, NULL, hinstance, NULL);
#else
	dwexstyle = WS_EX_APPWINDOW | WS_EX_WINDOWEDGE;
	dwstyle = WS_OVERLAPPEDWINDOW;
	width = WIDTH;
	height = HEIGHT;
	hwnd = CreateWindowEx(dwexstyle, "Attractor", "Attractor", dwstyle | WS_CLIPSIBLINGS | WS_CLIPCHILDREN, (screenwidth - WIDTH) / 2, (screenheight - HEIGHT) / 2, width, height, NULL, NULL, hinstance, NULL);
#endif

	//attach gl context and show window
	hdc = GetDC(hwnd);
	pixelformat = ChoosePixelFormat(hdc, &pfd);
	SetPixelFormat(hdc, pixelformat, &pfd);
	hrc = wglCreateContext(hdc);
	wglMakeCurrent(hdc, hrc);
	ShowWindow(hwnd, SW_SHOW);
	SetForegroundWindow(hwnd);
	SetFocus(hwnd);

	//bind gl extensions
	OutputDebugString("Binding OpenGL extensions...\n");
	glCreateProgram = (PFNGLCREATEPROGRAMPROC)wglGetProcAddress("glCreateProgram");
	glCreateShader = (PFNGLCREATESHADERPROC)wglGetProcAddress("glCreateShader");
	glShaderSource = (PFNGLSHADERSOURCEPROC)wglGetProcAddress("glShaderSource");
	glCompileShader = (PFNGLCOMPILESHADERPROC)wglGetProcAddress("glCompileShader");
	glAttachShader = (PFNGLATTACHSHADERPROC)wglGetProcAddress("glAttachShader");
	glGetShaderInfoLog = (PFNGLGETSHADERINFOLOGPROC)wglGetProcAddress("glGetShaderInfoLog");
	glGetProgramInfoLog = (PFNGLGETPROGRAMINFOLOGPROC)wglGetProcAddress("glGetProgramInfoLog");
	glLinkProgram = (PFNGLLINKPROGRAMPROC)wglGetProcAddress("glLinkProgram");
	glUseProgram = (PFNGLUSEPROGRAMPROC)wglGetProcAddress("glUseProgram");
	glGetUniformLocation = (PFNGLGETUNIFORMLOCATIONPROC)wglGetProcAddress("glGetUniformLocation");
	glUniform1f = (PFNGLUNIFORM1FPROC)wglGetProcAddress("glUniform1f");
	glUniform2f = (PFNGLUNIFORM2FPROC)wglGetProcAddress("glUniform2f");
	glUniform1d = (PFNGLUNIFORM1DPROC)wglGetProcAddress("glUniform1d");
	glUniform2d = (PFNGLUNIFORM2DPROC)wglGetProcAddress("glUniform2d");
	glUniform1i = (PFNGLUNIFORM1IPROC)wglGetProcAddress("glUniform1i");
	glBindFramebuffer = (PFNGLBINDFRAMEBUFFERPROC)wglGetProcAddress("glBindFramebuffer");
	glGenFramebuffers = (PFNGLGENFRAMEBUFFERSPROC)wglGetProcAddress("glGenFramebuffers");
	glGenRenderbuffers = (PFNGLGENRENDERBUFFERSEXTPROC)wglGetProcAddress("glGenRenderbuffersEXT");
	glBindRenderbuffer = (PFNGLBINDRENDERBUFFERPROC)wglGetProcAddress("glBindRenderbuffer");
	glRenderbufferStorage = (PFNGLRENDERBUFFERSTORAGEPROC)wglGetProcAddress("glRenderbufferStorage");
	glFramebufferTexture2D = (PFNGLFRAMEBUFFERTEXTURE2DPROC)wglGetProcAddress("glFramebufferTexture2D");
	glFramebufferRenderbuffer = (PFNGLFRAMEBUFFERRENDERBUFFERPROC)wglGetProcAddress("glFramebufferRenderbuffer");
	glUniform1fv = (PFNGLUNIFORM1FVPROC)wglGetProcAddress("glUniform1fv");
	glActiveTexture = (PFNGLACTIVETEXTUREARBPROC)wglGetProcAddress("glActiveTexture");
	glDeleteFramebuffers = (PFNGLDELETEFRAMEBUFFERSPROC)wglGetProcAddress("glDeleteFramebuffers");
	glDeleteRenderbuffers = (PFNGLDELETERENDERBUFFERSPROC)wglGetProcAddress("glDeleteRenderbuffers");
	glBlendEquation = (PFNGLBLENDEQUATIONPROC)wglGetProcAddress("glBlendEquation");

  //init time
  t1 = clock();

	//init frame buffer
	OutputDebugString("Generating frame buffers...\n");
	glGenFramebuffers(MAXBUFFER, framebuffer);
	OutputDebugString("Generating render buffers...\n");
	glGenRenderbuffers(MAXBUFFER, renderbuffer);
	OutputDebugString("Generating textures...\n");
	glGenTextures(MAXBUFFER, texture);

	//glow resolution
	int glowwidth = width/GLOWRES;
	int glowheight = height/GLOWRES;

	//main scene
	OutputDebugString("Allocating texture 0...\n");
	buffer[0] = (int*)malloc(width*height*sizeof(int));

	//vertical glow
	OutputDebugString("Allocating texture 1...\n");
	buffer[1] = (int*)malloc(glowwidth*glowheight*sizeof(int));

	//horizontal glow
	OutputDebugString("Allocating texture 1...\n");
	buffer[2] = (int*)malloc(glowwidth*glowheight*sizeof(int));

	//main scene
	glBindTexture(GL_TEXTURE_2D, texture[0]);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, buffer[0]);
	glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer[0]);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	//horizontal glow
	glBindTexture(GL_TEXTURE_2D, texture[1]);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, glowwidth, glowheight, 0, GL_RGB, GL_UNSIGNED_BYTE, buffer[1]);
	glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer[1]);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, glowwidth, glowheight);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	//vertical glow
	glBindTexture(GL_TEXTURE_2D, texture[2]);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, glowwidth, glowheight, 0, GL_RGB, GL_UNSIGNED_BYTE, buffer[2]);
	glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer[2]);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, glowwidth, glowheight);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	//compile shaders
	OutputDebugString("Compiling shaders...\n");
	char log[2014];

	//passthrough shader
	shaderprogram[0] = glCreateProgram();
	fragmentshader[0] = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragmentshader[0], 1, (const GLchar**)&passthrough, 0);
	glCompileShader(fragmentshader[0]);
	memset(log, 0, sizeof(log));
	glGetShaderInfoLog(fragmentshader[0], sizeof(log), 0, log);
	OutputDebugString("Passthrough shader log:\n");
	OutputDebugString(log);
	glAttachShader(shaderprogram[0], fragmentshader[0]);
	glLinkProgram(shaderprogram[0]);

	//horizontal glow shader
	shaderprogram[1] = glCreateProgram();
	fragmentshader[1] = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragmentshader[1], 1, (const GLchar**)&horizontalglow, 0);
	glCompileShader(fragmentshader[1]);
	memset(log, 0, sizeof(log));
	glGetShaderInfoLog(fragmentshader[1], sizeof(log), 0, log);
	OutputDebugString("Horizontal glow shader log:\n");
	OutputDebugString(log);
	glAttachShader(shaderprogram[1], fragmentshader[1]);
	glLinkProgram(shaderprogram[1]);

	//passthrough glow shader
	shaderprogram[2] = glCreateProgram();
	fragmentshader[2] = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragmentshader[2], 1, (const GLchar**)&verticalglow, 0);
	glCompileShader(fragmentshader[2]);
	memset(log, 0, sizeof(log));
	glGetShaderInfoLog(fragmentshader[2], sizeof(log), 0, log);
	OutputDebugString("Vertical glow shader log:\n");
	OutputDebugString(log);
	glAttachShader(shaderprogram[2], fragmentshader[2]);
	glLinkProgram(shaderprogram[2]);

	//init opengl
	glClearColor(0.0, 0.0, 0.0, 0.0);

#if BLUR
	//for frame blur
	glClearAccum(0.0, 0.0, 0.0, 1.0);
	glClear(GL_ACCUM_BUFFER_BIT);
#endif

	//init attractor
	gencoefficients(c, seed);
	v = (float*)malloc(MAXVERTS * 3 * sizeof(float));
	color = (float*)malloc(MAXVERTS * 3 * sizeof(float));
  

	while (!exitswitch){
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)){
			switch (msg.message){
			case WM_QUIT:
				exitswitch = TRUE;
				break;
			default:
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}
		else if (active){
      center[0]=0.0f;
      center[1]=0.0f;
      center[2]=0.0f;

			//calculate attractor
			bool regularcycledetected = FALSE;
			if (reset){
				x = 0.1;
				y = 0.1;
        z = 0.1;
				reset = FALSE;
			}
			for (int i = 0; i < MAXVERTS; i++){

        /*
        t2 = clock();
        float dt = (float)t2-(float)t1;
        dt*=0.00001f;
        static double set0[] = {
          -0.2f,0.7f,-0.9f,0.7f,-0.4f,1.1f,0.2f,0.2f,0.0f,-0.5f,
          0.0f,0.3f,-0.9f,0.4f,0.4f,0.1f,0.5f,-0.7f,-0.2f,-0.9f,
          0.9f,-0.2f,-0.6f,0.0f,0.9f,-0.3,-1.0f,-0.5f,-0.8f,-0.3f
        };
        static double set1[] = {
          0.7f,-0.6f,-0.5f,0.1f,-1.1f,-0.7f,0.8f,-0.1f,-1.0f,0.1f,
          -0.7f,-0.3f,0.9f,0.1f,-0.4f,0.8f,0.5f,1.1f,0.4f,-0.3f,
          0.7f,0.9f,-0.8f,-0.8f,0.5f,0.1f,0.5f,0.7f,-0.7f-0.4f
        };

        float grad = (cos(dt)+1.0f)/2.0f;
        for(int i = 0; i < 30; i++){
          c[i] = set1[i]*grad+set0[i]*(1.0f-grad);
        }
        */

				//calculate next vertex
				xn = c[0] + c[1]*x + c[2]*x*x + c[3]*x*y + c[4]*y + c[5]*y*y + c[6]*z + c[7]*z*x + c[8]*z*y + c[9]*z*z;
				yn = c[10] + c[11]*x + c[12]*x*x + c[13]*x*y + c[14]*y + c[15]*y*y + c[16]*z + c[17]*z*x + c[18]*z*y + c[19]*z*z;
        zn = c[20] + c[21]*x + c[22]*x*x + c[23]*x*y + c[24]*y + c[25]*y*y + c[26]*z + c[27]*z*x + c[28]*z*y + c[29]*z*z;
				v[i * 3] = xn;
				v[i * 3 + 1] = yn;
        v[i * 3 + 2] = zn;

        center[0]+=xn;
        center[1]+=yn;
        center[2]+=zn;

				//calculate color for vertex
				float length = sqrt((xn - x)*(xn - x) + (yn - y)*(yn - y) + (zn - z)*(zn - z));
				//color[i * 3] = (float)(sin((xn - x)*COLORMULTIPLIER) / 2.0 + 0.5);
				//color[i * 3 + 1] = (float)(sin((yn - y)*COLORMULTIPLIER) / 2.0 + 0.5);
				//color[i * 3 + 2] = (float)(sin((yn - x)*COLORMULTIPLIER) / 2.0 + 0.5);
				//color[i * 3 + 2] = (float)(sin(length*COLORMULTIPLIER) / 2.0 + 0.5);
        
				color[i * 3] = sin(length*COLORMULTIPLIER+3.14*2.0/3.0) / 2.0 + 0.5;
				color[i * 3 + 1] = sin(length*COLORMULTIPLIER+3.14/3.0) / 2.0 + 0.5;
				color[i * 3 + 2] = sin(length*COLORMULTIPLIER) / 2.0 + 0.5;
        

				//detect cycle
        if(autosearch){
				  if (i % MAXCYCLE == 0 && i != 0){
					  int cyclelength;
					  int cyclecount = 0;
					  for (int j = i - 1; j > i - MAXCYCLE; j--){
						  double dx = v[i * 3] - v[j * 3];
						  double dy = v[i * 3 + 1] - v[j * 3 + 1];
              double dz = v[i * 3 + 2] - v[j * 3 + 2];
						  if (dx*dx + dy*dy + dz*dz< NEARTHRESHOLD){
							  if (cyclecount == 0){
								  cyclecount++;
								  cyclelength = i - j;
							  }
							  else if ((i - j) % cyclelength == 0){
								  cyclecount++;
								  if (cyclecount >CYCLETHRESHOLD){
									  regularcycledetected = TRUE;
									  break;
								  }
							  }
						  }
					  }
				  }
        }

        bool esacped = xn*xn + yn*yn > 4.0;

				if ( (esacped || regularcycledetected) && autosearch ){
					if (dir)seed++;
					else seed--;
					x = 0.1;
					y = 0.1;
          z = 0.1;
					gencoefficients(c, seed);
				}
				else{
					x = xn;
					y = yn;
          z = zn;
				}
			}

      if(regularcycledetected == FALSE){
        autosearch = false;
      }

      center[0]/=MAXVERTS;
      center[1]/=MAXVERTS;
      center[2]/=MAXVERTS;
	
			//draw attractor at full resolution
			glViewport(0, 0, width, height);
			glBindFramebuffer(GL_FRAMEBUFFER, framebuffer[0]);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture[0], 0);
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, renderbuffer[0]);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			glLoadIdentity();

			//draw attractor			
			glLoadIdentity();
			glUseProgram(0);
			glScalef(scale, scale*WIDTH / HEIGHT, 0.0);
      gluPerspective(1.0,WIDTH/HEIGHT,0.0001f,10000.0f);
			glTranslatef(viewx, viewy, -100.0);

      //calculate eye point 
      t2 = clock();
      float dt = (float)t2-(float)t1;
      dt*=0.0001f;
      float eye[3];
      
      eye[0]=center[0]+sin(dt)/scale;
      eye[1]=center[1];
      eye[2]=center[2]+cos(dt)/scale;
      
      //eye[0]=center[0];
      //eye[1]=center[1];
      //eye[2]=center[2]+1.0/scale;

      //view
      gluLookAt(eye[0],eye[1],eye[2],center[0],center[1],center[2],0.0f,1.0f,0.0f);

			//blend
			//glEnable(GL_BLEND);
			//glBlendEquation(GL_FUNC_ADD);
			//glBlendFunc(GL_ONE, GL_ONE);
			glEnableClientState(GL_VERTEX_ARRAY);
			glEnableClientState(GL_COLOR_ARRAY);
			glColorPointer(3, GL_FLOAT, 0, color);
			glVertexPointer(3, GL_FLOAT, 0, v);
			glDrawArrays(GL_POINTS, 0, MAXVERTS * 2 * sizeof(float) / sizeof(float) / 2);
			glDisableClientState(GL_VERTEX_ARRAY);
			glDisableClientState(GL_COLOR_ARRAY);
			//disable blend
			//glDisable(GL_BLEND);
			
      glLoadIdentity();
      
      glTranslatef(-0.95f,0.0f,0.0f);
      glScalef(0.03f,0.03f,0.03f);
      char text[1024];
      sprintf_s(text,"seed=%d\n",seed);
        drawtext(text);
      for(int i=0;i<30;i++){
        if(i == c_select){
          glColor3f(1.0f,0.0f,0.0f);
        }else{
          glColor3f(1.0f,1.0f,1.0f);
        }
        sprintf_s(text,"C[%2i]=%3.2f\n",i,c[i]);
        drawtext(text);
      }

			//draw horizontal glow
			glViewport(0, 0, glowwidth, glowheight);
			glBindFramebuffer(GL_FRAMEBUFFER, framebuffer[1]);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture[1], 0);
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, renderbuffer[1]);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, texture[0]);
			glLoadIdentity();
			glUseProgram(shaderprogram[1]);
			glUniform2f(glGetUniformLocation(shaderprogram[0], "resolution"), glowwidth, glowheight);
			drawquad();

			//draw vertical glow
			glViewport(0, 0, glowwidth, glowheight);
			glBindFramebuffer(GL_FRAMEBUFFER, framebuffer[2]);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture[2], 0);
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, renderbuffer[2]);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, texture[1]);
			glLoadIdentity();
			glUseProgram(shaderprogram[2]);
			glUniform2f(glGetUniformLocation(shaderprogram[0], "resolution"), glowwidth, glowheight);
			drawquad();

			//draw final scene
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			glViewport(0, 0, width, height);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			glLoadIdentity();
			//use passthrough program
			glUseProgram(shaderprogram[0]);
			glUniform2f(glGetUniformLocation(shaderprogram[0], "resolution"), width, height);
			//disable depth test
			glDisable(GL_DEPTH_TEST);
			glDepthFunc(GL_ALWAYS);
			//enable blend
			glEnable(GL_BLEND);
			glBlendEquation(GL_FUNC_ADD);
			glBlendFunc(GL_ONE, GL_ONE);
			//draw glow
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, texture[2]);
			drawquad();
			//draw main scene
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, texture[0]);
			drawquad();
			//disable blend
			glDisable(GL_BLEND);

      if(blur){
			  //blur frames
			  glAccum(GL_MULT, (GLfloat)(0.95));
			  glAccum(GL_ACCUM, (GLfloat)(1.0 - 0.95));
			  glAccum(GL_RETURN, (GLfloat)(1.0));
			  glFlush();
      }

			SwapBuffers(hdc);
		}
	}

	//kill window
	wglMakeCurrent(NULL, NULL);
	wglDeleteContext(hrc);
	ReleaseDC(hwnd, hdc);
	DestroyWindow(hwnd);
	UnregisterClass("Attractor", hinstance);

	//free frame buffer
	for (int i = 0; i < MAXBUFFER; i++){
		free(buffer[i]);
	}
	glDeleteFramebuffers(MAXBUFFER, framebuffer);
	glDeleteRenderbuffers(MAXBUFFER, renderbuffer);
	free(v);
	free(color);

	return (msg.wParam);
}

LRESULT CALLBACK wndprc(HWND hwnd, UINT umsg, WPARAM wparam, LPARAM lparam){
	switch (umsg){
	case WM_ACTIVATE:
		if (!HIWORD(wparam)){
			active = TRUE;
		}
		else{
			active = FALSE;
		}
		return 0;
	case WM_CLOSE:
		PostQuitMessage(0);
		return 0;
	case WM_KEYDOWN:
		keydown(wparam);
		key[wparam] = TRUE;
		return 0;
	case WM_KEYUP:
		keyup(wparam);
		key[wparam] = FALSE;
		return 0;
	}
	return DefWindowProc(hwnd, umsg, wparam, lparam);
}


void gencoefficients(double *c, int seed){
	srand(seed);
	for (int i = 0; i < 30; i++){
		c[i] = -1.2 + rand() % 24 * 0.1;
	}
}

void keydown(WPARAM key){
	switch (key){
	case VK_ESCAPE:
		exitswitch = TRUE;
		break;
	case VK_PRIOR:
		seed--;
		gencoefficients(c, seed);
		dir = 0;
		reset = TRUE;
    autosearch=true;
		break;
	case VK_NEXT:
		seed++;
		gencoefficients(c, seed);
		dir = 1;
		reset = TRUE;
    autosearch=true;
		break;
	case VK_UP:
		viewy -= 0.01 / scale;
		break;
	case VK_DOWN:
		viewy += 0.01 / scale;
		break;
	case VK_LEFT:
		viewx += 0.01 / scale;
		break;
	case VK_RIGHT:
		viewx -= 0.01 / scale;
		break;
	case VK_HOME:
		scale *= 1.01;
		break;
	case VK_END:
		scale *= 0.99;
		break;
	case VK_INSERT:
		scale = 0.5;
		viewx = 0.0;
		viewy = 0.0;
		break;
  case VK_DELETE:
    blur = !blur;
    break;
  case VK_OEM_MINUS:
    c_select = (30+c_select-1)%30;
    break;
  case VK_OEM_PLUS:
    c_select = (c_select+1)%30;
    break;
  case VK_OEM_4:
    c[c_select]-=0.1f;
    reset=TRUE;
    break;
  case VK_OEM_6:
    c[c_select]+=0.1f;
    reset=TRUE;
    break;
	}
}

void keyup(WPARAM key){
}

void drawquad(){
	static float quadverts[] = {
		-1.0, -1.0,
		-1.0, 1.0,
		1.0, 1.0,
		-1.0, -1.0,
		1.0, 1.0,
		1.0, -1.0
	};
	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer(2, GL_FLOAT, 0, quadverts);
	glDrawArrays(GL_TRIANGLES, 0, sizeof(quadverts) / sizeof(float) / 2);
	glDisableClientState(GL_VERTEX_ARRAY);
}