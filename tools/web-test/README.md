# 前端回归测试（tools/web-test）

把之前一次性跑的 headless 验证固化进仓库：**改页面后先跑一遍，防回归**。
用 puppeteer-core 驱动本机 Chromium，实机访问板子页面（不走 mock）。

## 依赖

- Node.js ≥ 20
- Chromium（自动查找 `~/.cache/ms-playwright/chromium-*/`；也可用 `CHROME_PATH` 指定）
- 板子在线（默认 `192.168.1.50`，可用 `BASE_URL` 覆盖）

## 用法

```bash
cd tools/web-test
./run.sh                     # 板子在线时跑全部用例
CHROME_PATH=/path/to/chrome ./run.sh
BASE_URL=http://192.168.1.50 ./run.sh   # 默认值，显式也行
SKIP_IF_DOWN=0 ./run.sh      # 板子不在线时按失败退出（默认跳过并返回 0）
```

退出码：`0` 通过；`1` 有断言失败；`2` 环境问题（无 node / 无 Chromium / 板子不可达且不允许跳过）。

## 覆盖范围

**index.html 主聊天页**（`tests/chat-page.test.js`）：
- 页面无 JS 异常；输入框 / 发送按钮 / 设置按钮 / 侧栏存在
- 设置面板：打开、关闭；语言下拉即时生效（中文/英文 + localStorage 持久化）
- 主题下拉：暗色/浅色切换（`data-theme` + localStorage），浅/暗背景色不同

**mem.html 内存监控页**（`tests/mem-page.test.js`）：
- 页面无 JS 异常；标题含 ESP32；无「重新回放最近一次」按钮；按钮行只有实时/回放
- 主图已绘制（像素级验证）；实时列数 >0 且随时间增长
- 状态行单行省略（nowrap + ellipsis）、超长文本不改变高度（防抖动回归）
- 按钮行与图表间距 16px（平板叠字回归）
- 数值面板：总 DRAM 335,881 (100.0%)、其余各项含百分比
- 设置面板：⚙ 打开、语言切中文即时生效、刷新保持、切回英文
- 实时/回放切换竞态：点回放后状态不闪回实时文案
- 回放流程：有采样数据时播放完成且表格有数据；无数据时显示空态提示（自动跳过播放断言）

## 说明

- 每个用例从干净 localStorage 开始（`freshPage` 先清空再刷新），避免测试间相互干扰。
- 语言默认值取决于浏览器语言（headless Chromium 为 en），断言按实际语言分支判断。
- 测试是实机验证，会触发页面的 `/api/mem` 轮询（0.5s 间隔）；聊天页不发送真实聊天请求。
- 以后新增页面/修复点，按同样格式加 `tests/*.test.js` 即可，runner 自动收集。
