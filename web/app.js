      const log = document.getElementById('log');
      const q = document.getElementById('q');
      const send = document.getElementById('send');
      const convList = document.getElementById('convList');
      const convTitle = document.getElementById('convTitle');
      const STORE_KEY = 'esp32-chat-state';

      // ---------- 中英文文案：默认跟随浏览器语言，英文兜底 ----------
      const I18N = {
        zh: {
          appTitle: 'ESP32 聊天',
          newSessionBtn: '＋ 新建会话',
          newSession: '新会话',
          closeSidebar: '关闭侧栏',
          sessionList: '会话列表',
          settingsTitle: '设置（后端/模型）',
          send: '发送',
          placeholder: '输入问题，回车发送',
          convCount: '共 {n} 个会话',
          settings: '设置',
          language: '语言',
          theme: '主题',
          themeLight: '浅色',
          themeDark: '深色',
          palette: '配色',
          paletteBlue: '经典蓝',
          paletteGreen: '护眼绿',
          palettePurple: '静谧紫',
          paletteOrange: '暖橙',
          paletteTeal: '青碧',
          backend: '后端',
          ollamaLan: 'Ollama（局域网）',
          deepseekCloud: 'DeepSeek（云）',
          model: '模型',
          cancel: '取消',
          saveApply: '保存并生效',
          readConfigFail: '读取配置失败：',
          saved: '已保存：{name} / {model}（下次请求生效）',
          saveFail: '保存失败：',
          typingAnswer: '正在回答',
          typingCompact: '正在压缩历史',
          historyTooLong: '历史太长，板子内存放不下；请新建会话，或把问题缩短一些再试。',
          errorPrefix: '错误: ',
          emptyAnswer: '（空回答）',
          aborted: '（已中止）',
          requestFail: '请求失败: ',
          copy: '复制',
          copied: '已复制',
          nothingToCopy: '没有可复制的内容',
          copyCode: '已复制代码',
          download: '下载',
          downloading: '已开始下载 code.{ext}',
        },
        en: {
          appTitle: 'ESP32 Chat',
          newSessionBtn: '＋ New chat',
          newSession: 'New chat',
          closeSidebar: 'Close sidebar',
          sessionList: 'Chats',
          settingsTitle: 'Settings (backend/model)',
          send: 'Send',
          placeholder: 'Type a message, press Enter',
          convCount: '{n} chat(s)',
          settings: 'Settings',
          language: 'Language',
          theme: 'Theme',
          themeLight: 'Light',
          themeDark: 'Dark',
          palette: 'Palette',
          paletteBlue: 'Classic Blue',
          paletteGreen: 'Green',
          palettePurple: 'Purple',
          paletteOrange: 'Orange',
          paletteTeal: 'Teal',
          backend: 'Backend',
          ollamaLan: 'Ollama (LAN)',
          deepseekCloud: 'DeepSeek (Cloud)',
          model: 'Model',
          cancel: 'Cancel',
          saveApply: 'Save & apply',
          readConfigFail: 'Failed to read config: ',
          saved: 'Saved: {name} / {model} (takes effect next request)',
          saveFail: 'Save failed: ',
          typingAnswer: 'Thinking',
          typingCompact: 'Compacting history',
          historyTooLong: 'History is too long for the board; start a new chat or shorten your question.',
          errorPrefix: 'Error: ',
          emptyAnswer: '(empty answer)',
          aborted: '(aborted)',
          requestFail: 'Request failed: ',
          copy: 'Copy',
          copied: 'Copied',
          nothingToCopy: 'Nothing to copy',
          copyCode: 'Code copied',
          download: 'Download',
          downloading: 'Downloading code.{ext}',
        },
      };
      const LANG_KEY = 'esp32-chat-lang';
      function pickLang() {
        const saved = localStorage.getItem(LANG_KEY);
        if (saved === 'zh' || saved === 'en') return saved;
        const nl = (navigator.language || 'en').toLowerCase();
        return nl.startsWith('zh') ? 'zh' : 'en';
      }
      let lang = pickLang();
      function t(key) {
        return (I18N[lang] && I18N[lang][key]) || I18N.en[key] || key;
      }
      function applyI18n() {
        document.title = t('appTitle');
        for (const el of document.querySelectorAll('[data-i18n]')) {
          el.textContent = t(el.dataset.i18n);
        }
        for (const el of document.querySelectorAll('[data-i18n-placeholder]')) {
          el.placeholder = t(el.dataset.i18nPlaceholder);
        }
        for (const el of document.querySelectorAll('[data-i18n-title]')) {
          el.title = t(el.dataset.i18nTitle);
        }
        renderConvList();
      }

      // ---------- 移动端键盘：让页面高度跟随可视区域（visual viewport） ----------
      // iOS：键盘弹出时 100dvh 不变、visualViewport.height 变小 → 用 --app-h 收缩布局；
      // Android：viewport 的 interactive-widget=resizes-content 让 layout viewport 缩小，
      // 这里同样按较小值兜底。
      function fitViewport() {
        const vv = window.visualViewport;
        const h = Math.min(window.innerHeight, vv ? vv.height : window.innerHeight);
        if (h > 0) document.documentElement.style.setProperty('--app-h', h + 'px');
      }
      if (window.visualViewport) {
        window.visualViewport.addEventListener('resize', fitViewport);
        window.visualViewport.addEventListener('scroll', fitViewport);
      }
      window.addEventListener('resize', fitViewport);
      window.addEventListener('orientationchange', fitViewport);
      fitViewport();

      // ---------- 颜色主题：light / dark（localStorage 持久化） ----------
      const THEME_KEY = 'esp32-chat-theme';
      let theme = localStorage.getItem(THEME_KEY) === 'dark' ? 'dark' : 'light';
      const PALETTE_KEY = 'esp32-chat-palette';
      const PALETTES = ['blue', 'green', 'purple', 'orange', 'teal'];
      let palette = localStorage.getItem(PALETTE_KEY);
      if (!PALETTES.includes(palette)) palette = 'blue';
      function applyTheme() {
        document.documentElement.dataset.theme = theme;
      }
      function applyPalette() {
        document.documentElement.dataset.palette = palette;
        const sel = document.getElementById('setPalette');
        if (sel) sel.disabled = theme === 'dark'; // 暗色主题不受配色影响
      }

      let state = { sessions: [], currentId: null };
      let current = null;
      let llmCfg = { backend: 0, model: '' };

      function saveState() {
        try {
          localStorage.setItem(STORE_KEY, JSON.stringify(state));
        } catch (e) {}
      }

      function makeSession() {
        return {
          id: String(Date.now()) + Math.random().toString(36).slice(2, 6),
          title: '',
          updatedAt: Date.now(),
          messages: [],
          summary: '',
        };
      }

      function loadState() {
        try {
          const s = JSON.parse(localStorage.getItem(STORE_KEY) || 'null');
          if (s && Array.isArray(s.sessions) && s.sessions.length) state = s;
        } catch (e) {}
        if (!state.sessions.length) {
          const c = makeSession();
          state.sessions.push(c);
          state.currentId = c.id;
          current = c;
        } else {
          current =
            state.sessions.find((x) => x.id === state.currentId) ||
            state.sessions[0];
          state.currentId = current.id;
        }
        saveState();
        renderConvList();
        renderCurrent();
      }

      function newSession(open) {
        const c = makeSession();
        state.sessions.unshift(c);
        if (open !== false) {
          state.currentId = c.id;
          current = c;
        }
        saveState();
        renderConvList();
        if (open !== false) renderCurrent();
      }

      function openSession(id) {
        const c = state.sessions.find((x) => x.id === id);
        if (!c) return;
        current = c;
        state.currentId = id;
        saveState();
        renderConvList();
        renderCurrent();
        closeSidebar();
      }

      function deleteSession(id, ev) {
        if (ev) ev.stopPropagation();
        const idx = state.sessions.findIndex((x) => x.id === id);
        if (idx < 0) return;
        state.sessions.splice(idx, 1);
        if (state.currentId === id) {
          if (state.sessions.length) {
            current = state.sessions[0];
            state.currentId = current.id;
          } else {
            const c = makeSession();
            state.sessions.push(c);
            state.currentId = c.id;
            current = c;
          }
        }
        saveState();
        renderConvList();
        renderCurrent();
      }

      function renderConvList() {
        convList.innerHTML = '';
        for (const c of state.sessions) {
          const el = document.createElement('div');
          el.className = 'conv' + (c.id === state.currentId ? ' active' : '');
          const titleEl = document.createElement('span');
          titleEl.className = 'conv-title';
          titleEl.textContent = c.title || t('newSession');
          const del = document.createElement('button');
          del.className = 'del';
          del.textContent = '×';
          del.onclick = (e) => deleteSession(c.id, e);
          el.appendChild(titleEl);
          el.appendChild(del);
          el.onclick = () => openSession(c.id);
          convList.appendChild(el);
        }
        document.getElementById('sideFoot').textContent = t('convCount').replace(
          '{n}',
          state.sessions.length,
        );
      }

      function renderCurrent() {
        log.innerHTML = '';
        convTitle.textContent = current && current.title ? current.title : t('newSession');
        if (!current) return;
        for (const m of current.messages) {
          if (!m.text) continue;
          const { msg } = add(m.role, m.text);
          if (m.role === 'bot') renderMarkdown(msg, m.text);
        }
        log.scrollTop = log.scrollHeight;
      }

      function updateTitle(conv, text) {
        if (!conv.title) {
          conv.title = text.slice(0, 20);
          if (conv === current) convTitle.textContent = conv.title;
          renderConvList();
        }
      }

      function toggleSidebar() {
        document.getElementById('sidebar').classList.toggle('open');
        document.getElementById('backdrop').classList.toggle('show');
      }

      function closeSidebar() {
        document.getElementById('sidebar').classList.remove('open');
        document.getElementById('backdrop').classList.remove('show');
      }


      // 显示"正在回答/正在压缩历史"的波浪三点动画
      function showTyping(el, label) {
        el.innerHTML = '';
        if (label) {
          const l = document.createElement('span');
          l.className = 'typing-label';
          l.textContent = label;
          el.appendChild(l);
        }
        const dots = document.createElement('span');
        dots.className = 'typing-dots';
        for (let i = 0; i < 3; i++) {
          dots.appendChild(document.createElement('span'));
        }
        el.appendChild(dots);
      }

      function add(role, text) {
        const row = document.createElement('div');
        row.className = 'row ' + role;
        const av = document.createElement('div');
        av.className = 'avatar';
        av.textContent = role === 'bot' ? '🤖' : role === 'err' ? '⚠️' : '👤';
        // 气泡与操作栏放进一列：操作栏在气泡下方（仿 llm-web 的 DeepSeek 风格）
        const wrap = document.createElement('div');
        wrap.className = 'msg-wrap';
        const msg = document.createElement('div');
        msg.className = 'msg';
        msg.textContent = text || '';
        if (role === 'bot' || role === 'me') {
          msg.dataset.raw = text || '';
        }
        wrap.appendChild(msg);
        if (role === 'bot' || role === 'me') wrap.appendChild(buildMsgActions(msg));
        row.appendChild(av);
        row.appendChild(wrap);
        log.appendChild(row);
        log.scrollTop = log.scrollHeight;
        return { row, msg };
      }

      // ---------- AI 消息操作栏：复制 Markdown 源数据（参考 llm-web） ----------
      const ICON_COPY =
        '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><rect x="9" y="9" width="13" height="13" rx="2"></rect><path d="M5 15H4a2 2 0 0 1-2-2V4a2 2 0 0 1 2-2h9a2 2 0 0 1 2 2v1"></path></svg>';

      function buildMsgActions(msg) {
        const bar = document.createElement('div');
        bar.className = 'msg-actions';
        const btn = document.createElement('button');
        btn.className = 'act-btn';
        btn.type = 'button';
        // 防止点击时把焦点从输入框抢走（移动端软键盘会收起/抖动）
        btn.addEventListener('mousedown', (e) => e.preventDefault());
        btn.innerHTML = ICON_COPY + '<span>' + t('copy') + '</span>';
        btn.onclick = () => {
          const raw = msg.dataset.raw || '';
          if (!raw.trim()) {
            toast(t('nothingToCopy'));
            return;
          }
          copyToClipboard(raw, () => {
            flash(btn, t('copied'));
            toast(t('copied'));
          });
        };
        bar.appendChild(btn);
        return bar;
      }

      function copyToClipboard(txt, done) {
        if (navigator.clipboard && navigator.clipboard.writeText && window.isSecureContext) {
          navigator.clipboard.writeText(txt).then(done).catch(() => fallbackCopy(txt, done));
        } else {
          fallbackCopy(txt, done);
        }
      }

      function fallbackCopy(txt, done) {
        // 不移动焦点、不唤起软键盘（textarea.select() 在移动端会弹键盘导致抖动）：
        // 用 Range 选中屏幕外的隐藏元素再复制；个别浏览器不支持时退回只读 textarea。
        const el = document.createElement('div');
        el.textContent = txt;
        el.style.position = 'fixed';
        el.style.left = '-9999px';
        el.style.top = '0';
        document.body.appendChild(el);
        let ok = false;
        try {
          const sel = window.getSelection();
          const range = document.createRange();
          range.selectNodeContents(el);
          sel.removeAllRanges();
          sel.addRange(range);
          ok = document.execCommand('copy');
          sel.removeAllRanges();
        } catch (e) {}
        document.body.removeChild(el);
        if (ok) {
          done();
          return;
        }
        const ta = document.createElement('textarea');
        ta.value = txt;
        ta.setAttribute('readonly', '');
        ta.style.position = 'fixed';
        ta.style.left = '-9999px';
        ta.style.top = '0';
        document.body.appendChild(ta);
        ta.select();
        try {
          document.execCommand('copy');
        } catch (e) {}
        document.body.removeChild(ta);
        done();
      }

      function flash(btn, txt) {
        const sp = btn.querySelector('span');
        const old = sp.textContent;
        sp.textContent = txt;
        btn.classList.add('copied');
        setTimeout(() => {
          sp.textContent = old;
          btn.classList.remove('copied');
        }, 1400);
      }

      let toastTimer = null;
      function toast(msg) {
        let el = document.getElementById('toast');
        if (!el) {
          el = document.createElement('div');
          el.id = 'toast';
          document.body.appendChild(el);
        }
        el.textContent = msg;
        el.classList.add('show');
        if (toastTimer) clearTimeout(toastTimer);
        toastTimer = setTimeout(() => el.classList.remove('show'), 1600);
      }

      // 基础 XSS 清理：去掉脚本/内联事件/javascript: 链接
      function sanitize(html) {
        return html
          .replace(/<script[\s\S]*?<\/script>/gi, '')
          .replace(/<iframe[\s\S]*?<\/iframe>/gi, '')
          .replace(/\son\w+\s*=\s*(".*?"|'.*?'|[^\s>]+)/gi, '')
          .replace(/javascript:/gi, '');
      }

      // ---------- Markdown 代码块：复制 / 下载（参考 llm-web 的 .code-block） ----------
      const LANG_EXT = {
        python: 'py',
        py: 'py',
        javascript: 'js',
        js: 'js',
        node: 'js',
        typescript: 'ts',
        ts: 'ts',
        c: 'c',
        cpp: 'cpp',
        'c++': 'cpp',
        cxx: 'cpp',
        cs: 'cs',
        csharp: 'cs',
        h: 'h',
        html: 'html',
        css: 'css',
        scss: 'scss',
        less: 'less',
        json: 'json',
        jsonc: 'jsonc',
        xml: 'xml',
        yaml: 'yml',
        yml: 'yml',
        toml: 'toml',
        ini: 'ini',
        bash: 'sh',
        shell: 'sh',
        sh: 'sh',
        zsh: 'sh',
        console: 'sh',
        powershell: 'ps1',
        ps1: 'ps1',
        bat: 'bat',
        batch: 'bat',
        cmd: 'bat',
        go: 'go',
        rust: 'rs',
        java: 'java',
        kotlin: 'kt',
        kt: 'kt',
        scala: 'scala',
        groovy: 'groovy',
        sql: 'sql',
        markdown: 'md',
        md: 'md',
        php: 'php',
        ruby: 'rb',
        rb: 'rb',
        lua: 'lua',
        perl: 'pl',
        swift: 'swift',
        dart: 'dart',
        r: 'r',
        matlab: 'm',
        julia: 'jl',
        haskell: 'hs',
        hs: 'hs',
        elixir: 'ex',
        erlang: 'erl',
        clojure: 'clj',
        ocaml: 'ml',
        fortran: 'f90',
        asm: 'asm',
        wasm: 'wat',
        proto: 'proto',
        protobuf: 'proto',
        graphql: 'graphql',
        diff: 'diff',
        patch: 'patch',
        docker: 'Dockerfile',
        dockerfile: 'Dockerfile',
        makefile: 'Makefile',
        cmake: 'cmake',
        nginx: 'conf',
        apache: 'conf',
        vue: 'vue',
        svelte: 'svelte',
        jsx: 'jsx',
        tsx: 'tsx',
        plaintext: 'txt',
        text: 'txt',
      };

      function decorateCodeBlocks(el) {
        for (const pre of el.querySelectorAll('pre')) {
          const code = pre.querySelector('code');
          if (!code) continue;
          if (pre.closest('.code-block')) continue;
          const lang = (code.className.match(/language-([\w+#-]+)/) || [])[1] || 'text';
          // 语法高亮：有语言标记的代码块用 highlight.js 染色（复制/下载读 textContent 不受影响）
          if (window.hljs && /language-/.test(code.className)) {
            try {
              hljs.highlightElement(code);
            } catch (e) {}
          }
          const block = document.createElement('div');
          block.className = 'code-block';
          const head = document.createElement('div');
          head.className = 'code-head';
          const langEl = document.createElement('span');
          langEl.className = 'code-lang';
          langEl.textContent = lang;
          const copyBtn = document.createElement('button');
          copyBtn.className = 'copy-btn';
          copyBtn.type = 'button';
          copyBtn.addEventListener('mousedown', (e) => e.preventDefault());
          copyBtn.textContent = t('copy');
          copyBtn.addEventListener('click', () => {
            copyToClipboard(code.textContent, () => {
              copyBtn.textContent = t('copied');
              setTimeout(() => (copyBtn.textContent = t('copy')), 1200);
              toast(t('copyCode'));
            });
          });
          const dlBtn = document.createElement('button');
          dlBtn.className = 'download-btn';
          dlBtn.type = 'button';
          dlBtn.addEventListener('mousedown', (e) => e.preventDefault());
          dlBtn.textContent = t('download');
          dlBtn.addEventListener('click', () => downloadCode(code.textContent, lang));
          head.appendChild(langEl);
          head.appendChild(copyBtn);
          head.appendChild(dlBtn);
          pre.parentNode.insertBefore(block, pre);
          block.appendChild(head);
          block.appendChild(pre);
        }
      }

      function downloadCode(text, lang) {
        const ext = LANG_EXT[String(lang).toLowerCase()] || 'txt';
        const blob = new Blob([text], { type: 'application/octet-stream' });
        const url = URL.createObjectURL(blob);
        const a = document.createElement('a');
        a.href = url;
        a.download = 'code-' + Date.now() + '.' + ext;
        document.body.appendChild(a);
        a.click();
        document.body.removeChild(a);
        setTimeout(() => URL.revokeObjectURL(url), 1000);
        toast(t('downloading').replace('{ext}', ext));
      }

      // 表格包一层横向滚动容器：窄屏下列多/内容宽时可左右滑动；
      // 每列最小宽度由 CSS 的 th/td min-width 保证。
      function decorateTables(el) {
        for (const table of el.querySelectorAll('table')) {
          if (table.parentElement && table.parentElement.classList.contains('table-wrap')) {
            continue;
          }
          const wrap = document.createElement('div');
          wrap.className = 'table-wrap';
          table.parentNode.insertBefore(wrap, table);
          wrap.appendChild(table);
        }
      }

      function renderMarkdown(el, text) {
        // 保存 Markdown 源：复制按钮读 dataset.raw，保证拿到的是未渲染原文
        el.dataset.raw = text == null ? '' : String(text);
        try {
          el.innerHTML = sanitize(
            marked.parse(text, { breaks: true, gfm: true }),
          );
          el.classList.add('rendered');
          decorateCodeBlocks(el);
          decorateTables(el);
        } catch (e) {
          el.textContent = text;
        }
      }


      // 把消息数组按 user/assistant 配对成"轮次"
      function pairRounds(msgs) {
        const rounds = [];
        let cur = null;
        for (const m of msgs) {
          if (m.role === 'me') {
            cur = { user: m.text || '', bot: '' };
            rounds.push(cur);
          } else if (m.role === 'bot' && cur && !cur.bot) {
            cur.bot = m.text || '';
          }
        }
        return rounds.filter((r) => r.user || r.bot);
      }

      // 进行中的请求：页面被刷新/关闭时要主动掐断，否则板子那条流式连接会半开在那儿
      let currentAbort = null;
      window.addEventListener('pagehide', () => {
        if (currentAbort) currentAbort.abort();
      });

      // 组装上传表单：必须是"带 filename 的文件字段"才会走板子的 upload 回调；
      // 浏览器支持 gzip 时先压缩（板子在 upload 回调里用 payloadCap 判上限）
      async function payloadForm(obj) {
        const json = JSON.stringify(obj);
        const form = new FormData();
        if (typeof CompressionStream !== 'undefined') {
          try {
            const stream = new Blob([json]).stream().pipeThrough(new CompressionStream('gzip'));
            form.append('payload', await new Response(stream).blob(), 'payload.json.gz');
            return form;
          } catch (e) {
            form.delete('payload');
          }
        }
        form.append('payload', new Blob([json], { type: 'application/json' }), 'payload.json');
        return form;
      }

      async function ask() {
        const text = q.value.trim();
        if (!text) return;
        const conv = current;
        if (!conv) return;
        // 上一轮还挂着就先取消；本轮摘要与聊天共用同一个 AbortController，
        // 页面刷新/关闭时由 pagehide 统一掐断（否则板子那条流式连接会半开在那儿）
        if (currentAbort) currentAbort.abort();
        const ctrl = new AbortController();
        currentAbort = ctrl;
        q.value = '';
        const prevMsgs = conv.messages.slice(); // 此前消息快照（不含当前）

        add('me', text);
        conv.messages.push({ role: 'me', text: text });
        updateTitle(conv, text);
        conv.updatedAt = Date.now();
        saveState();
        send.disabled = true;
        const { row: botRow, msg: botMsg } = add('bot', '');
        showTyping(botMsg, t('typingAnswer'));
        conv.messages.push({ role: 'bot', text: '' });
        const botIdx = conv.messages.length - 1;
        saveState();

        // 预算感知历史：按当前模型字符预算，以完整轮次从最近保留；
        // 超预算的早期轮次压缩为摘要（缓存到 conv.summary）
        const budget = llmCfg.history_budget || 12000;
        const rounds = pairRounds(prevMsgs);
        let used = 0;
        const kept = [];
        for (let i = rounds.length - 1; i >= 0; i--) {
          const r = rounds[i];
          const len = r.user.length + r.bot.length;
          if (used + len > budget && kept.length > 0) break;
          used += len;
          kept.unshift(r);
        }
        const dropped = rounds.slice(0, rounds.length - kept.length);
        let summary = conv.summary || '';
        if (dropped.length) {
          showTyping(botMsg, t('typingCompact'));
          const earlyText = dropped
            .map((r) => '用户: ' + r.user + '\n助手: ' + r.bot)
            .join('\n\n');
          const summarizeText =
            (summary ? '【已有摘要】' + summary + '\n\n新增历史:\n' : '') + earlyText;
          try {
            // 和聊天走同一条管线（gzip + 文件字段），板子才能用 payloadCap 兜住
            const r = await fetch('/api/summarize', {
              method: 'POST',
              body: await payloadForm({ text: summarizeText }),
              signal: ctrl.signal,
            });
            if (r.ok) {
              const j = await r.json();
              if (j.summary) {
                summary = j.summary;
                conv.summary = summary;
                saveState();
              }
            }
          } catch (e) {
            // 摘要失败：仅丢失远期上下文，继续
          }
        }
        const hist = [];
        showTyping(botMsg, t('typingAnswer'));
        if (summary) hist.push({ role: 'user', content: '（此前对话摘要）' + summary });
        for (const r of kept) {
          if (r.user) hist.push({ role: 'user', content: r.user });
          if (r.bot) hist.push({ role: 'assistant', content: r.bot });
        }
        let full = '';
        try {
          // 用 Blob（带 filename）上传：只有"文件"字段才走 WebServer 的 upload 回调，
          // 从而避免整个请求体被塞进 _currentArgs（省一份 M 的副本）。
          // 浏览器支持 gzip 时先压缩（减小上传体积），板子在 upload 回调里解压。
          const form = await payloadForm({ message: text, history: hist });
          const resp = await fetch('/api/chat', { method: 'POST', body: form, signal: ctrl.signal });
          if (!resp.ok) {
            let msg = 'HTTP ' + resp.status;
            try {
              const j = await resp.json();
              // 板子内存吃不下这么大的历史：给一句能指导下一步的提示
              msg =
                j.code === 'payload_too_large'
                  ? t('historyTooLong')
                  : j.error || msg;
            } catch (e) {}
            botRow.className = 'row err';
            botMsg.textContent = t('errorPrefix') + msg;
            conv.messages[botIdx] = { role: 'err', text: t('errorPrefix') + msg };
            saveState();
            return;
          }
          const reader = resp.body.getReader();
          const dec = new TextDecoder();
          let buf = '';
          for (;;) {
            const { done, value } = await reader.read();
            if (done) break;
            buf += dec.decode(value, { stream: true });
            let idx;
            while ((idx = buf.indexOf('\n')) >= 0) {
              const line = buf.slice(0, idx);
              buf = buf.slice(idx + 1);
              if (!line.trim()) continue;
              let j;
              try {
                j = JSON.parse(line);
              } catch (e) {
                continue;
              }
              if (j.delta) {
                full += j.delta;
                botMsg.textContent = full;
                botMsg.dataset.raw = full;
                conv.messages[botIdx] = { role: 'bot', text: full };
                conv.updatedAt = Date.now();
                saveState();
                log.scrollTop = log.scrollHeight;
              }
              if (j.error) {
                botRow.className = 'row err';
                botMsg.textContent = t('errorPrefix') + j.error;
                conv.messages[botIdx] = {
                  role: 'err',
                  text: t('errorPrefix') + j.error,
                };
                saveState();
              }
            }
          }
          if (full) {
            renderMarkdown(botMsg, full);
            conv.messages[botIdx] = { role: 'bot', text: full };
          } else if (botMsg.textContent === '…') {
            botMsg.textContent = t('emptyAnswer');
            conv.messages[botIdx] = { role: 'bot', text: t('emptyAnswer') };
          }
          conv.updatedAt = Date.now();
          saveState();
        } catch (e) {
          if (e && e.name === 'AbortError') {
            // 用户刷新/关闭页面，或被下一问顶掉：不算错误，也不弹提示
            botMsg.textContent = full ? full + '\n\n' + t('aborted') : t('aborted');
            botMsg.dataset.raw = botMsg.textContent;
            conv.messages[botIdx] = { role: 'bot', text: botMsg.textContent };
            saveState();
          } else {
            botRow.className = 'row err';
            botMsg.textContent = t('requestFail') + e;
            conv.messages[botIdx] = { role: 'err', text: t('requestFail') + e };
            saveState();
          }
        } finally {
          if (currentAbort === ctrl) currentAbort = null;
          send.disabled = false;
          q.focus();
        }
      }


      // ---------- 设置：后端 / 模型在线切换 ----------
      let settingsModels = null;
      function renderModelOptions(backend, current) {
        const sel = document.getElementById('setModel');
        sel.innerHTML = '';
        const list =
          backend === 1
            ? (settingsModels && settingsModels.deepseek) || ['deepseek-chat', 'deepseek-reasoner']
            : (settingsModels && settingsModels.ollama) || [];
        for (const m of list) {
          const o = document.createElement('option');
          o.value = m;
          o.textContent = m;
          if (m === current) o.selected = true;
          sel.appendChild(o);
        }
        if (!list.length) {
          const o = document.createElement('option');
          o.textContent = '（无可用模型）';
          sel.appendChild(o);
        }
      }

      async function openSettings() {
        const el = document.getElementById('settings');
        el.classList.remove('hidden');
        document.getElementById('setMsg').textContent = '';
        try {
          const cfg = await (await fetch('/api/config', { cache: 'no-store' })).json();
          document.getElementById('setLang').value = lang;
          document.getElementById('setTheme').value = theme;
          document.getElementById('setPalette').value = palette;
          document.getElementById('setBackend').value = String(cfg.backend);
          settingsModels = await (await fetch('/api/models', { cache: 'no-store' })).json();
          renderModelOptions(cfg.backend, cfg.model);
        } catch (e) {
          document.getElementById('setMsg').textContent = t('readConfigFail') + e;
        }
      }

      document.getElementById('settingsBtn').onclick = openSettings;
      document.getElementById('setCancel').onclick = () =>
        document.getElementById('settings').classList.add('hidden');
      document.getElementById('settings').onclick = (e) => {
        if (e.target.id === 'settings') document.getElementById('settings').classList.add('hidden');
      };
      document.getElementById('setLang').onchange = (e) => {
        lang = e.target.value === 'en' ? 'en' : 'zh';
        localStorage.setItem(LANG_KEY, lang);
        applyI18n();
        renderCurrent();
      };
      document.getElementById('setTheme').onchange = (e) => {
        theme = e.target.value === 'dark' ? 'dark' : 'light';
        localStorage.setItem(THEME_KEY, theme);
        applyTheme();
        applyPalette();
      };
      document.getElementById('setPalette').onchange = (e) => {
        palette = PALETTES.includes(e.target.value) ? e.target.value : 'blue';
        localStorage.setItem(PALETTE_KEY, palette);
        applyPalette();
      };
      document.getElementById('setBackend').onchange = () => {
        const b = Number(document.getElementById('setBackend').value);
        renderModelOptions(b, '');
      };
      document.getElementById('setSave').onclick = async () => {
        const backend = Number(document.getElementById('setBackend').value);
        const model = document.getElementById('setModel').value;
        try {
          const r = await fetch('/api/config', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ backend: backend, model: model }),
          });
          const j = await r.json();
          llmCfg = j;
          document.getElementById('setMsg').textContent = t('saved')
            .replace('{name}', j.backend === 1 ? 'DeepSeek' : 'Ollama')
            .replace('{model}', j.model);
        } catch (e) {
          document.getElementById('setMsg').textContent = t('saveFail') + e;
        }
      };

      fetch('/api/config', { cache: 'no-store' })
        .then((r) => r.json())
        .then((j) => {
          llmCfg = j;
        })
        .catch(() => {});

      document.getElementById('newBtn').onclick = () => newSession(true);
      document.getElementById('sideToggle').onclick = toggleSidebar;
      document.getElementById('sideClose').onclick = closeSidebar;
      document.getElementById('backdrop').onclick = closeSidebar;
      send.onclick = ask;
      q.addEventListener('focus', () => {
        // 键盘弹出有动画，稍后把输入框滚进可视区，避免被键盘遮挡
        setTimeout(() => q.scrollIntoView({ block: 'nearest' }), 300);
      });
      q.addEventListener('keydown', (e) => {
        if (e.key === 'Enter') ask();
      });

      applyTheme();
      applyPalette();
      loadState();
      applyI18n();
    