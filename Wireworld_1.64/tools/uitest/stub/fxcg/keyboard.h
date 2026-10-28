#ifndef STUB_FXCG_KEYBOARD_H
#define STUB_FXCG_KEYBOARD_H
#define KEY_PRGM_DOWN 37
#define KEY_PRGM_EXIT 47
#define KEY_PRGM_F1 79
#define KEY_PRGM_F2 69
#define KEY_PRGM_F3 59
#define KEY_PRGM_F4 49
#define KEY_PRGM_F5 39
#define KEY_PRGM_F6 29
#define KEY_PRGM_LEFT 38
#define KEY_PRGM_RETURN 31
#define KEY_PRGM_RIGHT 27
#define KEY_PRGM_UP 28
#define KEY_PRGM_0 71
#define KEY_PRGM_1 72
#define KEY_PRGM_2 62
#define KEY_PRGM_3 52
#define KEY_PRGM_4 73
#define KEY_PRGM_5 63
#define KEY_PRGM_6 53
#define KEY_PRGM_7 74
#define KEY_PRGM_8 64
#define KEY_PRGM_9 54
#define KEY_PRGM_ALPHA 77
#define KEY_PRGM_SHIFT 78
#define KEY_PRGM_OPTN 68
#define KEY_PRGM_MENU 48

#define KEYWAIT_HALTON_TIMEROFF     0
#define KEYWAIT_HALTOFF_TIMEROFF    1
#define KEYWAIT_HALTON_TIMERON      2

#define KEYREP_NOEVENT              0
#define KEYREP_KEYEVENT             1
#define KEYREP_TIMEREVENT           2

int GetKey(int *key);
int GetKeyWait_OS(int *column, int *row, int type_of_waiting,
                  int timeout_period, int menu, unsigned short *keycode);
void Bkey_ClrAllFlags(void);
void EnableGetkeyToMainFunctionReturn(void);
int  GetGetkeyToMainFunctionReturnFlag(void);
void Set_FKeys1(unsigned int p1, unsigned int *p2);
void Set_FKeys2(unsigned int p1);
int Keyboard_PutKeycode(int X, int Y, int Keycode);
#define KEY_CTRL_MENU 30003
#endif
