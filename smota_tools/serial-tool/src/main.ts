import { createApp } from "vue";
import "./assets/main.css";
import { naiveUiPlugin } from "./plugins/naive-ui";
import App from "./App.vue";

const app = createApp(App);
app.use(naiveUiPlugin);
app.mount("#app");
