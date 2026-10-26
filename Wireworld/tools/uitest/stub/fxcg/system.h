#ifndef STUB_FXCG_SYSTEM_H
#define STUB_FXCG_SYSTEM_H
void OS_InnerWait_ms(int ms);
int Timer_Install(int InternalTimerID, void (*handler)(void), int elapse);
int Timer_Deinstall(int InternalTimerID);
int Timer_Start(int InternalTimerID);
int Timer_Stop(int InternalTimerID);
#endif
