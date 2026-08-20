import path from "node:path";
import react from "@vitejs/plugin-react";
import { defineConfig } from "vite";

// GitHub Pages 使用根目录构建产物；开发时仍从 client/ 读取前端源码。
export default defineConfig({
  plugins: [react()],
  resolve: {
    alias: {
      "@": path.resolve(import.meta.dirname, "client", "src"),
    },
  },
  root: path.resolve(import.meta.dirname, "client"),
  build: {
    outDir: path.resolve(import.meta.dirname, "dist/public"),
    emptyOutDir: true,
  },
  server: {
    host: true,
  },
});
