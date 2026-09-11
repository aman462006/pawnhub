import react from '@vitejs/plugin-react'
import { defineConfig } from 'vite'

// The React dev server runs on :5173 and proxies /api to the FastAPI backend on :8000,
// so the frontend can call the engine without any CORS/hostname juggling.
export default defineConfig({
  plugins: [react()],
  server: {
    proxy: {
      '/api': 'http://127.0.0.1:8000',
    },
  },
})
