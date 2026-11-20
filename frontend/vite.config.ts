import { defineConfig } from "vite";
import vue from "@vitejs/plugin-vue";
export default defineConfig({
  define: { global: "globalThis" },
  plugins: [vue()],
  server: { proxy: { "/api": "http://127.0.0.1:8000" } },
  build: { outDir: "../src/cobo_viewer/static", emptyOutDir: true },
});
