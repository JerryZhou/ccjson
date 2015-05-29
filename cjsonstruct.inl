/**
 * #define __cc_type_begin(type)
 * #define __cc_type_member(type, membertype, member)
 * #define __cc_type_member_array(type, membertype, member)
 * #define __cc_type_end(type)
 * */

// 日期: {有效日期，无效日期}
__cc_type_begin(config_date)
    __cc_type_member(config_date, ccstring, invaliddate) 
    __cc_type_member(config_date, ccstring, validdate) 
__cc_type_end(config_date)

// 账号数据： {账号类型，名字，生效状态}
__cc_type_begin(config_account)
    __cc_type_member(config_account, ccint, accounttype)
    __cc_type_member(config_account, ccstring, name)
    __cc_type_member(config_account, ccint, state)
__cc_type_end(config_account)

// 登陆配置：{账号类型列表}
__cc_type_begin(config_login)
    __cc_type_member_array(config_login, config_account, accounttypes)
__cc_type_end(config_login)

// 闪屏配置项目: {闪屏的候选图片列表，闪屏跳转URL，闪屏停留的秒数，闪屏生效日期}
__cc_type_begin(config_splashact)
    __cc_type_member_array(config_splashact, ccstring, imgs)
    __cc_type_member(config_splashact, ccstring, jump)
    __cc_type_member(config_splashact, ccint, secs)
    __cc_type_member(config_splashact, config_date, date)
__cc_type_end(config_splashact)

// 图片配置项目：{候选图片列表，跳转URL}
__cc_type_begin(config_imgact)
    __cc_type_member_array(config_imgact, ccstring, imgs)
    __cc_type_member(config_imgact, ccstring, jump)
__cc_type_end(config_imgact)

// 登陆配置项目：{登陆介绍图片列表，生效日期}
__cc_type_begin(config_regact)
    __cc_type_member_array(config_regact, config_imgact, imgs)
    __cc_type_member(config_regact, config_date, date)
__cc_type_end(config_regact)

// 系统配置：{邀请者的奖励，被邀请者的奖励}
__cc_type_begin(config_sysact)
    __cc_type_member(config_sysact, ccint, referee_award)
    __cc_type_member(config_sysact, ccint, referer_award)
__cc_type_end(config_sysact)

// 应用配置选项 {登陆，存储，统计，崩溃，闪屏，注册，系统}
__cc_type_begin(config_app)
    __cc_type_member(config_app, config_login, login)
    __cc_type_member(config_app, ccbool, qiniu)
    __cc_type_member(config_app, ccbool, hiido)
    __cc_type_member(config_app, ccbool, ym)
    __cc_type_member(config_app, ccbool, ym_crash)
    __cc_type_member(config_app, ccbool, default_log)
    __cc_type_member(config_app, config_splashact, splash)
    __cc_type_member(config_app, config_regact, reg)
    __cc_type_member(config_app, config_sysact, sys)
__cc_type_end(config_app)
    


