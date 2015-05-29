#ifndef __COMMON_H__
#define __COMMON_H__

#include <stdio.h>
#include "camera.h"
#include "mmalincludes.h"

#define NUMDBG 10

extern FILE * logfile;
extern char messages[NUMDBG][300];
extern unsigned int msgi;

#define DBG(...) {sprintf(messages[msgi%NUMDBG], __VA_ARGS__); msgi++; fprintf(logfile, __VA_ARGS__); fprintf(logfile, "\n");}
#define OPENLOG logfile = fopen("log.txt", "w")
#define CLOSELOG fclose(logfile)

#define MAINSHADER "./shaders/out.glsl"

//#define METERING_MODE METERINGMODE_AVERAGE
//#define METERING_MODE METERINGMODE_BACKLIT

#define EXPOSURE_OFF 				MMAL_PARAM_EXPOSUREMODE_OFF
#define EXPOSURE_AUTO 				MMAL_PARAM_EXPOSUREMODE_AUTO
#define EXPOSURE_NIGHT 				MMAL_PARAM_EXPOSUREMODE_NIGHT
#define EXPOSURE_NIGHTPREVIEW 		MMAL_PARAM_EXPOSUREMODE_NIGHTPREVIEW
#define EXPOSURE_BACKLIGHT 			MMAL_PARAM_EXPOSUREMODE_BACKLIGHT
#define EXPOSURE_SPOTLIGHT 			MMAL_PARAM_EXPOSUREMODE_SPOTLIGHT
#define EXPOSURE_SPORTS 			MMAL_PARAM_EXPOSUREMODE_SPORTS
#define EXPOSURE_SNOW 				MMAL_PARAM_EXPOSUREMODE_SNOW
#define EXPOSURE_BEACH 				MMAL_PARAM_EXPOSUREMODE_BEACH
#define EXPOSURE_VERYLONG 			MMAL_PARAM_EXPOSUREMODE_VERYLONG
#define EXPOSURE_FIXEDFPS 			MMAL_PARAM_EXPOSUREMODE_FIXEDFPS
#define EXPOSURE_ANTISHAKE 			MMAL_PARAM_EXPOSUREMODE_ANTISHAKE
#define EXPOSURE_FIREWORKS 			MMAL_PARAM_EXPOSUREMODE_FIREWORKS

#define NUM_EXPOSURE 13
extern int current_Exposure;
extern const char* Exposure_Name_Enum[NUM_EXPOSURE];
extern const MMAL_PARAM_EXPOSUREMODE_T Exposure_Enum[NUM_EXPOSURE];

#define AWB_OFF 			MMAL_PARAM_AWBMODE_OFF
#define AWB_AUTO 			MMAL_PARAM_AWBMODE_AUTO
#define AWB_SUNLIGHT 		MMAL_PARAM_AWBMODE_SUNLIGHT
#define AWB_CLOUDY 			MMAL_PARAM_AWBMODE_CLOUDY
#define AWB_SHADE 			MMAL_PARAM_AWBMODE_SHADE
#define AWB_TUNGSTEN 		MMAL_PARAM_AWBMODE_TUNGSTEN
#define AWB_FLUORESCENT 	MMAL_PARAM_AWBMODE_FLUORESCENT
#define AWB_INCANDESCENT 	MMAL_PARAM_AWBMODE_INCANDESCENT
#define AWB_FLASH 			MMAL_PARAM_AWBMODE_FLASH
#define AWB_HORIZON 		MMAL_PARAM_AWBMODE_HORIZON

#define NUM_AWB 10
extern int current_AWB;
extern const char* AWB_Name_Enum[NUM_AWB];
extern const MMAL_PARAM_AWBMODE_T AWB_Enum[NUM_AWB];

#define FX_NONE MMAL_PARAM_IMAGEFX_NONE
#define FX_NEGATIVE MMAL_PARAM_IMAGEFX_NEGATIVE
#define FX_SOLARIZE MMAL_PARAM_IMAGEFX_SOLARIZE
#define FX_SKETCH MMAL_PARAM_IMAGEFX_SKETCH
#define FX_DENOISE MMAL_PARAM_IMAGEFX_DENOISE
#define FX_EMBOSS MMAL_PARAM_IMAGEFX_EMBOSS
#define FX_OILPAINT MMAL_PARAM_IMAGEFX_OILPAINT
#define FX_HATCH MMAL_PARAM_IMAGEFX_HATCH
#define FX_GPEN MMAL_PARAM_IMAGEFX_GPEN
#define FX_PASTEL MMAL_PARAM_IMAGEFX_PASTEL
#define FX_WATERCOLOUR MMAL_PARAM_IMAGEFX_WATERCOLOUR
#define FX_FILM MMAL_PARAM_IMAGEFX_FILM
#define FX_BLUR MMAL_PARAM_IMAGEFX_BLUR
#define FX_SATURATION MMAL_PARAM_IMAGEFX_SATURATION
#define FX_COLOURSWAP MMAL_PARAM_IMAGEFX_COLOURSWAP
#define FX_WASHEDOUT MMAL_PARAM_IMAGEFX_WASHEDOUT
#define FX_POSTERIZE MMAL_PARAM_IMAGEFX_POSTERISE
#define FX_COLOURPOINT MMAL_PARAM_IMAGEFX_COLOURPOINT
#define FX_COLOURBALANCE MMAL_PARAM_IMAGEFX_COLOURBALANCE
#define FX_CARTOON MMAL_PARAM_IMAGEFX_CARTOON

#define NUM_FX 20
extern int current_FX;
extern const char* FX_Name_Enum[NUM_FX];
extern const int FX_Enum[NUM_FX];

#define METERINGMODE_AVERAGE 	MMAL_PARAM_EXPOSUREMETERINGMODE_AVERAGE
#define METERINGMODE_SPOT 		MMAL_PARAM_EXPOSUREMETERINGMODE_SPOT
#define METERINGMODE_BACKLIT 	MMAL_PARAM_EXPOSUREMETERINGMODE_BACKLIT
#define METERINGMODE_MATRIX 	MMAL_PARAM_EXPOSUREMETERINGMODE_MATRIX

#define NUM_METERINGMODE 4
extern int current_MeteringMode;
extern const char* MeteringMode_Name_Enum[NUM_METERINGMODE];
extern const MMAL_PARAM_EXPOSUREMETERINGMODE_T MeteringMode_Enum[NUM_METERINGMODE];

#endif
