import { defineConfig } from "vitepress";

// https://vitepress.dev/reference/site-config
export default defineConfig({
  title: "FoveaCam++",
  description: "Awesome Device!",
  themeConfig: {
    // https://vitepress.dev/reference/default-theme-config
    nav: [
      { text: "Home", link: "/" },
      { text: "Publication", link: "/publication/" },
      { text: "Build Your Own", link: "/build-your-own/" },
    ],
    sidebar: {
      "/build-your-own/": [
        {
          text: "Prerequisites",
          items: [
            { text: "Markdown Examples", link: "/placeholder" },
            { text: "Runtime API Examples", link: "/placeholder" },
          ],
        },
        {
          text: "Building Hardware",
          items: [
            // Items to be purchased & 3D printed
            { text: "Sourcing Parts", link: "/placeholder" },
            // How to assemble the parts
            { text: "Putting Together the Camera", link: "/placeholder" },
          ],
        },
        {
          text: "Preparing Software",
          items: [
            { text: "Markdown Examples", link: "/placeholder" },
            { text: "Runtime API Examples", link: "/placeholder" },
          ],
        },
        {
          text: "Calibration",
          items: [
            { text: "Markdown Examples", link: "/placeholder" },
            { text: "Runtime API Examples", link: "/placeholder" },
          ],
        },
        {
          text: "Field Operation",
          items: [
            { text: "Markdown Examples", link: "/placeholder" },
            { text: "Runtime API Examples", link: "/placeholder" },
          ],
        },
      ],
    },
    socialLinks: [
      { icon: "github", link: "https://github.com/vuejs/vitepress" },
    ],
  },
});
