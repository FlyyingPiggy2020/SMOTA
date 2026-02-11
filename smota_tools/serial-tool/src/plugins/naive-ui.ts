import { type Plugin } from "vue";
import {
  create,
  // 核心环境组件
  NConfigProvider,
  NLayout,
  NLayoutHeader,
  NLayoutSider,
  NLayoutContent,
  NDivider,
  NSpace,
  // 基础组件
  NButton,
  NCard,
  NSelect,
  NInput,
  NInputNumber,
  NSwitch,
  NGrid,
  NGi, // 建议用 NGi，它是 NGridItem 的别名，更常用
  NInputGroup,
  NLog, // 串口助手必备
  // 折叠面板
  NCollapse,
  NCollapseItem,
  // 滚动条
  NScrollbar,
} from "naive-ui";

// 1. 这里的配置会影响全局 Naive UI 组件的外观
const themeOverrides = {
  //TODO:设置字体
  common: {
    fontFamily: '"Inter", "Helvetica Neue", sans-serif',
    fontFamilyMono: '"Fira Code", monospace',
    primaryColor: '#2f27ce',
    primaryColorHover: '#443dff',
    primaryColorPressed: '#1f1991',
    borderRadius: '12px',

  },
  Button: {
    borderRadiusMedium: '24px',
  },
  Input: {
    borderRadius: '24px',
  },
}

export const naiveUiPlugin: Plugin = {
  install(app) {
    const naive = create({
      components: [
        NConfigProvider,
        NLayout,
        NLayoutHeader,
        NLayoutSider,
        NLayoutContent,
        NDivider,
        NSpace,
        NButton,
        NCard,
        NSelect,
        NInput,
        NInputNumber,
        NSwitch,
        NGrid,
        NGi,
        NInputGroup,
        NLog,
        NCollapse,
        NCollapseItem,
        NScrollbar,
      ],
      // 注意：这里我们直接传样式配置
      // 如果你想在 App.vue 里动态改，也可以在 App.vue 的 <n-config-provider> 传
    });
    app.use(naive);
  },
};

// 为了方便你在 App.vue 里引用这个配置，我们可以把它也导出
export { themeOverrides };