'use strict';
// 内存监控页回归：结构 / 图表绘制 / 状态行防抖 / 面板百分比 / i18n 设置面板 / 实时轮询 / 回放

const { freshPage, countBars } = require('../lib/harness');

module.exports = {
  name: 'mem.html 内存监控页',
  async run({ browser, base, suite }) {
    const { page, errors } = await freshPage(browser, base + '/mem.html', 6000);

    // 1. 无 JS 异常
    suite.ok(errors.length === 0, '页面无 JS 异常' + (errors.length ? '（' + errors.join(' | ') + '）' : ''));

    // 2. 结构：标题、按钮行只有「实时/回放」、无旧的重放按钮
    const struct = await page.evaluate(() => ({
      title: document.title,
      btnRunGone: !document.getElementById('btnRun'),
      rowButtons: Array.from(document.querySelectorAll('.row button')).map((b) => b.textContent.trim()),
      hasSettingsBtn: !!document.getElementById('settingsBtn'),
    }));
    suite.ok(struct.title.indexOf('ESP32') >= 0, '标题含 ESP32');
    suite.ok(struct.btnRunGone, '无「重新回放最近一次」按钮');
    suite.ok(struct.rowButtons.length === 2, '按钮行只有 2 个按钮，实际: ' + struct.rowButtons.join(','));
    suite.ok(struct.hasSettingsBtn, '⚙ 设置按钮存在');

    // 3. 图表已绘制（非透明像素足够多）
    const painted = await page.evaluate(() => {
      const c = document.getElementById('chart');
      if (!c) return 0;
      const ctx = c.getContext('2d');
      const d = ctx.getImageData(0, 0, c.width, c.height).data;
      let n = 0;
      for (let i = 3; i < d.length; i += 4) if (d[i] > 100) n++;
      return n;
    });
    suite.ok(painted > 1000, '主图已绘制（非透明像素 ' + painted + '）');

    // 4. 状态行防抖：单行省略、超长文本不撑高
    const statusStyle = await page.evaluate(() => {
      const el = document.getElementById('status');
      const cs = getComputedStyle(el);
      const before = el.clientHeight;
      el.textContent = '这是一段非常非常长的状态通知文字用于测试窄屏换行是否导致界面抖动抖动抖动抖动抖动抖动';
      const after = el.clientHeight;
      el.textContent = el.title || el.textContent;
      return { ws: cs.whiteSpace, to: cs.textOverflow, ov: cs.overflow, before, after };
    });
    suite.eq(statusStyle.ws, 'nowrap', '状态行 white-space');
    suite.eq(statusStyle.to, 'ellipsis', '状态行 text-overflow');
    suite.eq(statusStyle.ov, 'hidden', '状态行 overflow');
    suite.eq(statusStyle.before, statusStyle.after, '状态行超长文本不改变高度');

    // 5. 按钮行与图表间距 16px
    const gap = await page.evaluate(() => {
      const row = document.querySelector('.row').getBoundingClientRect();
      const chart = document.getElementById('chart').getBoundingClientRect();
      return Math.round(chart.top - row.bottom);
    });
    suite.eq(gap, 16, '按钮行与图表间距');

    // 6. 数值面板：总 DRAM 100%、其余含百分比
    const stats = await page.evaluate(() => {
      const g = (id) => (document.getElementById(id) || {}).textContent || '';
      return {
        vDram: g('vDram'), vTotalUsed: g('vTotalUsed'), vFree: g('vFree'),
        vMin: g('vMin'), vUsed: g('vUsed'), vMax: g('vMax'),
      };
    });
    suite.ok(stats.vDram.indexOf('335,881') >= 0 && stats.vDram.indexOf('100.0%') >= 0, '总 DRAM 显示');
    for (const [k, v] of Object.entries(stats)) {
      if (k === 'vDram' || k === 'vMax') continue;
      suite.ok(v.indexOf('%') >= 0, k + ' 含百分比');
    }

    // 7. 实时轮询：列数 >0 且随时间增长
    const c0 = await countBars(page);
    await new Promise((r) => setTimeout(r, 1200));
    const c1 = await countBars(page);
    suite.ok(c0 > 0, '实时模式已有列（' + c0 + '）');
    suite.ok(c1 > c0, '实时列随时间增长（' + c0 + '→' + c1 + '）');

    // 8. i18n：⚙ 打开设置面板 → 切中文 → 刷新保持 → 切英文
    await page.click('#settingsBtn');
    await new Promise((r) => setTimeout(r, 200));
    const panelOpened = await page.evaluate(() => {
      const el = document.getElementById('settings');
      return el ? !el.classList.contains('hidden') : false;
    });
    suite.ok(panelOpened, '⚙ 打开设置面板');
    await page.select('#setLang', 'zh');
    await new Promise((r) => setTimeout(r, 300));
    const zh = await page.evaluate(() => ({
      title: document.title,
      gDram: (document.querySelector('.stat .k') || {}).textContent || '',
      ls: localStorage.getItem('esp32-chat-lang'),
    }));
    suite.eq(zh.title, 'ESP32 内存监控', '切中文后标题');
    suite.eq(zh.gDram, '总 DRAM', '切中文后面板标签');
    suite.eq(zh.ls, 'zh', '切中文后 localStorage');
    await page.reload({ waitUntil: 'domcontentloaded' });
    await new Promise((r) => setTimeout(r, 4000));
    suite.eq(await page.title(), 'ESP32 内存监控', '刷新后保持中文');
    await page.click('#settingsBtn');
    await new Promise((r) => setTimeout(r, 200));
    await page.select('#setLang', 'en');
    await new Promise((r) => setTimeout(r, 300));
    suite.eq(await page.title(), 'ESP32 Memory Monitor', '切回英文标题');
    // 关闭设置面板（遮罩会挡住页面按钮的点击）
    await page.click('#setClose');
    await new Promise((r) => setTimeout(r, 200));
    suite.ok(await page.evaluate(() => document.getElementById('settings').classList.contains('hidden')), '设置面板已关闭');

    // 9. 实时/回放竞态：点回放后状态不能闪回实时文案
    const liveRe = /每 0\.5s 采样|sampling every 0\.5s/;
    await page.click('#btnReplay');
    let raced = false;
    for (let i = 0; i < 10; i++) {
      await new Promise((r) => setTimeout(r, 100));
      const s = await page.evaluate(() => document.getElementById('status').textContent);
      if (liveRe.test(s)) { raced = true; break; }
    }
    suite.ok(!raced, '切回放后状态不闪回实时文案（竞态）');

    // 10. 回放流程：有数据则播放完成，无数据则显示空态提示
    let done = '';
    for (let i = 0; i < 40; i++) {
      await new Promise((r) => setTimeout(r, 250));
      done = await page.evaluate(() => document.getElementById('status').textContent);
      if (/回放完成|Replay done|还没有对话数据|No chat data yet/.test(done)) break;
    }
    if (/回放完成|Replay done/.test(done)) {
      const rows = await page.evaluate(() => document.querySelectorAll('#samples tr').length);
      suite.ok(rows >= 2, '回放采样表格有数据（' + rows + ' 行）');
    } else if (/还没有对话数据|No chat data yet/.test(done)) {
      suite.skip('回放播放完成', '板子当前无对话采样数据');
    } else {
      suite.ok(false, '回放状态异常: ' + done);
    }

    // 11. 点实时恢复轮询
    await page.click('#btnLive');
    await new Promise((r) => setTimeout(r, 1200));
    const live = await page.evaluate(() => ({
      status: document.getElementById('status').textContent,
      on: document.getElementById('btnLive').classList.contains('on'),
    }));
    suite.ok(live.on, '点实时后按钮高亮');
    suite.ok(/实时|Live/.test(live.status), '点实时后状态为实时文案');
    const back = await countBars(page);
    suite.ok(back > 0, '恢复实时后图仍有列');

    await page.close();
  },
};
