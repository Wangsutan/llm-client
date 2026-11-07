// HTML → PDF（A4，页眉页脚带页码）。
// 依赖 puppeteer-core（不下载浏览器）+ 一个 Chromium；可用环境变量覆盖：
//   PUPPETEER_MODULE  puppeteer-core 入口 js（默认从当前目录 require.resolve）
//   CHROME_PATH       Chromium 可执行文件（默认在 ~/.cache/ms-playwright/ 下找）
import { createRequire } from "node:module";
import { existsSync, readdirSync } from "node:fs";
import { join } from "node:path";
import { homedir } from "node:os";

const require = createRequire(import.meta.url);

function findChrome() {
  const root = join(homedir(), ".cache", "ms-playwright");
  if (existsSync(root)) {
    for (const d of readdirSync(root).sort().reverse()) {
      if (!d.startsWith("chromium-")) continue;
      const p = join(root, d, "chrome-linux64", "chrome");
      if (existsSync(p)) return p;
    }
  }
  return null;
}

const MODULE = process.env.PUPPETEER_MODULE || require.resolve("puppeteer-core");
const CHROME = process.env.CHROME_PATH || findChrome();
if (!CHROME) {
  console.error("错误：找不到 Chromium，请用 CHROME_PATH 指定可执行文件");
  process.exit(2);
}

const { default: puppeteer } = await import(MODULE);

const html = process.argv[2];
const out = process.argv[3];
if (!html || !out) {
  console.error("用法: node render.mjs <input.html> <output.pdf>");
  process.exit(2);
}

const browser = await puppeteer.launch({
  executablePath: CHROME,
  headless: "new",
  args: ["--no-sandbox", "--disable-gpu", "--font-render-hinting=none"],
});
const page = await browser.newPage();
await page.setViewport({ width: 794, height: 1123, deviceScaleFactor: 2 }); // A4 @96dpi
const errs = [];
page.on("pageerror", (e) => errs.push("pageerror: " + e.message));
await page.goto("file://" + html, { waitUntil: "load" });
await new Promise((r) => setTimeout(r, 500));

await page.pdf({
  path: out,
  format: "A4",
  printBackground: true,
  displayHeaderFooter: true,
  headerTemplate:
    '<div style="font-family:DejaVu Sans Mono;font-size:7pt;color:#888;width:100%;padding:0 14mm;">llm-client · C++ 源码</div>',
  footerTemplate:
    '<div style="font-family:DejaVu Sans Mono;font-size:7pt;color:#888;width:100%;padding:0 14mm;text-align:right;">第 <span class="pageNumber"></span> / <span class="totalPages"></span> 页</div>',
  margin: { top: "16mm", bottom: "14mm", left: "14mm", right: "14mm" },
});

console.log("pdf written", out, "errors:", errs.length ? errs : "none");
await browser.close();
