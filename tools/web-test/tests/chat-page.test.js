'use strict';
// 主聊天页回归：结构 / 无 JS 异常 / 设置面板（语言、主题）即时生效

const { freshPage } = require('../lib/harness');

module.exports = {
  name: 'index.html 主聊天页',
  async run({ browser, base, suite }) {
    const { page, errors } = await freshPage(browser, base + '/');

    // 1. 无 JS 异常
    suite.ok(errors.length === 0, '页面无 JS 异常' + (errors.length ? '（' + errors.join(' | ') + '）' : ''));

    // 2. 基本结构
    const struct = await page.evaluate(() => ({
      hasQ: !!document.getElementById('q'),
      hasSend: !!document.getElementById('send'),
      hasSettingsBtn: !!document.getElementById('settingsBtn'),
      hasSidebar: !!document.getElementById('sidebar'),
      title: document.title,
    }));
    suite.ok(struct.hasQ, '#q 输入框存在');
    suite.ok(struct.hasSend, '#send 发送按钮存在');
    suite.ok(struct.hasSettingsBtn, '#settingsBtn 设置按钮存在');
    suite.ok(struct.hasSidebar, '#sidebar 侧栏存在');

    // 3. 语言切换（走设置面板，即时生效）
    await page.click('#settingsBtn');
    await new Promise((r) => setTimeout(r, 300));
    const panel = await page.evaluate(() => ({
      visible: !document.getElementById('settings').classList.contains('hidden'),
      hasLang: !!document.getElementById('setLang'),
      hasTheme: !!document.getElementById('setTheme'),
      langLabel: (document.querySelector('label[for="setLang"]') || {}).textContent || '',
    }));
    suite.ok(panel.visible, '设置面板打开');
    suite.ok(panel.hasLang, '语言下拉存在');
    suite.ok(panel.hasTheme, '主题下拉存在');
    suite.ok(panel.langLabel.trim().length > 0, '语言 label 有文案');

    await page.select('#setLang', 'zh');
    await new Promise((r) => setTimeout(r, 300));
    const zh = await page.evaluate(() => ({
      title: document.title,
      ls: localStorage.getItem('esp32-chat-lang'),
    }));
    suite.eq(zh.title, 'ESP32 聊天', '切中文后标题');
    suite.eq(zh.ls, 'zh', '切中文后 localStorage');

    await page.select('#setLang', 'en');
    await new Promise((r) => setTimeout(r, 300));
    const en = await page.evaluate(() => ({
      title: document.title,
      ls: localStorage.getItem('esp32-chat-lang'),
    }));
    suite.eq(en.title, 'ESP32 Chat', '切英文后标题');
    suite.eq(en.ls, 'en', '切英文后 localStorage');

    // 4. 主题切换（暗色 / 浅色）
    await page.select('#setTheme', 'dark');
    await new Promise((r) => setTimeout(r, 300));
    const dark = await page.evaluate(() => ({
      theme: document.documentElement.dataset.theme,
      ls: localStorage.getItem('esp32-chat-theme'),
      bg: getComputedStyle(document.body).backgroundColor,
    }));
    suite.eq(dark.theme, 'dark', '切暗色后 data-theme');
    suite.eq(dark.ls, 'dark', '切暗色后 localStorage');

    await page.select('#setTheme', 'light');
    await new Promise((r) => setTimeout(r, 300));
    const light = await page.evaluate(() => ({
      theme: document.documentElement.dataset.theme,
      bg: getComputedStyle(document.body).backgroundColor,
    }));
    suite.eq(light.theme, 'light', '切回浅色后 data-theme');
    suite.ok(light.bg !== dark.bg, '浅色背景与暗色不同');

    // 5. 关闭设置面板（点「取消」）
    await page.click('#setCancel');
    await new Promise((r) => setTimeout(r, 200));
    const closed = await page.evaluate(() =>
      document.getElementById('settings').classList.contains('hidden')
    );
    suite.ok(closed, '设置面板可关闭');

    await page.close();
  },
};
