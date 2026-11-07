'use strict';
// 前端回归测试公共工具：浏览器发现 / 断言汇总 / 页面加载 / 可达性探测

const fs = require('fs');
const os = require('os');
const path = require('path');
const puppeteer = require('puppeteer-core');

// 找本机 Chromium：CHROME_PATH 优先，其次 playwright 缓存（新版 / 旧版目录名），最后系统浏览器
function findChrome() {
  const env = process.env.CHROME_PATH;
  if (env && fs.existsSync(env)) return env;
  const candidates = [];
  const cache = path.join(os.homedir(), '.cache', 'ms-playwright');
  if (fs.existsSync(cache)) {
    for (const d of fs.readdirSync(cache)) {
      if (d.startsWith('chromium-')) {
        candidates.push(path.join(cache, d, 'chrome-linux64', 'chrome'));
        candidates.push(path.join(cache, d, 'chrome-linux', 'chrome'));
      }
    }
  }
  candidates.push('/usr/bin/chromium', '/usr/bin/chromium-browser', '/usr/bin/google-chrome');
  return candidates.find((p) => fs.existsSync(p)) || null;
}

// 断言收集器：eq / ok / in，失败累积，最后统一汇总
function createSuite() {
  const fails = [];
  const skips = [];
  const suite = {
    asserts: 0,
    eq(actual, expected, label) {
      suite.asserts++;
      if (actual !== expected) {
        fails.push(`${label}: 期望 ${JSON.stringify(expected)}，实际 ${JSON.stringify(actual)}`);
      }
    },
    ok(cond, label) {
      suite.asserts++;
      if (!cond) fails.push(`${label}: 期望为真，实际为假`);
    },
    in(actual, list, label) {
      suite.asserts++;
      if (!list.includes(actual)) {
        fails.push(`${label}: 期望在 ${JSON.stringify(list)} 中，实际 ${JSON.stringify(actual)}`);
      }
    },
    skip(label, reason) {
      skips.push(`${label}${reason ? '：' + reason : ''}`);
    },
    result() {
      return { fails, skips, asserts: suite.asserts };
    },
  };
  return suite;
}

async function launchBrowser(chromePath) {
  return puppeteer.launch({
    executablePath: chromePath,
    headless: 'new',
    args: ['--no-sandbox', '--disable-gpu', '--disable-dev-shm-usage'],
  });
}

// 打开页面：监听 JS 异常（pageerror）；goto 后等待首屏渲染
async function newPage(browser, url, waitMs = 2000) {
  const page = await browser.newPage();
  await page.setViewport({ width: 900, height: 900 });
  const errors = [];
  page.on('pageerror', (e) => errors.push('pageerror: ' + e.message));
  await page.goto(url, { waitUntil: 'domcontentloaded', timeout: 30000 });
  await new Promise((r) => setTimeout(r, waitMs));
  return { page, errors };
}

// 干净页面：清 localStorage 后刷新，避免上次测试残留的语言/主题干扰
async function freshPage(browser, url, waitMs = 2000) {
  const { page, errors } = await newPage(browser, url, 500);
  await page.evaluate(() => localStorage.clear());
  await page.reload({ waitUntil: 'domcontentloaded' });
  await new Promise((r) => setTimeout(r, waitMs));
  return { page, errors };
}

// 探测板子是否可达（/api/config 走 WebServer 主端口）
async function reachable(url, timeoutMs = 4000) {
  try {
    const ctrl = new AbortController();
    const timer = setTimeout(() => ctrl.abort(), timeoutMs);
    const r = await fetch(url, { signal: ctrl.signal });
    clearTimeout(timer);
    return r.ok;
  } catch {
    return false;
  }
}

// 数内存页主图底部绿条（每列一条 2px）的连续段数 ≈ 已画列数
function countBars(page) {
  return page.evaluate(() => {
    const c = document.getElementById('chart');
    if (!c) return -1;
    const ctx = c.getContext('2d');
    const W = c.width;
    const row = ctx.getImageData(0, c.height - 2, W, 1).data;
    let cols = 0, inside = false;
    for (let x = 0; x < W; x++) {
      const a = row[x * 4 + 3];
      const r = row[x * 4];
      const green = a > 100 && r > 40 && r < 120;
      if (green && !inside) { cols++; inside = true; }
      else if (!green) inside = false;
    }
    return cols;
  });
}

module.exports = { findChrome, createSuite, launchBrowser, newPage, freshPage, reachable, countBars };
