#include "loadstats.h"
#include "graphics.h"
#include "camera.h"
#include "common.h"
#include "config.h"
#include "window.h"
#include <stdio.h>
#include <curses.h>
#include <time.h>
#include <unistd.h>
#include <cmath>
#include "i2c_motor.h"

#include "GLES2/gl2.h"
#include "EGL/egl.h"
#include "EGL/eglext.h"

#define PI (3.141592653589793)

int dWidth, dHeight; //width and height of "dedonuted" image
bool gHaveI2C = false;
bool showWindow = false;
bool renderScreen = false;

int blue_x [30];
int blue_y [30];
int red_x [30];
int red_y [30];
int total_blue_x; 
int total_blue_y; 
int total_red_x; 
int total_red_y; 


#define UPDATERATE 10
#define MAX_COORDS 100

#define NUMKEYS 5
const char* keys[NUMKEYS] = {
	"arrow keys: move bot",
	"s: show snapshot window",
	"w: save framebuffers",
	"r: turn HDMI live rendering on/off",
	"q: quit"
};

void drawCurses(float fr, long nsec_curses, long nsec_readframe, long nsec_putframe, long nsec_draw, long nsec_getdata){
	updateStats();
	mvprintw(0,0,"Framerate: %g  ",fr);	
	mvprintw(2,0,"CPU Total: %d   ",cputot_stats.load);
	mvprintw(3,0,"CPU1: %d  ",cpu1_stats.load);
	mvprintw(4,0,"CPU2: %d  ",cpu2_stats.load);
	mvprintw(5,0,"CPU3: %d  ",cpu3_stats.load);
	mvprintw(6,0,"CPU4: %d  ",cpu4_stats.load);
	
	mvprintw(8,0,"Left Speed: %d     ",
		getDirection(LEFT_MOTOR)==FORWARD ? getSpeed(LEFT_MOTOR) : -1*((int)getSpeed(LEFT_MOTOR)));
	mvprintw(9,0,"Right Speed: %d     ",
		getDirection(RIGHT_MOTOR)==FORWARD ? getSpeed(RIGHT_MOTOR) : -1*((int)getSpeed(RIGHT_MOTOR)));
		
	if(gHaveI2C) mvprintw(11,0,"I2C ON  ");
	else mvprintw(11,0,"I2C FAIL");
	if(renderScreen) mvprintw(12,0, "HDMI ON     ");
	else mvprintw(12,0, "HDMI OFF     ");
	
	mvprintw(14,0,"msec Curses: %d   ",nsec_curses/1000000);
	mvprintw(15,0,"msec Readframe: %d   ",nsec_readframe/1000000);
	mvprintw(16,0,"msec Putframe: %d   ",nsec_putframe/1000000);
	mvprintw(17,0,"msec Draw: %d   ",nsec_draw/1000000);
	mvprintw(18,0,"msec Getdata: %d   ",nsec_getdata/1000000);
	
	mvprintw(0,30,"Controls:");
	int h=0;
	for(h=0; h<NUMKEYS; h++) mvprintw(h+3,30,keys[h]);
	
	//print the objects
	mvprintw(20,0, "Blue coordinates found: %d      ", total_blue_x+total_blue_y);
	for(int i=0; i<20; i++){
		if(i<total_blue_x) mvprintw(i+21,0, "X Blue object found: (%d,?)      ",blue_x[i]);
		else if(i<total_blue_y) mvprintw(i+21,0, "Y Blue object found: (?,%d)      ",blue_y[i-total_blue_x]);
		else mvprintw(i+21,0,"(not found)                                                 ");
	}
	mvprintw(20,40, "Red coordinates found: %d      ", total_red_x+total_red_y);
	for(int i=0; i<20; i++){
		if(i<total_red_x) mvprintw(i+21,40, "X Red object found: (%d,?)      ",red_x[i]);
		else if(i<total_red_y) mvprintw(i+21,40, "Y Red object found: (?,%d)      ",red_y[i-total_red_x]);
		else mvprintw(i+21,40,"(not found)                                                 ");
	}
	
	refresh();
}

