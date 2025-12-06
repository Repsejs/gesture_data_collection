#include "timer_config.h"
#include "eclicw.h"

void timer_interrupt_config(void) {
    timer_oc_parameter_struct timer_ocinitpara;
    timer_parameter_struct timer_initpara;

    eclic_irq_enable(TIMER1_IRQn, 1, 0);

    rcu_periph_clock_enable(RCU_TIMER1);

    timer_deinit(TIMER1);

    timer_struct_para_init(&timer_initpara);
    /* Frequency = core clock / ((1+prescaler)*period) = 96MHz / (40 * 48000) = 50Hz */
   
    uint32_t period = SystemCoreClock / (1000 * 2);
    timer_initpara.prescaler         = 39;
    timer_initpara.alignedmode       = TIMER_COUNTER_EDGE;
    timer_initpara.counterdirection  = TIMER_COUNTER_UP;
    timer_initpara.period            = period;
    timer_initpara.clockdivision     = TIMER_CKDIV_DIV1;
    timer_init(TIMER1, &timer_initpara);

    timer_channel_output_struct_para_init(&timer_ocinitpara);

    timer_ocinitpara.outputstate  = TIMER_CCX_ENABLE;
    timer_ocinitpara.ocpolarity   = TIMER_OC_POLARITY_HIGH;
    timer_ocinitpara.ocidlestate  = TIMER_OC_IDLE_STATE_LOW;
    timer_channel_output_config(TIMER1, TIMER_CH_0, &timer_ocinitpara);

    timer_channel_output_pulse_value_config(TIMER1, TIMER_CH_0, SystemCoreClock/4000);
    timer_channel_output_mode_config(TIMER1, TIMER_CH_0, TIMER_OC_MODE_TIMING);
    timer_channel_output_shadow_config(TIMER1, TIMER_CH_0, TIMER_OC_SHADOW_DISABLE);

    timer_interrupt_enable(TIMER1, TIMER_INT_CH0);
    timer_interrupt_flag_clear(TIMER1, TIMER_INT_CH0);

    timer_enable(TIMER1);
}
