'use strict';
// 前端回归测试 runner：依次执行 tests/*.test.js，汇总断言
// 用法：BASE_URL=http://192.168.1.50 node runner.js
//   环境变量：
//     BASE_URL     板子地址（默认 http://192.168.1.50）
//     CHROME_PATH  Chromium 可执行文件（默认自动查找 playwright 缓存）
//     SKIP_IF_DOWN 设 0 时板子不可达按失败处理（默认跳过并返回 0）

const fs = require('fs');
const path = require('path');
const { findChrome, createSuite, launchBrowser, reachable } = require('./lib/harness');

const BASE = (process.env.BASE_URL || 'http://192.168.1.50').replace(/\/$/, '');
const SKIP_IF_DOWN = process.env.SKIP_IF_DOWN !== '0';

(async () => {
  console.log('== llm-client 前端回归测试 ==');
  console.log('板子:', BASE);

  const chrome = process.env.CHROME_PATH || findChrome();
  if (!chrome) {
    console.error('未找到 Chromium。请设置 CHROME_PATH，或安装 playwright 浏览器缓存。');
    process.exit(2);
  }
  console.log('Chromium:', chrome);

  const up = await reachable(BASE + '/api/config');
  if (!up) {
    if (SKIP_IF_DOWN) {
      console.log('SKIP: 板子不可达（' + BASE + '），全部用例跳过。');
      process.exit(0);
    }
    console.error('板子不可达: ' + BASE);
    process.exit(2);
  }

  const browser = await launchBrowser(chrome);
  const suite = createSuite();
  const testDir = path.join(__dirname, 'tests');
  const files = fs.readdirSync(testDir).filter((f) => f.endsWith('.test.js')).sort();

  for (const f of files) {
    const mod = require(path.join(testDir, f));
    console.log('\n[' + (mod.name || f) + ']');
    try {
      await mod.run({ browser, base: BASE, suite });
    } catch (e) {
      suite.ok(false, f + ' 运行异常: ' + (e && e.message ? e.message : e));
    }
  }
  await browser.close();

  const r = suite.result();
  console.log('\n== 汇总 ==');
  if (r.fails.length) {
    console.log('失败 ' + r.fails.length + ' 项：');
    for (const x of r.fails) console.log('  ✗ ' + x);
  }
  if (r.skips.length) {
    console.log('跳过：');
    for (const x of r.skips) console.log('  - ' + x);
  }
  console.log(`断言 ${r.asserts} 条，失败 ${r.fails.length} 条，跳过 ${r.skips.length} 条`);
  process.exit(r.fails.length ? 1 : 0);
})().catch((e) => {
  console.error('FATAL', e);
  process.exit(2);
});