void initDeDonutTextures(GfxTexture *targetmap){
	//this initializes the DeDonut texture, which is a texture that maps pixels of dedonuted to the donuted texture space (basically coordinate translation).

	//approximate dedonuted width based on circumference
	float widthf = (PI*g_conf.CAPTURE_WIDTH); 
	//approximate dedonuted height based on difference of radii of donut
	float heightf = ((g_conf.DONUTOUTERRATIO-g_conf.DONUTINNERRATIO)*g_conf.CAPTURE_WIDTH); 
	int width = (int)widthf;
	//Now a dirty way to get rid of the texture size limitation
	if(width>2048) width = 2048;
	int height = (int)heightf;	
	dWidth = width;
	dHeight = height;
	DBG("Dedonut: %dx%d", width, height);
	targetmap->CreateRGBA(width,height);
	targetmap->GenerateFrameBuffer();
	
	//now, generate the pixels
	unsigned char* data = (unsigned char*)calloc(1,4*(size_t)width*(size_t)height);
	int i,j;
	float x,y; //pixel
	float cx,cy; //center
	float rin,rout; //radii
	float d; //distance
	float dx, dy; //cartesian distances
	float phi; //polar angle to top
	float xout,yout; //mapping in floats (normalized)
	
	//donut parameters
	cx = g_conf.DONUTXRATIO;
	cy = g_conf.DONUTYRATIO;
	rin = g_conf.DONUTINNERRATIO;
	rout = g_conf.DONUTOUTERRATIO;
	for(i=0; i<width; i++)
		for(j=0; j<height; j++){
			//normalize coordinates
			x = ((float)i)/(float)width;
			y = ((float)j)/(float)height;
			//calculations
			phi = x*2*PI;
			d = (1-y)*(rout-rin)+rin;
			dx = d*sin(phi);
			dy = d*cos(phi);
			xout = cx+dx;
			yout = cy+dy;
			
			//store in pixel form
			data[(width*j+i)*4] = (int)(xout*255.0);
			data[(width*j+i)*4+1] = (int)(yout*255.0);
			data[(width*j+i)*4+3] = 255;
		}
	targetmap->SetPixels(data);
	free(data);	
	
	return;
}

