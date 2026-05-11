/** @type {import('tailwindcss').Config} */
export default {
  content: ['./src/**/*.{svelte,js,ts}', './index.html'],
  theme: {
    extend: {
      colors: {
        surface: {
          DEFAULT: '#1C1C1E',
          raised: '#2C2C2E',
          overlay: '#3A3A3C',
        },
        accent: {
          DEFAULT: '#6C63FF',
          hover: '#7F78FF',
          muted: '#6C63FF33',
        },
        text: {
          primary: '#F5F5F7',
          secondary: '#A1A1A6',
          tertiary: '#6E6E73',
        },
        heatmap: {
          low: '#2C2C2E',
          mid: '#6C63FF66',
          high: '#6C63FF',
        },
        success: '#30D158',
        danger: '#FF453A',
      },
      fontFamily: {
        sans: ['var(--app-font)', '"JetBrains Mono"', '"Fira Code"', '"Cascadia Code"', '"Consolas"', 'monospace'],
        mono: ['var(--app-font)', '"JetBrains Mono"', '"Fira Code"', '"Cascadia Code"', '"Consolas"', 'monospace'],
      },
      borderRadius: {
        card: '12px',
      },
      boxShadow: {
        card: '0 1px 3px rgba(0,0,0,0.3)',
      },
    },
  },
  plugins: [],
}
