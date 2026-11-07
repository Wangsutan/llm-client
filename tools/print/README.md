# 打印稿生成（C++ 源码 → 可彩色打印的 PDF）

```bash
# 1. 生成排版用的 HTML（浅色高亮、行号、封面/文件清单/目录树）
python3 tools/print/gen_pdf.py            # 输出 print.html 到当前目录

# 2. HTML → PDF（需要 Chromium + puppeteer-core）
node tools/print/render.mjs print.html out.pdf
# 依赖路径可用环境变量覆盖：PUPPETEER_MODULE / CHROME_PATH
```

- 收录 `ORDER` 里列出的 C++ 文件；**`secrets.h` 因含 Wi-Fi 密码与 API Key 被排除**，封面有说明。
- 当前仓库（1,383 行、18 个文件）实测：A4 / 9 pt / 行高 1.25 → **24 页**（双面 12 张）。

## 踩过的坑（都验证过，别再踩）

1. **pygments 自带的 `linenos`**：行号列只有 9 mm 时，三位数行号 + 内边距在窄栏里会被挤成两行，
   整份从 21 页涨到 34 页。改成自己按行渲染。
2. **`@page { margin: 0 }` 会覆盖** `page.pdf({ margin })` 的边距：正文顶到 y=0、和页眉叠在一起。
   边距只交给 `page.pdf` 的 margin 参数。
3. **封面别用固定高度**（`height: 258mm`）：内容一多就溢出，压住下一页正文。用自然流 +
   `break-after: page`，并给 `break-inside: avoid` 让清单/目录树整块不拆。
4. **行号布局不能用 flex，也不能 `position: absolute`**：Chromium 打印时会把行号块甩到下一页顶部、
   正文留在上一页。现在用「每行一个 block + 12 mm 悬垂缩进 + 行号做成 inline-block」，
   行号与代码同属一行，分页只在行间断开。
