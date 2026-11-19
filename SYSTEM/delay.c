#include "delay.h"
#include "sys.h"
 
/*
//roughly delay
void delay_us(u32 nus)
{		
	u32 i;
	u16 temp;
	temp = nus*SYS_CLK/15;
	for(i=0;i<temp;i++);
}

//延时nms
//nms:要延时的ms�?
void delay_ms(u16 nms)
{
	u32 i;
	for(i=0;i<nms;i++) delay_us(1000);
}
*/

/*----------------------------------------------------------------------------
 * DWT 周期计数器使能（delay_us �? HAL_GetTick 共用，避免两处重�?)
 * DWT->CYCCNT �?�? HCLK 在跑就在递�?�，不需要任何中�?，所以在�?�?关闭�?
 * 调度器还没启动的情况下照样能用�?
 *----------------------------------------------------------------------------*/
static void delay_dwt_enable(void)
{
	if ((CoreDebug->DEMCR & CoreDebug_DEMCR_TRCENA_Msk) == 0U)
	{
		CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
	}
	if ((DWT->CTRL & DWT_CTRL_CYCCNTENA_Msk) == 0U)
	{
		DWT->CYCCNT = 0U;
		DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
	}
}

void delay_init(void)
{
	HAL_SYSTICK_CLKSourceConfig(SYSTICK_CLKSOURCE_HCLK);
	/* Configure the SysTick to have interrupt in 1ms time basis*/
	HAL_SYSTICK_Config(SystemCoreClock / (1000U / uwTickFreq));
}

#if OS_SUPPORT 						    								   	   
void delay_us(u32 nus)
{		
	u32 ticks;
	u32 told,tnow,tcnt=0;
	u32 reload=SysTick->LOAD;				   	 
	ticks=nus*SYS_CLK; 						 
	delay_osschedlock();					
	told=SysTick->VAL;        				
	while(1)
	{
		tnow=SysTick->VAL;	
		if(tnow!=told)
		{	    
			if(tnow<told)tcnt+=told-tnow;//这里注意一下SYSTICK�?一�?递减的�?�数器就�?以了.	
			else tcnt+=reload-tnow+told;	    
			told=tnow;
			if(tcnt>=ticks)break;			
		}  
	};
	delay_osschedunlock();															    
}  

void delay_ms(u16 nms)
{	
	if(delay_osrunning&&delay_osintnesting==0)    
	{		 
		if(nms>=fac_ms)						
		{ 
   			delay_ostimedly(nms/fac_ms);	
		}
		nms%=fac_ms;						 
	}
	delay_us((u32)(nms*1000));			
}
#else  
	 
void delay_us(u32 nus)
{		
	/* DWT cycle-counter based delay.
	   Works both before and after the RTOS scheduler starts.  The old
	   SysTick->VAL version spins forever when SysTick is not running yet
	   (e.g. when called from main() before osKernelStart). */
	u32 start, ticks;

	delay_dwt_enable();

	ticks = nus * (SystemCoreClock / 1000000U);
	start = DWT->CYCCNT;
	while ((DWT->CYCCNT - start) < ticks) {
	}
}

void delay_ms(u16 nms)
{
	u32 i;
	for(i=0;i<nms;i++) delay_us(1000);
}
#endif

void HAL_Delay(uint32_t Delay)
{
	while (Delay > 0U)
	{
		/* delay_ms() 的入参是 u16，超�? 65535 就分�? */
		u16 chunk = (Delay > 1000U) ? 1000U : (u16)Delay;
		delay_ms(chunk);
		Delay -= chunk;
	}
}

/* 覆盖 HAL_GetTick()：用 DWT->CYCCNT 合成�?秒�?�数，单调递�?? */
uint32_t HAL_GetTick(void)
{
	static u32 s_last = 0U;
	static u32 s_frac = 0U;		/* 不足 1 ms 的周期余数，�?加避免丢时间 */
	static u32 s_ms   = 0U;
	static u8  s_init = 0U;

	u32 cyc_per_ms;
	u32 now;
	u32 delta;

	delay_dwt_enable();

	cyc_per_ms = SystemCoreClock / 1000U;
	if (cyc_per_ms == 0U)
	{
		return 0U;
	}

	now = DWT->CYCCNT;
	if (!s_init)
	{
		s_last = now;
		s_init = 1U;
		return 0U;
	}

	delta  = now - s_last;		/* u32 减法天然处理 2^32 回绕 */
	s_last = now;

	s_frac += delta;
	s_ms   += s_frac / cyc_per_ms;
	s_frac %= cyc_per_ms;

	return s_ms;
}
