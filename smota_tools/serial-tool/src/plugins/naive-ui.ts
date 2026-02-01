import { type Plugin } from "vue";
import {
  create,
  // component tree
  NConfigProvider,
  NMessageProvider,
  NNotificationProvider,
  NDialogProvider,
} from "naive-ui";

// 全局配置
const globalConfig = {
  themeOverrides: {
    common: {
      primaryColor: "#18a058",
    },
  },
};

export const naiveUiPlugin: Plugin = {
  install(app) {
    const naive = create({
      components: [
        NConfigProvider,
        NMessageProvider,
        NNotificationProvider,
        NDialogProvider,
      ],
      ...globalConfig,
    });
    app.use(naive);
  },
};
