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
      {
        text: 'Build Your Own',
        items: [
          { text: 'Overview', link: '/build-your-own/index' },
          { text: 'Prerequisites', link: '/build-your-own/prereq' },
          { text: 'Sourcing Hardware', link: '/build-your-own/parts' },
          { text: 'Assembly', link: '/build-your-own/together' },
          { text: 'Preparing Software', link: '/build-your-own/install' },
          { text: 'Calibration', link: '/build-your-own/calibrate' },
          { text: 'Field Operation', link: '/build-your-own/field' }
        ]
      }
    ],
    sidebar: {
      "/build-your-own/": [
        {
          text: "Introduction",
          items: [
            { text: "Overview", link: "/build-your-own/index" }
          ],
        },
        {
          text: "Prerequisites",
          items: [
            { text: "Prerequisites Overview", link: "/build-your-own/prereq" }
          ],
        },
        {
          text: "Building Hardware",
          items: [
            // Items to be purchased & 3D printed
            { text: "Sourcing Parts", link: "/build-your-own/parts" },
            // How to assemble the parts
            { text: "Putting Together the Camera", link: "/build-your-own/together" },
          ],
        },
        {
          text: "Preparing Software",
          items: [
            { text: "Installation", link: "/build-your-own/install" }
          ],
        },
        {
          text: "Calibration",
          items: [
            { text: "How to Calibrate", link: "/build-your-own/calibrate" }
          ],
        },
        {
          text: "Operation",
          items: [
            { text: "Field Operation", link: "/build-your-own/field" }
          ],
        },
      ],
    },
    //socialLinks: [
      //{ icon: "github", link: "" },
    //],
  },
});
