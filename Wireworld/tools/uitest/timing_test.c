/* Scheduler rate, pause/speed, midnight rollover, and backlog tests. */
#define main ww_main
#include "../../src/main.c"
#undef main
#include <stdio.h>
int host_key_down(int key) { (void)key; return 0; }
void host_putdisp(void) { }
void host_tick(void) { }
int host_os_menu(void) { return 0; }
extern void host_set_clock_ms(long value);
static int tests, fails;
static void check(int good, const char *label)
{
    tests++;
    if(!good) { fails++; fprintf(stderr,"FAIL %s\n",label); }
}
int main(void)
{
    static const int rates[]={1,2,5,10,20,30,60};
    int i, tick;
    for(i=0;i<(int)(sizeof(rates)/sizeof(rates[0]));i++) {
        SpeedClock c={0,0,0,1}; int total=0;
        for(tick=0;tick<=10*128;tick++) total+=speed_clock_due(&c,1,rates[i],tick);
        if(total!=1+rates[i]*10)
            fprintf(stderr,"rate %d: got %d steps, expected %d\n",rates[i],total,1+rates[i]*10);
        check(total==1+rates[i]*10,"10-second exact rate");
    }
    {
        SpeedClock c={0,0,0,1}; int total=0;
        for(tick=0;tick<50;tick++) total+=speed_clock_due(&c,1,0,tick);
        check(total==50,"MAX allows every iteration");
        check(!speed_clock_due(&c,0,0,50),"paused");
        check(speed_clock_due(&c,1,1,50),"resume takes immediate step");
        for(tick=51;tick<178;tick++)
            check(!speed_clock_due(&c,1,1,tick),"no early 1/s step");
        check(speed_clock_due(&c,1,1,178),"1/s exactly one second later");
        check(!speed_clock_due(&c,1,60,178),"switch speed without extra step");
        check(!speed_clock_due(&c,1,60,180),"60/s uses fractional tick credit");
        check(speed_clock_due(&c,1,60,181),"60/s advances after 3 ticks");
        check(speed_clock_due(&c,1,60,500),"long menu / slow generation advances once");
        check(!speed_clock_due(&c,1,60,500),"no unbounded catch-up burst");
    }
    check(rtc_ticks_between((int)RTC_DAY_TICKS-3,2)==5,"midnight tick delta");
    {
        SpeedClock c={(int)RTC_DAY_TICKS-2,60,0,0};
        check(speed_clock_due(&c,1,60,2),"scheduler crosses midnight");
        check(c.credit==112,"scheduler midnight credit");
    }
    {
        int start;
        host_set_clock_ms(86400000L-30);
        start=RTC_GetTicks();
        host_set_clock_ms(86400000L+20);
        check(rtc_elapsed_ms(start,40),"elapsed timer crosses midnight");
        check(!rtc_elapsed_ms(start,70),"elapsed timer not early at midnight");
    }
    printf("%d scheduler/RTC checks, %d failures\n",tests,fails);
    return fails?1:0;
}
