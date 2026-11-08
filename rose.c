
void setup_display0x2dtrace(void);
void state_setup(void);
void setup_list0x2drescale(void);
void getenv_setup(void);

void rose_setup(void)
{
	setup_display0x2dtrace();
	state_setup();
	setup_list0x2drescale();
	getenv_setup();
}
