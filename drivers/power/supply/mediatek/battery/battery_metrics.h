#ifndef _BATTERY_METRICS_H
#define _BATTERY_METRICS_H

#if defined(CONFIG_AMAZON_METRICS_LOG)
int bat_metrics_slp_current(u32 ma);
int bat_metrics_aicl(bool is_detected, u32 aicl_result);
int bat_metrics_vbus(bool is_on);
int bat_metrics_chrdet(u32 chr_type);
int bat_metrics_chg_fault(u8 fault_type);
int bat_metrics_chg_state(u32 chg_sts);
int bat_metrics_critical_shutdown(void);
int bat_metrics_top_off_mode(bool is_on, long total_time_plug_in);
int bat_metrics_demo_mode(bool is_on, long total_time_plug_in);
int bat_metrics_suspend(void);
int bat_metrics_resume(void);
int bat_metrics_init(void);
void bat_metrics_uninit(void);
#else
static inline void bat_metrics_slp_current(u32 ma) {}
static inline void bat_metrics_aicl(bool is_detected, u32 aicl_result) {}
static inline void bat_metrics_vbus(bool is_on) {}
static inline void bat_metrics_chrdet(u32 chr_type) {}
static inline void bat_metrics_chg_fault(u8 fault_type) {}
static inline void bat_metrics_chg_state(u32 chg_sts) {}
static inline void bat_metrics_critical_shutdown(void) {}
static inline void bat_metrics_top_off_mode(bool is_on, long total_time_plug_in) {}
static inline void bat_metrics_demo_mode(bool is_on, long total_time_plug_in) {}
static inline void bat_metrics_suspend(void) {}
static inline void bat_metrics_resume(void) {}
static inline void bat_metrics_init(void) {}
static inline void bat_metrics_uninit(void) {}
#endif

#endif /* _BATTERY_METRICS_H */
