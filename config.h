#ifndef _CONFIG_H_
#define _CONFIG_H_

typedef struct config_t{
	bool ready;
	float DONUTXRATIO;
	float DONUTYRATIO;
	float DONUTINNERRATIO;
	float DONUTOUTERRATIO;
}config_t;

extern config_t g_conf;

void initConfig(void);
void writeConfig(void);

#endif