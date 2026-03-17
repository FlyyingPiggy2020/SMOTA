import { createApp } from "vue";
import "./assets/main.css";
import "vfonts/Lato.css";
import "vfonts/FiraCode.css";
import { createPinia } from 'pinia'
import piniaPluginPersistedstate from 'pinia-plugin-persistedstate'
import { naiveUiPlugin } from "./plugins/naive-ui";
import App from "./App.vue";

const pinia = createPinia();
pinia.use(piniaPluginPersistedstate);

const app = createApp(App);
app.use(pinia);
app.use(naiveUiPlugin);
app.mount("#app");