int main(int argc, const char **argv)
{
	updateStats(); //baseline
	initConfig(); //get program settings
	if(startI2CMotor()) gHaveI2C = true;

	//init graphics and camera
	DBG("Start.");
	DBG("Initializing graphics and camera.");
	CCamera* cam = StartCamera(g_conf.CAPTURE_WIDTH, g_conf.CAPTURE_HEIGHT, g_conf.CAPTURE_FPS, 1, false); //camera
	InitGraphics();
	DBG("Camera resolution: %dx%d", g_conf.CAPTURE_WIDTH, g_conf.CAPTURE_HEIGHT);
	
	//set camera settings
	raspicamcontrol_set_metering_mode(cam->CameraComponent, METERINGMODE_AVERAGE);
	
	GfxTexture ytexture, utexture, vtexture, rgbtexture, rgblowtexture, thresholdtexture, thresholdlowtexture;
	GfxTexture dedonutmap, horsumtexture1, horsumtexture2, versumtexture1, versumtexture2, coordtexture;		
	GfxTexture horsumlowtexture1, versumlowtexture1, horsumlowtexture2, versumlowtexture2;
	DBG("Creating Textures.");
	//DeDonut RGB textures
	DBG("Max texture size: %d", GL_MAX_TEXTURE_SIZE);
	initDeDonutTextures(&dedonutmap);	
	//create YUV textures
	//these textures will store the image data coming from the camera.
	//the camera records in YUV color space. Each color component is stored in a SEPARATE buffer, which is why
	//we have three separate textures for them.
	ytexture.CreateGreyScale(g_conf.CAPTURE_WIDTH, g_conf.CAPTURE_HEIGHT, NULL, (GLfloat)GL_LINEAR,(GLfloat)GL_CLAMP_TO_EDGE);
	utexture.CreateGreyScale(g_conf.CAPTURE_WIDTH/2, g_conf.CAPTURE_HEIGHT/2, NULL, (GLfloat)GL_LINEAR,(GLfloat)GL_CLAMP_TO_EDGE);
	vtexture.CreateGreyScale(g_conf.CAPTURE_WIDTH/2, g_conf.CAPTURE_HEIGHT/2, NULL, (GLfloat)GL_LINEAR,(GLfloat)GL_CLAMP_TO_EDGE);
	//Main combined RGB textures
	//rgbtexture will hold the result of transforming the separate y,u,v textures into a single RGBA format texture.
	//in addition to that, rgbtexture is already unrolled into a wide panoramic image
	rgbtexture.CreateRGBA(dWidth,dHeight,NULL, (GLfloat)GL_NEAREST,(GLfloat)GL_CLAMP_TO_EDGE);
	rgbtexture.GenerateFrameBuffer();
	//thresholdtexture holds the result of performing thresholding (and possibly other filtering steps)
	thresholdtexture.CreateRGBA(dWidth,dHeight,NULL, (GLfloat)GL_NEAREST,(GLfloat)GL_CLAMP_TO_EDGE);
	thresholdtexture.GenerateFrameBuffer();
	//the textures below hold the summation results. It is a two-stage summation: therefore there are two textures for this with different sizes.
	//the two-stage approach is because of the GLSL limitation on Pi that allows only reading 64 pixels max per shader.
	horsumtexture1.CreateRGBA((dWidth/64)+1,dHeight,NULL, (GLfloat)GL_NEAREST,(GLfloat)GL_CLAMP_TO_EDGE);
	horsumtexture1.GenerateFrameBuffer();
	horsumtexture2.CreateRGBA(1,dHeight,NULL, (GLfloat)GL_NEAREST,(GLfloat)GL_CLAMP_TO_EDGE);
	horsumtexture2.GenerateFrameBuffer();
	versumtexture1.CreateRGBA(dWidth,(dHeight/64)+1,NULL, (GLfloat)GL_NEAREST,(GLfloat)GL_CLAMP_TO_EDGE);
	versumtexture1.GenerateFrameBuffer();
	versumtexture2.CreateRGBA(dWidth,1,NULL, (GLfloat)GL_NEAREST,(GLfloat)GL_CLAMP_TO_EDGE);
	versumtexture2.GenerateFrameBuffer();
	//will be deleted: nvm.
	coordtexture.CreateRGBA(MAX_COORDS, 1,NULL, (GLfloat)GL_NEAREST,(GLfloat)GL_CLAMP_TO_EDGE);
	coordtexture.GenerateFrameBuffer();
	

	//showWindowd RGB textures
	//these textures are just low-resolution versions of the above. they are used when we want to get a preview window: transfering the
	//full-size textures takes too much time so they are first down-sampled.
	float lowhf = ((float)dHeight/(float)dWidth);
	int lowh = (int)(lowhf * g_conf.LOWRES_WIDTH);
	if(!lowh) lowh = 1;
	rgblowtexture.CreateRGBA(g_conf.LOWRES_WIDTH, lowh,NULL, (GLfloat)GL_LINEAR,(GLfloat)GL_CLAMP_TO_EDGE);
	rgblowtexture.GenerateFrameBuffer();
	thresholdlowtexture.CreateRGBA(g_conf.LOWRES_WIDTH, lowh,NULL, (GLfloat)GL_LINEAR,(GLfloat)GL_CLAMP_TO_EDGE);
	thresholdlowtexture.GenerateFrameBuffer();
	horsumlowtexture1.CreateRGBA(200,lowh,NULL, (GLfloat)GL_LINEAR,(GLfloat)GL_CLAMP_TO_EDGE);
	horsumlowtexture1.GenerateFrameBuffer();
	horsumlowtexture2.CreateRGBA(20,lowh,NULL, (GLfloat)GL_LINEAR,(GLfloat)GL_CLAMP_TO_EDGE);
	horsumlowtexture2.GenerateFrameBuffer();
	versumlowtexture1.CreateRGBA(g_conf.LOWRES_WIDTH,100,NULL, (GLfloat)GL_LINEAR,(GLfloat)GL_CLAMP_TO_EDGE);
	versumlowtexture1.GenerateFrameBuffer();
	versumlowtexture2.CreateRGBA(g_conf.LOWRES_WIDTH,20,NULL, (GLfloat)GL_LINEAR,(GLfloat)GL_CLAMP_TO_EDGE);
	versumlowtexture2.GenerateFrameBuffer();
	
	
	//Start the processing loop.
	DBG("Starting process loop.");
	
	long int start_time;
	double total_frameset_time_s=0;
	long int time_difference;
	struct timespec gettime_now;
	clock_gettime(CLOCK_REALTIME, &gettime_now);
	start_time = gettime_now.tv_nsec ;
	double total_time_s = 0;
	bool do_pipeline = false;
	
	struct timespec t_start, t_curses, t_readframe, t_putframe, t_draw, t_getdata;
	long int nsec_curses, nsec_readframe, nsec_putframe, nsec_draw, nsec_getdata;
	
	struct timespec t_up, t_down, t_left, t_right;
	long int nsec_up, nsec_down, nsec_left, nsec_right;

	initscr();      /* initialize the curses library */
	keypad(stdscr, TRUE);  /* enable keyboard mapping */
	nonl();         /* tell curses not to do NL->CR/NL on output */
	cbreak();       /* take input chars one at a time, no wait for \n */
	clear();
	nodelay(stdscr, TRUE);
	
	clock_gettime(CLOCK_REALTIME, &t_up);
	t_down=t_left=t_right=t_up;

	for(long i = 0;; i++)
	{
		clock_gettime(CLOCK_REALTIME, &t_start);
		
		if(showWindow){
			SDL_Rect inrect = {0, 0, g_conf.LOWRES_WIDTH, lowh};
			SDL_Rect thresholdrect = {0, lowh+200, g_conf.LOWRES_WIDTH, lowh};
			SDL_Rect versum1rect = {0,lowh+90,g_conf.LOWRES_WIDTH,100};
			SDL_Rect versum2rect = {0,lowh+50,g_conf.LOWRES_WIDTH,20};
			SDL_Rect horsum1rect = {g_conf.LOWRES_WIDTH,lowh+200,200,lowh};
			SDL_Rect horsum2rect = {g_conf.LOWRES_WIDTH+240,lowh+200,20,lowh};
			
			rgblowtexture.Show(&inrect);
			thresholdlowtexture.Show(&thresholdrect);
			if(showWindow){
				versumlowtexture1.Show(&versum1rect);
				versumlowtexture2.Show(&versum2rect);
				horsumlowtexture1.Show(&horsum1rect);
				horsumlowtexture2.Show(&horsum2rect);
			}
			
			showWindow = false;
		}

		int ch = getch();
		if(ch==ERR){ //no keypress
			gHaveI2C = setSpeed(RIGHT_MOTOR,0);
			gHaveI2C = setSpeed(LEFT_MOTOR, 0);
		}
		else{
			while(ch != ERR)
			{
				struct timespec temp;
				long diff;
				switch(ch){
				case KEY_LEFT:					
					clock_gettime(CLOCK_REALTIME, &temp);
					diff = temp.tv_nsec - t_left.tv_nsec;
					if(diff < 0) diff += 1000000000;
					t_left = temp;
					if(diff<=60000000){
						gHaveI2C = setSpeedDir(LEFT_MOTOR, BACKWARD, MAX_SPEED);
						gHaveI2C = setSpeedDir(RIGHT_MOTOR,FORWARD, MAX_SPEED);
					}
					break;
				case KEY_UP:
					clock_gettime(CLOCK_REALTIME, &temp);
					diff = temp.tv_nsec - t_up.tv_nsec;
					if(diff < 0) diff += 1000000000;
					t_up = temp;
					if(diff<=60000000){
						gHaveI2C = setSpeedDir(RIGHT_MOTOR,FORWARD, MAX_SPEED);
						gHaveI2C = setSpeedDir(LEFT_MOTOR, FORWARD, MAX_SPEED);
					}
					break;
				case KEY_DOWN:
					clock_gettime(CLOCK_REALTIME, &temp);
					diff = temp.tv_nsec - t_down.tv_nsec;
					if(diff < 0) diff += 1000000000;
					t_down = temp;
					if(diff<=60000000){
						gHaveI2C = setSpeedDir(RIGHT_MOTOR,BACKWARD, MAX_SPEED);
						gHaveI2C = setSpeedDir(LEFT_MOTOR, BACKWARD, MAX_SPEED);
					}
					break;
				case KEY_RIGHT:
					clock_gettime(CLOCK_REALTIME, &temp);
					diff = temp.tv_nsec - t_right.tv_nsec;
					if(diff < 0) diff += 1000000000;
					t_right = temp;
					if(diff<=60000000){
						gHaveI2C = setSpeedDir(RIGHT_MOTOR, BACKWARD, MAX_SPEED);
						gHaveI2C = setSpeedDir(LEFT_MOTOR,FORWARD, MAX_SPEED);
					}
					break;
				case 's': //save framebuffers
					showWindow = true;
					break;
				case 'w': //save framebuffers
					//SaveFrameBuffer("tex_fb.png");
					rgbtexture.Save("./captures/tex_rgb.png");
					thresholdtexture.Save("./captures/tex_out.png");
					horsumtexture2.Save("./captures/tex_hor.png");
					versumtexture2.Save("./captures/tex_ver.png");
					break;
				case 'r': //rendering on/off_type
					if(renderScreen) renderScreen = false;
					else renderScreen = true;
					break;
				case 'q': //quit
					endwin();
					exit(1);
				}
	
				move(0,0);
				refresh();
				ch = getch();
			}
		}
		
		clock_gettime(CLOCK_REALTIME, &t_curses);
		
		//spin until we have a camera frame
		const void* frame_data; int frame_sz;
		//the camera is set to a certain framerate. Therefore, by the time we get here, the next frame may not be ready yet.
		//therefore this while loop loops until a frame is ready, and when it is,  grabs it and puts it in the frame_data buffer.
		while(!cam->BeginReadFrame(0,frame_data,frame_sz)) {usleep(500);};
		clock_gettime(CLOCK_REALTIME, &t_readframe);
		//lock the chosen frame buffer, and copy it directly into the corresponding open gl texture
		const uint8_t* data = (const uint8_t*)frame_data;
		int ypitch = g_conf.CAPTURE_WIDTH;
		int ysize = ypitch*g_conf.CAPTURE_HEIGHT;
		int uvpitch = g_conf.CAPTURE_WIDTH/2;
		int uvsize = uvpitch*g_conf.CAPTURE_HEIGHT/2;
		//int upos = ysize+16*uvpitch;
		//int vpos = upos+uvsize+4*uvpitch;
		int upos = ysize;
		int vpos = upos+uvsize;
		//printf("Frame data len: 0x%x, ypitch: 0x%x ysize: 0x%x, uvpitch: 0x%x, uvsize: 0x%x, u at 0x%x, v at 0x%x, total 0x%x\n", frame_sz, ypitch, ysize, uvpitch, uvsize, upos, vpos, vpos+uvsize);		
		//now we have the frame from the camera in the CPU memory in the 'data'  buffer. Now we have to upload it to the GPU as textures using the
		//SetPixels() function provided by the OpenGL API.
		ytexture.SetPixels(data);
		utexture.SetPixels(data+upos);
		vtexture.SetPixels(data+vpos);
		cam->EndReadFrame(0);
		
		clock_gettime(CLOCK_REALTIME, &t_putframe);
		
		
		
		//begin frame: a call from the graphics.h/cpp module that starts the rendering pipeline for this frame.
		BeginFrame();
			
		DrawYUVTextureRect(&ytexture,&utexture,&vtexture,&dedonutmap,-1.f,-1.f,1.f,1.f,&rgbtexture);
		
		//make thresholded
		DrawThresholdRect(&rgbtexture, -1.0f, -1.0f, 1.0f, 1.0f, &thresholdtexture); 
		
		//sum rows/columns
		DrawHorSum1(&thresholdtexture, -1.0f, -1.0f, 1.0f, 1.0f, &horsumtexture1);
		DrawHorSum2(&horsumtexture1, -1.0f, -1.0f, 1.0f, 1.0f, &horsumtexture2);
		DrawVerSum1(&thresholdtexture, -1.0f, -1.0f, 1.0f, 1.0f, &versumtexture1);
		DrawVerSum2(&versumtexture1, -1.0f, -1.0f, 1.0f, 1.0f, &versumtexture2);
		
		//showWindow for preview window on this frame
		if(showWindow){
			DrawTextureRect(&rgbtexture, -1.0f, -1.0f, 1.0f, 1.0f, &rgblowtexture);
			DrawTextureRect(&thresholdtexture, -1.0f, -1.0f, 1.0f, 1.0f, &thresholdlowtexture);
			DrawTextureRect(&horsumtexture1, -1.0f, -1.0f, 1.0f, 1.0f, &horsumlowtexture1);
			DrawTextureRect(&horsumtexture2, -1.0f, -1.0f, 1.0f, 1.0f, &horsumlowtexture2);
			DrawTextureRect(&versumtexture1, -1.0f, -1.0f, 1.0f, 1.0f, &versumlowtexture1);
			DrawTextureRect(&versumtexture2, -1.0f, -1.0f, 1.0f, 1.0f, &versumlowtexture2);
		}
		
		if(renderScreen){
			DrawTextureRect(&rgbtexture, 0.8f, 1.0f, -1.0f, 0.2f, NULL);
			DrawTextureRect(&thresholdtexture, 0.8f, -0.2f, -1.0f, -1.0f, NULL);
			DrawTextureRect(&horsumtexture1, 0.95f, -0.2f, 0.8f, -1.0f,  NULL);
			DrawTextureRect(&horsumtexture2, 1.0f, -0.2f, 0.95f, -1.0f, NULL);
			DrawTextureRect(&versumtexture1, 0.8f, 0.0f, -1.0f, -0.2f, NULL);
			DrawTextureRect(&versumtexture2, 0.8f, 0.15f, -1.0f, 0.05f, NULL);
		}
		
		EndFrame();
		
		clock_gettime(CLOCK_REALTIME, &t_draw);
		
		//get summed data back to host (CPU side)
		versumtexture2.Get();
		horsumtexture2.Get();
		versumtexture1.Get();
		horsumtexture1.Get();
		
		struct centroid{
			int x;
			int y;
		};

		struct centroid c_red[30];
		struct centroid c_blue[30];

		int a;
		int a1,a2, sum_blue, sum_red;
		int x_bhigh[15];
		int x_blow[15];
		int x_rhigh[15];
		int x_rlow[15];
		int total_red_x = 0; 
		int set_blue = 1;
		int set_red =1;
		int k_blue=0;
		int k_red=0;
		total_blue_x = total_red_x = 0;
		unsigned char * verptr = (unsigned char*)versumtexture2.image;
		for(int j=0; j< (versumtexture2.Width); j++)
		{   
			a1 = verptr[j*4 +2];
			if (set_blue ==1)
			{
		       if(a1>0)
			   {   
			    x_bhigh[k_blue] =j;
			    set_blue =0;
               }
		     }
			if (set_blue ==0)
			{
			if ((a1==0)||(j == (versumtexture2.Width) -1))
			{   
			    total_blue_x++;
			    x_blow[k_blue] =j;
			    set_blue =1;
               
			    sum_blue = (x_bhigh[k_blue] + ( x_blow[k_blue] -x_bhigh[k_blue] )/2);
				blue_x[k_blue]=sum_blue;
				//printf(" * x_blue[%d]  = %d\n", total_blue_x,sum);
				k_blue++;
		     }
			}
			  
			//red x coordinates
			a2 = verptr[j*4];
			if (set_red ==1)
			{
		       if(a2>0)
			   {   
			    x_rhigh[k_red] =j;
			    set_red =0;
               }
		     }
			if (set_red ==0)
			{
			if ((a2==0)||(j == (versumtexture2.Width) -1))
			{   
			    total_red_x++;
			    x_rlow[k_red] =j;
			    set_red =1;
               
			    sum_red = (x_rhigh[k_red] + ( x_rlow[k_red] -x_rhigh[k_red] )/2);
				red_x[k_red]=sum_red;
				//printf(" * x_blue[%d]  = %d\n", total_blue_x,sum);
				k_red++;
		     }

		}
	}
		
		
		
		
		
		set_blue = 1;
		set_red =1;
		k_blue=0;
		k_red=0;
		int y_bhigh[15];
		int y_blow[15];
		int y_rhigh[15];
		int y_rlow[15];
		total_red_y = 0; 
		total_blue_y= 0; 
		unsigned char * horptr = (unsigned char*)horsumtexture2.image;
		for(int j=0; j< (horsumtexture2.Height); j++)
		{
			
			a1 = horptr[j*4 +2];;
			if (set_blue ==1)
			{
		       if(a1>0)
			   {   
			    
			    y_bhigh[k_blue] =i;
			    set_blue =0;
               }
		     }

			if (set_blue ==0)
			{
			if ((a1==0)||(i == (horsumtexture2.Height)-1))
			{   
			    total_blue_y++;
			    y_blow[k_blue] =i;
			    set_blue =1;
			    sum_blue = (y_bhigh[k_blue] + ( y_blow[k_blue] - y_bhigh[k_blue] )/2);
				blue_y[k_blue] =sum_blue;
				//printf(" * y_blue[%d]  = %d\n", total_blue_y,sum);
				k_blue++;
		     }

	     	}
			 
			//red 
			a2 = horptr[j*4];;
			if (set_red ==1)
			{
		       if(a2>0)
			   {   
			    
			    y_rhigh[k_red] =i;
			    set_red =0;
               }
		     }

			if (set_red ==0)
			{
			if ((a2==0)||(i == (horsumtexture2.Height)-1))
			{   
			    total_red_y++;
			    y_rlow[k_red] =i;
			    set_red =1;
			    sum_red = (y_rhigh[k_red] + ( y_rlow[k_red] - y_rhigh[k_red] )/2);
				red_y[k_red] =sum_red;
				//printf(" * y_blue[%d]  = %d\n", total_blue_y,sum);
				k_red++;
		     }

	     	}
		}

		
		clock_gettime(CLOCK_REALTIME, &t_getdata);
		
		//benchmarking
		nsec_curses = t_curses.tv_nsec - t_start.tv_nsec;
		nsec_readframe = t_readframe.tv_nsec - t_curses.tv_nsec;
		nsec_putframe = t_putframe.tv_nsec - t_readframe.tv_nsec;
		nsec_draw = t_draw.tv_nsec - t_putframe.tv_nsec;
		nsec_getdata = t_getdata.tv_nsec - t_draw.tv_nsec;
		if(nsec_curses < 0) nsec_curses += 1000000000;
		if(nsec_readframe < 0) nsec_readframe += 1000000000;
		if(nsec_putframe < 0) nsec_putframe += 1000000000;
		if(nsec_draw < 0) nsec_draw += 1000000000;
		if(nsec_getdata < 0) nsec_getdata += 1000000000;
		
		//read current time
		clock_gettime(CLOCK_REALTIME, &gettime_now);
		time_difference = gettime_now.tv_nsec - start_time;
		if(time_difference < 0) time_difference += 1000000000;
		total_time_s += double(time_difference)/1000000000.0;
		total_frameset_time_s += double(time_difference)/1000000000.0;
		start_time = gettime_now.tv_nsec;
		float fr = float(double(UPDATERATE)/total_frameset_time_s);
		
		
	
		//print the screen
		if((i%UPDATERATE)==0)
		{			//draw the terminal window
			drawCurses(fr, nsec_curses, nsec_readframe, nsec_putframe, nsec_draw, nsec_getdata);
			total_frameset_time_s = 0;
		}

	}

	StopCamera();

	endwin();
	
	return 0;
}
