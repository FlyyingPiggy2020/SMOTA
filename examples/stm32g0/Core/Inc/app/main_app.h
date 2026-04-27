/*
 * Copyright (c) 2025 by Lu Xianfan.
 * @FilePath     : app/main_app.h
 * @Author       : lxf
 * @Date         : 2026-04-27 10:00:00
 * @LastEditors  : lxf_zjnb@qq.com
 * @LastEditTime : 2026-04-27 10:00:00
 * @Brief        : app层主循环任务声明
 */

#ifndef MAIN_APP_H
#define MAIN_APP_H

#ifdef __cplusplus
extern "C" {
#endif

/*---------- includes ----------*/

/*---------- macro ----------*/

/*---------- type define ----------*/

/*---------- variable prototype ----------*/

/*---------- function prototype ----------*/
/**
 * @brief  Initialize app tasks.
 */
void main_app_init(void);

/**
 * @brief  Poll app tasks.
 */
void main_app_poll(void);

/*---------- end of file ----------*/

#ifdef __cplusplus
}
#endif

#endif /* MAIN_APP_H */
