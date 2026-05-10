import { defineConfig } from "astro/config";
import starlight from "@astrojs/starlight";

export default defineConfig({
  site: "https://haven-tau-eight.vercel.app",
  integrations: [
    starlight({
      title: "Haven",
      description:
        "Static Partition Hypervisor for Asymmetric Multiprocessing — enforcing spatial and temporal isolation between Linux and RTOS on a heterogeneous SoC.",
      logo: {
        light: "./src/assets/logo-light.svg",
        dark: "./src/assets/logo-dark.svg",
        replacesTitle: false,
      },
      social: {
        github: "https://github.com/NguyenTrongPhuc552003/haven",
      },
      editLink: {
        baseUrl:
          "https://github.com/NguyenTrongPhuc552003/haven/edit/main/website/",
      },
      customCss: ["./src/styles/custom.css"],
      head: [
        {
          tag: "meta",
          attrs: {
            property: "og:image",
            content: "https://haven-tau-eight.vercel.app/og.png",
          },
        },
      ],
      sidebar: [
        {
          label: "Getting Started",
          items: [
            { label: "Introduction", slug: "getting-started/introduction" },
            { label: "Quick Start", slug: "getting-started/quickstart" },
            { label: "Repository Structure", slug: "getting-started/structure" },
          ],
        },
        {
          label: "Architecture",
          items: [
            { label: "Overview", slug: "architecture/overview" },
            { label: "Isolation Model", slug: "architecture/isolation-model" },
            { label: "Stage-2 MMU", slug: "architecture/stage2-mmu" },
            { label: "IRQ Ownership", slug: "architecture/irq-ownership" },
            { label: "SMMU / DMA Isolation", slug: "architecture/smmu-dma" },
            { label: "Budget Scheduler", slug: "architecture/budget-scheduler" },
          ],
        },
        {
          label: "Platform",
          items: [
            { label: "NXP i.MX95 Dev Kit", slug: "platform/imx95" },
            { label: "QEMU arm64", slug: "platform/qemu-arm64" },
            { label: "Porting Guide", slug: "platform/porting" },
          ],
        },
        {
          label: "Thesis",
          items: [
            { label: "Background & Related Work", slug: "thesis/background" },
            { label: "Chapter Traceability", slug: "thesis/traceability" },
            { label: "Evaluation Plan", slug: "thesis/evaluation" },
            { label: "Milestones & Roadmap", slug: "thesis/milestones" },
            { label: "Evidence Templates", slug: "thesis/evidence" },
          ],
        },
        {
          label: "Reference",
          items: [
            { label: "API Reference", slug: "reference/api" },
            { label: "FAQ", slug: "reference/faq" },
            { label: "Changelog", slug: "reference/changelog" },
            { label: "Contributing", slug: "reference/contributing" },
            { label: "Security Policy", slug: "reference/security" },
          ],
        },
      ],
    }),
  ],
});
