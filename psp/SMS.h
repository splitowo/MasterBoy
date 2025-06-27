// Not much in here at the moment
/*typedef struct 
{
	char GameName[100];
	int SoundEnabled;
	int FullScreen;
	int WifiEnabled;
	int Server;
} sms_config;

sms_config SmsConfig;*/

void SmsEmulate();

void system_manage_sram(uint8 *sram, int slot, int mode);

extern int frameReady, gblFramerate, gblVirtualFramerate;

enum {EM_SMS, EM_GBC};
extern char gblMachineType;
extern char gblNewGame;

void gbe_reset();
void gbe_init();
void gbe_updatePad();
void machine_reset();
void machine_frame(int skip);
//Does a game boy frame
void gb_doFrame(int skip);


